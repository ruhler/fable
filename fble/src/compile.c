// compile.c --
//   This file describes code to compile fble values into fble instructions.

#include <assert.h>   // for assert
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "fble-alloc.h"     // for FbleAlloc, etc.
#include "fble-vector.h"    // for FbleVectorInit, etc.

#include "code.h"
#include "tc.h"
#include "typecheck.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// Local --
//   Information about a value available in the stack frame.
//
// index - the index of the value in the current stack frame.
// refcount - the number of references to the local.
typedef struct {
  FbleFrameIndex index;
  size_t refcount;
} Local;

// LocalV --
//   A vector of pointers to locals.
typedef struct {
  size_t size;
  Local** xs;
} LocalV;

// Scope --
//   Scope of variables visible during compilation.
//
// Fields:
//   statics - variables captured from the parent scope.
//     Owns the Locals.
//   vars - stack of local variables in scope order. Entries may be NULL.
//     Owns the Locals.
//   locals - local values. Entries may be NULL to indicate a free slot.
//     Owns the Local.
//   code - the instruction block for this scope.
//   pending_profile_ops - profiling ops to associated with the next
//                         instruction added.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  LocalV statics;
  LocalV vars;
  LocalV locals;
  FbleCode* code;
  FbleProfileOp* pending_profile_ops;
  struct Scope* parent;
} Scope;

static Local* NewLocal(Scope* scope);
static void LocalRelease(Scope* scope, Local* local);
static void PushVar(Scope* scope, Local* local);
static void PopVar(Scope* scope);
static Local* GetVar(Scope* scope, FbleVarIndex index);
static void SetVar(Scope* scope, size_t index, Local* local);

static void InitScope(Scope* scope, FbleCode** code, size_t args, size_t statics, Scope* parent);
static void FreeScope(Scope* scope);
static void AppendInstr(Scope* scope, FbleInstr* instr);
static void AppendProfileOp(Scope* scope, FbleProfileOpTag tag, FbleBlockId block);

// Blocks --
//   A stack of block frames tracking the current block for profiling.
//
// Fields:
//   stack - The stack of black frames representing the current location.
//   profile - The profile to append blocks to.
typedef struct {
  FbleBlockIdV stack;
  FbleProfile* profile;
} Blocks;

static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope);
static void EnterBodyBlock(Blocks* blocks, FbleLoc loc, Scope* scope);
static void ExitBlock(Blocks* blocks, Scope* scope, bool exit);

static void CompileExit(bool exit, Scope* scope, Local* result);
static Local* CompileExpr(Blocks* blocks, bool exit, Scope* scope, FbleTc* tc);
static FbleCode* Compile(size_t argc, FbleTc* tc, FbleName name, FbleProfile* profile);

// NewLocal --
//   Allocate space for an anonymous local variable on the stack frame.
//
// Inputs:
//   scope - the scope to allocate the local on.
//
// Results:
//   A newly allocated local.
//
// Side effects:
//   Allocates a space on the scope's locals for the local. The local should
//   be freed with LocalRelease when no longer in use.
static Local* NewLocal(Scope* scope)
{
  size_t index = scope->locals.size;
  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] == NULL) {
      index = i;
      break;
    }
  }

  if (index == scope->locals.size) {
    FbleVectorAppend(scope->locals, NULL);
    scope->code->_base.locals = scope->locals.size;
  }

  Local* local = FbleAlloc(Local);
  local->index.section = FBLE_LOCALS_FRAME_SECTION;
  local->index.index = index;
  local->refcount = 1;

  scope->locals.xs[index] = local;
  return local;
}

// LocalRelease --
//   Decrement the reference count on a local and free it if appropriate.
//
// Inputs:
//   scope - the scope that the local belongs to
//   local - the local to release. May be NULL.
//
// Results:
//   none
//
// Side effects:
//   Decrements the reference count on the local and frees it if the refcount
//   drops to 0.
static void LocalRelease(Scope* scope, Local* local)
{
  if (local == NULL) {
    return;
  }

  local->refcount--;
  if (local->refcount == 0) {
    assert(local->index.section == FBLE_LOCALS_FRAME_SECTION);
    assert(scope->locals.xs[local->index.index] == local);
    scope->locals.xs[local->index.index] = NULL;
    FbleFree(local);
  }
}

// PushVar --
//   Push a variable onto the current scope.
//
// Inputs:
//   scope - the scope to push the variable on to.
//   local - the local address of the variable. Consumed.
//
// Results:
//   None.
//
// Side effects:
//   Pushes a new variable onto the scope. Takes ownership of the given local,
//   which will be released when the variable is freed by a call to PopVar or
//   FreeScope.
static void PushVar(Scope* scope, Local* local)
{
  FbleVectorAppend(scope->vars, local);
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   scope - the scope to pop from.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope.
static void PopVar(Scope* scope)
{
  scope->vars.size--;
  Local* var = scope->vars.xs[scope->vars.size];
  LocalRelease(scope, var);
}

// GetVar --
//   Lookup a var in the given scope.
//
// Inputs:
//   scope - the scope to look in.
//   index - the index of the variable to look up.
//
// Result:
//   The variable from the scope. The variable is owned by the scope and
//   remains valid until either PopVar is called or the scope is finished.
//
// Side effects:
//   Behavior is undefined if there is no such variable at the given index.
static Local* GetVar(Scope* scope, FbleVarIndex index)
{
  switch (index.source) {
    case FBLE_LOCAL_VAR: {
      assert(index.index < scope->vars.size && "invalid local var index");
      return scope->vars.xs[index.index];
    }

    case FBLE_STATIC_VAR: {
      assert(index.index < scope->statics.size && "invalid static var index");
      return scope->statics.xs[index.index];
    }
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// SetVar --
//   Change the value of a variable in scope.
//
// Inputs:
//   scope - scope of variables
//   index - the index of the local variable to change
//   local - the new value for the local variable
//
// Side effects:
// * Frees the existing local value of the variable.
// * Sets the value of the variable to the given local.
// * Takes ownership of the given local.
static void SetVar(Scope* scope, size_t index, Local* local)
{
  assert(index < scope->vars.size);
  LocalRelease(scope, scope->vars.xs[index]);
  scope->vars.xs[index] = local;
}

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   scope - the scope to initialize.
//   code - a pointer to store the allocated code block for this scope.
//   args - the number of arguments to the function the scope is for.
//   statics - the number of statics captured by the scope (??).
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
// * Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetimes of the code block
//   and the parent scope must exceed the lifetime of this scope.
// * The caller is responsible for calling FbleFreeCode on *code when it
//   is no longer needed.
static void InitScope(Scope* scope, FbleCode** code, size_t args, size_t statics, Scope* parent)
{
  FbleVectorInit(scope->statics);
  for (size_t i = 0; i < statics; ++i) {
    Local* local = FbleAlloc(Local);
    local->index.section = FBLE_STATICS_FRAME_SECTION;
    local->index.index = i;
    local->refcount = 1;
    FbleVectorAppend(scope->statics, local);
  }

  FbleVectorInit(scope->vars);
  FbleVectorInit(scope->locals);

  scope->code = FbleNewCode(args, statics, 0);
  scope->pending_profile_ops = NULL;
  scope->parent = parent;

  *code = scope->code;
}

// FreeScope --
//   Free memory associated with a Scope.
//
// Inputs:
//   scope - the scope to finish.
//
// Side effects:
//   * Frees memory associated with scope.
static void FreeScope(Scope* scope)
{
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleFree(scope->statics.xs[i]);
  }
  FbleFree(scope->statics.xs);

  while (scope->vars.size > 0) {
    PopVar(scope);
  }
  FbleFree(scope->vars.xs);

  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] != NULL) {
      FbleFree(scope->locals.xs[i]);
    }
  }
  FbleFree(scope->locals.xs);

  while (scope->pending_profile_ops != NULL) {
    FbleProfileOp* op = scope->pending_profile_ops;
    scope->pending_profile_ops = op->next;
    FbleFree(op);
  }
}

// AppendInstr --
//   Append an instruction to the code block for the given scope.
//
// Inputs:
//   scope - the scope to append the instruction to.
//   instr - the instruction to append.
//
// Side effects:
//   Appends instr to the code block for the given scope, thus taking
//   ownership of the instr.
static void AppendInstr(Scope* scope, FbleInstr* instr)
{
  assert(instr->profile_ops == NULL);
  instr->profile_ops = scope->pending_profile_ops;
  scope->pending_profile_ops = NULL;
  FbleVectorAppend(scope->code->instrs, instr);
}

// AppendProfileOp --
//   Append a profile op to the code block for the given scope.
//
// Inputs:
//   scope - the scope to append the instruction to.
//   tag - the tag of the profile op to insert.
//   block - the block id if relevant.
//
// Side effects:
//   Appends the profile op to the code block for the given scope.
static void AppendProfileOp(Scope* scope, FbleProfileOpTag tag, FbleBlockId block)
{
  FbleProfileOp* op = FbleAlloc(FbleProfileOp);
  op->tag = tag;
  op->block = block;
  op->next = NULL;

  if (scope->pending_profile_ops == NULL) {
    scope->pending_profile_ops = op;
  } else {
    FbleProfileOp* curr = scope->pending_profile_ops;
    while (curr->next != NULL) {
      curr = curr->next;
    }
    curr->next = op;
  } 
}

// EnterBlock --
//   Enter a new profiling block.
//
// Inputs:
//   blocks - the blocks stack.
//   name - name to add to the current block path for naming the new block.
//   loc - the location of the block.
//   scope - where to add the ENTER_BLOCK instruction to.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock.
static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope)
{
  const char* curr = NULL;
  size_t currlen = 0;
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile->blocks.xs[curr_id]->name.name->str;
    currlen = strlen(curr);
  }

  // Append ".name" to the current block name to figure out the new block
  // name.
  FbleString* str = FbleAllocExtra(FbleString, currlen + strlen(name.name->str) + 3);
  str->refcount = 1;
  str->magic = FBLE_STRING_MAGIC;
  str->str[0] = '\0';
  if (currlen > 0) {
    strcat(str->str, curr);
    strcat(str->str, ".");
  }
  strcat(str->str, name.name->str);
  switch (name.space) {
    case FBLE_NORMAL_NAME_SPACE: break;
    case FBLE_TYPE_NAME_SPACE: strcat(str->str, "@"); break;
  }

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = FbleProfileAddBlock(blocks->profile, nm);

  AppendProfileOp(scope, FBLE_PROFILE_ENTER_OP, id);
  FbleVectorAppend(blocks->stack, id);
}

// EnterBodyBlock --
//   Enter a new body profiling block. This is used for the body of functions
//   and processes that are executed when they are called, not when they are
//   defined.
//
// Inputs:
//   blocks - the blocks stack.
//   loc - The location of the new block.
//   scope - where to add the ENTER_BLOCK instruction to.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock or one of the other
//   functions that exit a block.
static void EnterBodyBlock(Blocks* blocks, FbleLoc loc, Scope* scope)
{
  const char* curr = "";
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile->blocks.xs[curr_id]->name.name->str;
  }

  // Append "!" to the current block name to figure out the new block name.
  FbleString* str = FbleAllocExtra(FbleString, strlen(curr) + 2);
  str->refcount = 1;
  str->magic = FBLE_STRING_MAGIC;
  str->str[0] = '\0';
  strcat(str->str, curr);
  strcat(str->str, "!");

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = FbleProfileAddBlock(blocks->profile, nm);

  AppendProfileOp(scope, FBLE_PROFILE_ENTER_OP, id);
  FbleVectorAppend(blocks->stack, id);
}

// ExitBlock --
//   Exit the current profiling block frame.
//
// Inputs:
//   blocks - the blocks stack.
//   scope - where to append the profile exit op.
//   exit - whether the frame has already been exited.
//
// Side effects:
//   Pops the top block frame off the blocks stack and append a
//   profile exit op to the scope if exit is false.
static void ExitBlock(Blocks* blocks, Scope* scope, bool exit)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;

  if (!exit) {
    AppendProfileOp(scope, FBLE_PROFILE_EXIT_OP, 0);
  }
}

// CompileExit --
//   If exit is true, appends an exit scope instruction to instrs.
//
// Inputs:
//   blocks - the blocks stack.
//   exit - whether we actually want to exit.
//   scope - the scope to append the instructions to.
//   result - the result to return when exiting. May be NULL.
//
// Side effects:
//   If exit is true, appends an exit scope instruction to instrs
static void CompileExit(bool exit, Scope* scope, Local* result)
{
  if (exit && result != NULL) {
    AppendProfileOp(scope, FBLE_PROFILE_EXIT_OP, 0);

    FbleReturnInstr* return_instr = FbleAlloc(FbleReturnInstr);
    return_instr->_base.tag = FBLE_RETURN_INSTR;
    return_instr->_base.profile_ops = NULL;
    return_instr->result = result->index;
    AppendInstr(scope, &return_instr->_base);
  }
}

// CompileExpr --
//   Compile the given expression. Returns the local variable that will hold
//   the result of the expression and generates instructions to compute the
//   value of that expression at runtime.
//
// Inputs:
//   blocks - the blocks stack.
//   exit - if true, generate instructions to exit the current scope.
//   scope - the list of variables in scope.
//   tc - the type checked expression to compile.
//
// Results:
//   The local of the compiled expression.
//
// Side effects:
// * Updates the blocks stack with with compiled block information.
// * Appends instructions to the scope for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   the scope if the expression fails to compile.
// * The caller should call LocalRelease when the returned results are no
//   longer needed. Note that FreeScope calls LocalRelease for all locals
//   allocated to the scope, so that can also be used to clean up the local.
static Local* CompileExpr(Blocks* blocks, bool exit, Scope* scope, FbleTc* v)
{
  switch (v->tag) {
    case FBLE_TYPE_VALUE_TC: {
      Local* local = NewLocal(scope);
      FbleTypeInstr* instr = FbleAlloc(FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      instr->_base.profile_ops = NULL;
      instr->dest = local->index.index;
      AppendInstr(scope, &instr->_base);
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_VAR_TC: {
      FbleVarTc* var_tc = (FbleVarTc*)v;
      Local* local = GetVar(scope, var_tc->index);
      local->refcount++;
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)v;

      size_t base_index = scope->vars.size;

      Local* vars[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        vars[i] = NULL;
        if (let_tc->recursive) {
          vars[i] = NewLocal(scope);
          FbleRefValueInstr* ref_instr = FbleAlloc(FbleRefValueInstr);
          ref_instr->_base.tag = FBLE_REF_VALUE_INSTR;
          ref_instr->_base.profile_ops = NULL;
          ref_instr->dest = vars[i]->index.index;
          AppendInstr(scope, &ref_instr->_base);
        }
        PushVar(scope, vars[i]);
      }

      // Compile the values of the variables.
      Local* defs[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        EnterBlock(blocks, let_tc->bindings.xs[i].profile_name, let_tc->bindings.xs[i].profile_loc, scope);
        defs[i] = CompileExpr(blocks, false, scope, let_tc->bindings.xs[i].tc);
        ExitBlock(blocks, scope, false);
      }

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        if (let_tc->recursive) {
          FbleRefDefInstr* ref_def_instr = FbleAlloc(FbleRefDefInstr);
          ref_def_instr->_base.tag = FBLE_REF_DEF_INSTR;
          ref_def_instr->_base.profile_ops = NULL;
          ref_def_instr->loc = FbleCopyLoc(let_tc->bindings.xs[i].var_loc);
          ref_def_instr->ref = vars[i]->index.index;
          ref_def_instr->value = defs[i]->index;
          AppendInstr(scope, &ref_def_instr->_base);
        }
        SetVar(scope, base_index + i, defs[i]);
      }

      Local* body = CompileExpr(blocks, exit, scope, let_tc->body);

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        PopVar(scope);
      }

      return body;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* struct_tc = (FbleStructValueTc*)v;

      size_t argc = struct_tc->fieldc;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, scope, struct_tc->fields[i]);
      }

      Local* local = NewLocal(scope);
      FbleStructValueInstr* struct_instr = FbleAlloc(FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->_base.profile_ops = NULL;
      struct_instr->dest = local->index.index;
      FbleVectorInit(struct_instr->args);
      AppendInstr(scope, &struct_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(struct_instr->args, args[i]->index);
        LocalRelease(scope, args[i]);
      }

      return local;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* union_tc = (FbleUnionValueTc*)v;
      Local* arg = CompileExpr(blocks, false, scope, union_tc->arg);

      Local* local = NewLocal(scope);
      FbleUnionValueInstr* union_instr = FbleAlloc(FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->_base.profile_ops = NULL;
      union_instr->tag = union_tc->tag;
      union_instr->arg = arg->index;
      union_instr->dest = local->index.index;
      AppendInstr(scope, &union_instr->_base);
      CompileExit(exit, scope, local);
      LocalRelease(scope, arg);
      return local;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* select_tc = (FbleUnionSelectTc*)v;
      Local* condition = CompileExpr(blocks, false, scope, select_tc->condition);

      if (exit) {
        AppendProfileOp(scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->_base.profile_ops = NULL;
      select_instr->loc = FbleCopyLoc(select_tc->loc);
      select_instr->condition = condition->index;
      FbleVectorInit(select_instr->jumps);
      AppendInstr(scope, &select_instr->_base);

      size_t select_instr_pc = scope->code->instrs.size;
      size_t branch_offsets[select_tc->choicec];
      Local* target = exit ? NULL : NewLocal(scope);
      FbleJumpInstr* exit_jumps[select_tc->choicec];

      for (size_t i = 0; i < select_tc->choicec; ++i) {
        exit_jumps[i] = NULL;

        // Check if we have already generated the code for this branch.
        bool already_done = false;
        for (size_t j = 0; j < i; ++j) {
          if (select_tc->choices[i] == select_tc->choices[j]) {
            branch_offsets[i] = branch_offsets[j];
            already_done = true;
            break;
          }
        }

        if (!already_done) {
          // TODO: Could we arrange for the branches to put their value in
          // the target directly instead of in some cases allocating a new
          // local and then copying that to target?
          branch_offsets[i] = scope->code->instrs.size - select_instr_pc;
          Local* result = CompileExpr(blocks, exit, scope, select_tc->choices[i]);

          if (!exit) {
            FbleCopyInstr* copy = FbleAlloc(FbleCopyInstr);
            copy->_base.tag = FBLE_COPY_INSTR;
            copy->_base.profile_ops = NULL;
            copy->source = result->index;
            copy->dest = target->index.index;
            AppendInstr(scope, &copy->_base);
          }

          LocalRelease(scope, result);

          if (!exit) {
            exit_jumps[i] = FbleAlloc(FbleJumpInstr);
            exit_jumps[i]->_base.tag = FBLE_JUMP_INSTR;
            exit_jumps[i]->_base.profile_ops = NULL;
            exit_jumps[i]->count = scope->code->instrs.size + 1;
            AppendInstr(scope, &exit_jumps[i]->_base);
          }
        }

        FbleVectorAppend(select_instr->jumps, branch_offsets[i]);
      }

      // Fix up exit jumps now that all the branch code is generated.
      if (!exit) {
        for (size_t i = 0; i < select_tc->choicec; ++i) {
          if (exit_jumps[i] != NULL) {
            exit_jumps[i]->count = scope->code->instrs.size - exit_jumps[i]->count;
          }
        }
      }

      // TODO: We ought to release the condition right after jumping into
      // a branch, otherwise we'll end up unnecessarily holding on to it
      // for the full duration of the block. Technically this doesn't
      // appear to be a violation of the language spec, because it only
      // effects constants in runtime. But we probably ought to fix it
      // anyway.
      LocalRelease(scope, condition);
      return target;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* access_tc = (FbleDataAccessTc*)v;
      Local* obj = CompileExpr(blocks, false, scope, access_tc->obj);

      FbleInstrTag tag;
      if (access_tc->datatype == FBLE_STRUCT_DATATYPE) {
        tag = FBLE_STRUCT_ACCESS_INSTR;
      } else {
        assert(access_tc->datatype == FBLE_UNION_DATATYPE);
        tag = FBLE_UNION_ACCESS_INSTR;
      }

      FbleAccessInstr* access = FbleAlloc(FbleAccessInstr);
      access->_base.tag = tag;
      access->_base.profile_ops = NULL;
      access->obj = obj->index;
      access->tag = access_tc->tag;
      access->loc = FbleCopyLoc(access_tc->loc);
      AppendInstr(scope, &access->_base);

      Local* local = NewLocal(scope);
      access->dest = local->index.index;
      CompileExit(exit, scope, local);
      LocalRelease(scope, obj);
      return local;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)v;
      size_t argc = func_tc->argc;

      FbleFuncValueInstr* instr = FbleAlloc(FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->_base.profile_ops = NULL;

      FbleVectorInit(instr->scope);
      for (size_t i = 0; i < func_tc->scope.size; ++i) {
        Local* local = GetVar(scope, func_tc->scope.xs[i]);
        FbleVectorAppend(instr->scope, local->index);
      }

      Scope func_scope;
      InitScope(&func_scope, &instr->code, argc, func_tc->scope.size, scope);
      EnterBodyBlock(blocks, func_tc->body_loc, &func_scope);

      for (size_t i = 0; i < argc; ++i) {
        Local* local = NewLocal(&func_scope);
        PushVar(&func_scope, local);
      }

      Local* func_result = CompileExpr(blocks, true, &func_scope, func_tc->body);
      ExitBlock(blocks, &func_scope, true);
      LocalRelease(&func_scope, func_result);
      FreeScope(&func_scope);

      Local* local = NewLocal(scope);
      instr->dest = local->index.index;
      AppendInstr(scope, &instr->_base);
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)v;
      Local* func = CompileExpr(blocks, false, scope, apply_tc->func);

      size_t argc = apply_tc->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, scope, apply_tc->args.xs[i]);
      }

      if (exit) {
        AppendProfileOp(scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      Local* dest = exit ? NULL : NewLocal(scope);

      FbleCallInstr* call_instr = FbleAlloc(FbleCallInstr);
      call_instr->_base.tag = FBLE_CALL_INSTR;
      call_instr->_base.profile_ops = NULL;
      call_instr->loc = FbleCopyLoc(apply_tc->loc);
      call_instr->exit = exit;
      call_instr->func = func->index;
      FbleVectorInit(call_instr->args);
      call_instr->dest = exit ? 0 : dest->index.index;
      AppendInstr(scope, &call_instr->_base);

      LocalRelease(scope, func);
      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(call_instr->args, args[i]->index);
        LocalRelease(scope, args[i]);
      }

      return dest;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)v;

      FbleLinkInstr* link = FbleAlloc(FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;
      link->_base.profile_ops = NULL;

      Local* get_local = NewLocal(scope);
      link->get = get_local->index.index;
      PushVar(scope, get_local);

      Local* put_local = NewLocal(scope);
      link->put = put_local->index.index;
      PushVar(scope, put_local);

      AppendInstr(scope, &link->_base);

      Local* result = CompileExpr(blocks, exit, scope, link_tc->body);

      PopVar(scope);
      PopVar(scope);
      return result;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)v;

      Local* args[exec_tc->bindings.size];
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        args[i] = CompileExpr(blocks, false, scope, exec_tc->bindings.xs[i]);
      }

      FbleForkInstr* fork = FbleAlloc(FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      fork->_base.profile_ops = NULL;
      FbleVectorInit(fork->args);
      fork->dests.xs = FbleArrayAlloc(FbleLocalIndex, exec_tc->bindings.size);
      fork->dests.size = exec_tc->bindings.size;
      AppendInstr(scope, &fork->_base);

      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        // Note: Make sure we call NewLocal before calling LocalRelease on any
        // of the arguments.
        Local* local = NewLocal(scope);
        fork->dests.xs[i] = local->index.index;
        PushVar(scope, local);
      }

      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        // TODO: Does this hold on to the bindings longer than we want to?
        if (args[i] != NULL) {
          FbleVectorAppend(fork->args, args[i]->index);
          LocalRelease(scope, args[i]);
        }
      }

      Local* local = CompileExpr(blocks, exit, scope, exec_tc->body);

      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        PopVar(scope);
      }

      return local;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)v;
      EnterBlock(blocks, profile_tc->name, profile_tc->loc, scope);
      Local* result = CompileExpr(blocks, exit, scope, profile_tc->body);
      ExitBlock(blocks, scope, exit);
      return result;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// Compile --
//   Compile a type-checked expression.
//
// Inputs:
//   argc - the number of local variables to reserve for arguments.
//   tc - the type-checked expression to compile.
//   name - the name of the expression to use in profiling. Borrowed.
//   profile - profile to populate with blocks. May be NULL.
//
// Results:
//   The compiled program.
//
// Side effects:
// * Adds blocks to the given profile.
// * The caller should call FbleFreeCode to release resources
//   associated with the returned program when it is no longer needed.
static FbleCode* Compile(size_t argc, FbleTc* tc, FbleName name, FbleProfile* profile)
{
  bool profiling_disabled = false;
  if (profile == NULL) {
    // Profiling is disabled. Allocate a new temporary profile to use during
    // compilation so we don't have to special case the code for emitting
    // profiling information.
    profiling_disabled = true;
    profile = FbleNewProfile();
  }

  Blocks blocks;
  FbleVectorInit(blocks.stack);
  blocks.profile = profile;

  FbleCode* code;
  Scope scope;
  InitScope(&scope, &code, argc, 0, NULL);

  for (size_t i = 0; i < argc; ++i) {
    Local* local = NewLocal(&scope);
    PushVar(&scope, local);
  }

  // CompileExpr assumes it is in a profile block that it needs to exit when
  // exit is true, so make sure we wrap the top level expression in a profile
  // block.
  EnterBlock(&blocks, name, name.loc, &scope);
  CompileExpr(&blocks, true, &scope, tc);
  ExitBlock(&blocks, &scope, true);

  FreeScope(&scope);
  assert(blocks.stack.size == 0);
  FbleFree(blocks.stack.xs);
  if (profiling_disabled) {
    FbleFreeProfile(profile);
  }
  return code;
}

// FbleFreeCompiledProgram -- see documentation in fble-compile.h
void FbleFreeCompiledProgram(FbleCompiledProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleCompiledModule* module = program->modules.xs + i;
      FbleFreeModulePath(module->path);
      for (size_t j = 0; j < module->deps.size; ++j) {
        FbleFreeModulePath(module->deps.xs[j]);
      }
      FbleFree(module->deps.xs);
      FbleFreeCode(module->code);
    }
    FbleFree(program->modules.xs);
    FbleFree(program);
  }
}

// FbleCompile -- see documentation in fble-compile.h
FbleCompiledProgram* FbleCompile(FbleLoadedProgram* program, FbleProfile* profile)
{
  FbleTcV typechecked;
  FbleVectorInit(typechecked);
  if (!FbleTypeCheck(program, &typechecked)) { 
    for (size_t i = 0; i < typechecked.size; ++i) {
      FbleFreeTc(typechecked.xs[i]);
    }
    FbleFree(typechecked.xs);
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleAlloc(FbleCompiledProgram);
  FbleVectorInit(compiled->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleLoadedModule* module = program->modules.xs + i;

    FbleCompiledModule* compiled_module = FbleVectorExtend(compiled->modules);
    compiled_module->path = FbleCopyModulePath(module->path);
    FbleVectorInit(compiled_module->deps);
    for (size_t d = 0; d < module->deps.size; ++d) {
      FbleVectorAppend(compiled_module->deps, FbleCopyModulePath(module->deps.xs[d]));
    }

    FbleName label = FbleModulePathName(module->path);
    compiled_module->code = Compile(module->deps.size, typechecked.xs[i], label, profile);
    FbleFreeTc(typechecked.xs[i]);
    FbleFreeName(label);
  }

  FbleFree(typechecked.xs);
  return compiled;
}
