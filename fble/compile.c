// compile.c --
//   This file implements the fble compilation routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "instr.h"
#include "typecheck.h"

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
  FbleInstrBlock* code;
  FbleProfileOp* pending_profile_ops;
  struct Scope* parent;
} Scope;

static Local* NewLocal(FbleArena* arena, Scope* scope);
static void LocalRelease(FbleArena* arena, Scope* scope, Local* local, bool exit);
static void PushVar(FbleArena* arena, Scope* scope, Local* local);
static void PopVar(FbleArena* arena, Scope* scope, bool exit);
static Local* GetVar(FbleArena* arena, Scope* scope, FbleVarIndex index);
static void SetVar(FbleArena* arena, Scope* scope, size_t index, Local* local);

static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock** code, size_t statics, Scope* parent);
static void FreeScope(FbleArena* arena, Scope* scope, bool exit);
static void AppendInstr(FbleArena* arena, Scope* scope, FbleInstr* instr);
static void AppendProfileOp(FbleArena* arena, Scope* scope, FbleProfileOpTag tag, FbleBlockId block);

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

static void EnterBlock(FbleArena* arena, Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope);
static void EnterBodyBlock(FbleArena* arena, Blocks* blocks, FbleLoc loc, Scope* scope);
static void ExitBlock(FbleArena* arena, Blocks* blocks, Scope* scope, bool exit);

static void CompileExit(FbleArena* arena, bool exit, Scope* scope, Local* result);
static Local* CompileExpr(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleTc* tc);

// NewLocal --
//   Allocate space for an anonymous local variable on the stack frame.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to allocate the local on.
//
// Results:
//   A newly allocated local.
//
// Side effects:
//   Allocates a space on the scope's locals for the local. The local should
//   be freed with LocalRelease when no longer in use.
static Local* NewLocal(FbleArena* arena, Scope* scope)
{
  size_t index = scope->locals.size;
  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] == NULL) {
      index = i;
      break;
    }
  }

  if (index == scope->locals.size) {
    FbleVectorAppend(arena, scope->locals, NULL);
    scope->code->locals = scope->locals.size;
  }

  Local* local = FbleAlloc(arena, Local);
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
//   arena - the arena for allocations
//   scope - the scope that the local belongs to
//   local - the local to release. May be NULL.
//   exit - whether the frame has already been exited
//
// Results:
//   none
//
// Side effects:
//   Decrements the reference count on the local and frees it if the refcount
//   drops to 0. Generates instructions to free the value at runtime as
//   appropriate.
static void LocalRelease(FbleArena* arena, Scope* scope, Local* local, bool exit)
{
  if (local == NULL) {
    return;
  }

  local->refcount--;
  if (local->refcount == 0) {
    assert(local->index.section == FBLE_LOCALS_FRAME_SECTION);

    if (!exit) {
      FbleReleaseInstr* release = FbleAlloc(arena, FbleReleaseInstr);
      release->_base.tag = FBLE_RELEASE_INSTR;
      release->_base.profile_ops = NULL;
      release->value = local->index.index;
      AppendInstr(arena, scope, &release->_base);
    }

    assert(scope->locals.xs[local->index.index] == local);
    scope->locals.xs[local->index.index] = NULL;
    FbleFree(arena, local);
  }
}

// PushVar --
//   Push a variable onto the current scope.
//
// Inputs:
//   arena - arena to use for allocations.
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
static void PushVar(FbleArena* arena, Scope* scope, Local* local)
{
  FbleVectorAppend(arena, scope->vars, local);
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to pop from.
//   exit - whether the frame has already been exited.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope.
static void PopVar(FbleArena* arena, Scope* scope, bool exit)
{
  scope->vars.size--;
  Local* var = scope->vars.xs[scope->vars.size];
  LocalRelease(arena, scope, var, exit);
}

// GetVar --
//   Lookup a var in the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to look in.
//   index - the index of the variable to look up.
//
// Result:
//   The variable from the scope. The variable is owned by the scope and
//   remains valid until either PopVar is called or the scope is finished.
//
// Side effects:
//   Behavior is undefined if there is no such variable at the given index.
static Local* GetVar(FbleArena* arena, Scope* scope, FbleVarIndex index)
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
//   arena - arena to use for allocations
//   scope - scope of variables
//   index - the index of the local variable to change
//   local - the new value for the local variable
//
// Side effects:
// * Frees the existing local value of the variable.
// * Sets the value of the variable to the given local.
// * Takes ownership of the given local.
static void SetVar(FbleArena* arena, Scope* scope, size_t index, Local* local)
{
  assert(index < scope->vars.size);
  LocalRelease(arena, scope, scope->vars.xs[index], false);
  scope->vars.xs[index] = local;
}

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to initialize.
//   code - a pointer to store the allocated code block for this scope.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetimes of the code block
//   and the parent scope must exceed the lifetime of this scope.
//   The caller is responsible for calling FbleFreeInstrBlock on *code when it
//   is no longer needed.
static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock** code, size_t statics, Scope* parent)
{
  FbleVectorInit(arena, scope->statics);
  for (size_t i = 0; i < statics; ++i) {
    Local* local = FbleAlloc(arena, Local);
    local->index.section = FBLE_STATICS_FRAME_SECTION;
    local->index.index = i;
    local->refcount = 1;
    FbleVectorAppend(arena, scope->statics, local);
  }

  FbleVectorInit(arena, scope->vars);
  FbleVectorInit(arena, scope->locals);

  scope->code = FbleAlloc(arena, FbleInstrBlock);
  scope->code->refcount = 1;
  scope->code->magic = FBLE_INSTR_BLOCK_MAGIC;
  scope->code->statics = statics;
  scope->code->locals = 0;
  FbleVectorInit(arena, scope->code->instrs);
  scope->pending_profile_ops = NULL;
  scope->parent = parent;

  *code = scope->code;
}

// FreeScope --
//   Free memory associated with a Scope.
//
// Inputs:
//   arena - arena to use for allocations
//   scope - the scope to finish.
//   exit - whether the frame has already been exited.
//
// Results:
//   None.
//
// Side effects:
//   * Frees memory associated with scope.
static void FreeScope(FbleArena* arena, Scope* scope, bool exit)
{
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleFree(arena, scope->statics.xs[i]);
  }
  FbleFree(arena, scope->statics.xs);

  while (scope->vars.size > 0) {
    PopVar(arena, scope, exit);
  }
  FbleFree(arena, scope->vars.xs);

  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] != NULL) {
      FbleFree(arena, scope->locals.xs[i]);
    }
  }
  FbleFree(arena, scope->locals.xs);

  while (scope->pending_profile_ops != NULL) {
    FbleProfileOp* op = scope->pending_profile_ops;
    scope->pending_profile_ops = op->next;
    FbleFree(arena, op);
  }
}

// AppendInstr --
//   Append an instruction to the code block for the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to append the instruction to.
//   instr - the instruction to append.
//
// Result:
//   none.
//
// Side effects:
//   Appends instr to the code block for the given scope, thus taking
//   ownership of the instr.
static void AppendInstr(FbleArena* arena, Scope* scope, FbleInstr* instr)
{
  assert(instr->profile_ops == NULL);
  instr->profile_ops = scope->pending_profile_ops;
  scope->pending_profile_ops = NULL;
  FbleVectorAppend(arena, scope->code->instrs, instr);
}

// AppendProfileOp --
//   Append a profile op to the code block for the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to append the instruction to.
//   tag - the tag of the profile op to insert.
//   block - the block id if relevant.
//
// Result:
//   none.
//
// Side effects:
//   Appends the profile op to the code block for the given scope.
static void AppendProfileOp(FbleArena* arena, Scope* scope, FbleProfileOpTag tag, FbleBlockId block)
{
  FbleProfileOp* op = FbleAlloc(arena, FbleProfileOp);
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
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   name - name to add to the current block path for naming the new block.
//   loc - the location of the block.
//   scope - where to add the ENTER_BLOCK instruction to.
//
// Results:
//   none.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock.
static void EnterBlock(FbleArena* arena, Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope)
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
  FbleString* str = FbleAllocExtra(arena, FbleString, currlen + strlen(name.name->str) + 3);
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
    case FBLE_MODULE_NAME_SPACE: strcat(str->str, "%"); break;
  }

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = FbleProfileAddBlock(arena, blocks->profile, nm);

  AppendProfileOp(arena, scope, FBLE_PROFILE_ENTER_OP, id);
  FbleVectorAppend(arena, blocks->stack, id);
}

// EnterBodyBlock --
//   Enter a new body profiling block. This is used for the body of functions
//   and processes that are executed when they are called, not when they are
//   defined.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   loc - The location of the new block.
//   scope - where to add the ENTER_BLOCK instruction to.
//
// Results:
//   none.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock or one of the other
//   functions that exit a block.
static void EnterBodyBlock(FbleArena* arena, Blocks* blocks, FbleLoc loc, Scope* scope)
{
  const char* curr = "";
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile->blocks.xs[curr_id]->name.name->str;
  }

  // Append "!" to the current block name to figure out the new block name.
  FbleString* str = FbleAllocExtra(arena, FbleString, strlen(curr) + 2);
  str->refcount = 1;
  str->magic = FBLE_STRING_MAGIC;
  str->str[0] = '\0';
  strcat(str->str, curr);
  strcat(str->str, "!");

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = FbleProfileAddBlock(arena, blocks->profile, nm);

  AppendProfileOp(arena, scope, FBLE_PROFILE_ENTER_OP, id);
  FbleVectorAppend(arena, blocks->stack, id);
}

// ExitBlock --
//   Exit the current profiling block frame.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   scope - where to append the profile exit op.
//   exit - whether the frame has already been exited.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top block frame off the blocks stack and append a
//   profile exit op to the scope if exit is false.
static void ExitBlock(FbleArena* arena, Blocks* blocks, Scope* scope, bool exit)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;

  if (!exit) {
    AppendProfileOp(arena, scope, FBLE_PROFILE_EXIT_OP, 0);
  }
}

// CompileExit --
//   If exit is true, appends an exit scope instruction to instrs.
//
// Inputs:
//   arena - arena for allocations.
//   blocks - the blocks stack.
//   exit - whether we actually want to exit.
//   scope - the scope to append the instructions to.
//   result - the result to return when exiting. May be NULL.
//
// Results:
//   none.
//
// Side effects:
//   If exit is true, appends an exit scope instruction to instrs
static void CompileExit(FbleArena* arena, bool exit, Scope* scope, Local* result)
{
  if (exit && result != NULL) {
    // Clean up any remaining locals before we return from the stack frame.
    for (size_t i = 0; i < scope->locals.size; ++i) {
      Local* local = scope->locals.xs[i];
      if (local != NULL && local != result) {
        FbleReleaseInstr* release = FbleAlloc(arena, FbleReleaseInstr);
        release->_base.tag = FBLE_RELEASE_INSTR;
        release->_base.profile_ops = NULL;
        release->value = local->index.index;
        AppendInstr(arena, scope, &release->_base);
      }
    }

    AppendProfileOp(arena, scope, FBLE_PROFILE_EXIT_OP, 0);

    FbleReturnInstr* return_instr = FbleAlloc(arena, FbleReturnInstr);
    return_instr->_base.tag = FBLE_RETURN_INSTR;
    return_instr->_base.profile_ops = NULL;
    return_instr->result = result->index;
    AppendInstr(arena, scope, &return_instr->_base);
  }
}

// CompileExpr --
//   Compile the given expression. Returns the local variable that will hold
//   the result of the expression and generates instructions to compute the
//   value of that expression at runtime.
//
// Inputs:
//   arena - arena to use for type allocations.
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
static Local* CompileExpr(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleTc* tc)
{
  switch (tc->tag) {
    case FBLE_TYPE_TC: {
      Local* local = NewLocal(arena, scope);
      FbleTypeInstr* instr = FbleAlloc(arena, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      instr->_base.profile_ops = NULL;
      instr->dest = local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_VAR_TC: {
      FbleVarTc* var_tc = (FbleVarTc*)tc;
      Local* local = GetVar(arena, scope, var_tc->index);
      local->refcount++;
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_LET_TC: {
      FbleLetTc* let_tc = (FbleLetTc*)tc;

      size_t base_index = scope->vars.size;

      Local* vars[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        vars[i] = NULL;
        if (let_tc->recursive) {
          vars[i] = NewLocal(arena, scope);
          FbleRefValueInstr* ref_instr = FbleAlloc(arena, FbleRefValueInstr);
          ref_instr->_base.tag = FBLE_REF_VALUE_INSTR;
          ref_instr->_base.profile_ops = NULL;
          ref_instr->dest = vars[i]->index.index;
          AppendInstr(arena, scope, &ref_instr->_base);
        }
        PushVar(arena, scope, vars[i]);
      }

      // Compile the values of the variables.
      Local* defs[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        defs[i] = CompileExpr(arena, blocks, false, scope, let_tc->bindings.xs[i]);
      }

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        if (let_tc->recursive) {
          FbleRefDefInstr* ref_def_instr = FbleAlloc(arena, FbleRefDefInstr);
          ref_def_instr->_base.tag = FBLE_REF_DEF_INSTR;
          ref_def_instr->_base.profile_ops = NULL;
          ref_def_instr->ref = vars[i]->index.index;
          ref_def_instr->value = defs[i]->index;
          AppendInstr(arena, scope, &ref_def_instr->_base);
        }
        SetVar(arena, scope, base_index + i, defs[i]);
      }

      Local* body = CompileExpr(arena, blocks, exit, scope, let_tc->body);

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        PopVar(arena, scope, exit);
      }

      return body;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* struct_tc = (FbleStructValueTc*)tc;

      size_t argc = struct_tc->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(arena, blocks, false, scope, struct_tc->args.xs[i]);
      }

      Local* local = NewLocal(arena, scope);
      FbleStructValueInstr* struct_instr = FbleAlloc(arena, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->_base.profile_ops = NULL;
      struct_instr->dest = local->index.index;
      FbleVectorInit(arena, struct_instr->args);
      AppendInstr(arena, scope, &struct_instr->_base);
      CompileExit(arena, exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(arena, struct_instr->args, args[i]->index);
        LocalRelease(arena, scope, args[i], exit);
      }

      return local;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* access_tc = (FbleDataAccessTc*)tc;
      Local* obj = CompileExpr(arena, blocks, false, scope, access_tc->obj);

      FbleInstrTag tag;
      if (access_tc->datatype == FBLE_STRUCT_DATATYPE) {
        tag = FBLE_STRUCT_ACCESS_INSTR;
      } else {
        assert(access_tc->datatype == FBLE_UNION_DATATYPE);
        tag = FBLE_UNION_ACCESS_INSTR;
      }

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = tag;
      access->_base.profile_ops = NULL;
      access->obj = obj->index;
      access->tag = access_tc->tag;
      access->loc = FbleCopyLoc(access_tc->loc);
      AppendInstr(arena, scope, &access->_base);

      Local* local = NewLocal(arena, scope);
      access->dest = local->index.index;
      CompileExit(arena, exit, scope, local);
      LocalRelease(arena, scope, obj, exit);
      return local;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* union_tc = (FbleUnionValueTc*)tc;
      Local* arg = CompileExpr(arena, blocks, false, scope, union_tc->arg);

      Local* local = NewLocal(arena, scope);
      FbleUnionValueInstr* union_instr = FbleAlloc(arena, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->_base.profile_ops = NULL;
      union_instr->tag = union_tc->tag;
      union_instr->arg = arg->index;
      union_instr->dest = local->index.index;
      AppendInstr(arena, scope, &union_instr->_base);
      CompileExit(arena, exit, scope, local);
      LocalRelease(arena, scope, arg, exit);
      return local;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* select_tc = (FbleUnionSelectTc*)tc;
      Local* condition = CompileExpr(arena, blocks, false, scope, select_tc->condition);

      if (exit) {
        AppendProfileOp(arena, scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(arena, FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->_base.profile_ops = NULL;
      select_instr->loc = FbleCopyLoc(tc->loc);
      select_instr->condition = condition->index;
      FbleVectorInit(arena, select_instr->jumps);
      AppendInstr(arena, scope, &select_instr->_base);

      size_t select_instr_pc = scope->code->instrs.size;
      size_t branch_offsets[select_tc->branches.size];

      // Generate code for each of the branches.
      Local* target = exit ? NULL : NewLocal(arena, scope);
      FbleJumpInstr* exit_jumps[select_tc->choices.size];
      for (size_t i = 0; i < select_tc->branches.size; ++i) {
        // TODO: Could we arrange for the branches to put their value in the
        // target directly instead of in some cases allocating a new local and
        // then copying that to target?
        branch_offsets[i] = scope->code->instrs.size - select_instr_pc;
        Local* result = CompileExpr(arena, blocks, exit, scope, select_tc->branches.xs[i]);

        if (!exit) {
          FbleCopyInstr* copy = FbleAlloc(arena, FbleCopyInstr);
          copy->_base.tag = FBLE_COPY_INSTR;
          copy->_base.profile_ops = NULL;
          copy->source = result->index;
          copy->dest = target->index.index;
          AppendInstr(arena, scope, &copy->_base);
        }

        LocalRelease(arena, scope, result, exit);

        if (!exit) {
          exit_jumps[i] = FbleAlloc(arena, FbleJumpInstr);
          exit_jumps[i]->_base.tag = FBLE_JUMP_INSTR;
          exit_jumps[i]->_base.profile_ops = NULL;
          exit_jumps[i]->count = scope->code->instrs.size + 1;
          AppendInstr(arena, scope, &exit_jumps[i]->_base);
        }
      }

      // Fix up exit jumps now that all the branch code is generated.
      if (!exit) {
        for (size_t i = 0; i < select_tc->branches.size; ++i) {
          exit_jumps[i]->count = scope->code->instrs.size - exit_jumps[i]->count;
        }
      }
      
      // Fix up the jumps into the code generated for branches.
      for (size_t i = 0; i < select_tc->choices.size; ++i) {
        assert(select_tc->choices.xs[i] < select_tc->branches.size);
        size_t offset = branch_offsets[select_tc->choices.xs[i]];
        FbleVectorAppend(arena, select_instr->jumps, offset);
      }

      // TODO: We ought to release the condition right after jumping into a
      // branch, otherwise we'll end up unnecessarily holding on to it for the
      // full duration of the block. Technically this doesn't appear to be a
      // violation of the language spec, because it only effects constants in
      // runtime. But we probably ought to fix it anyway.
      LocalRelease(arena, scope, condition, exit);
      return target;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)tc;
      size_t argc = func_tc->argc;

      FbleFuncValueInstr* instr = FbleAlloc(arena, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->_base.profile_ops = NULL;
      instr->argc = argc;

      FbleVectorInit(arena, instr->scope);
      for (size_t i = 0; i < func_tc->scope.size; ++i) {
        Local* local = GetVar(arena, scope, func_tc->scope.xs[i]);
        FbleVectorAppend(arena, instr->scope, local->index);
      }

      Scope func_scope;
      InitScope(arena, &func_scope, &instr->code, func_tc->scope.size, scope);
      EnterBodyBlock(arena, blocks, func_tc->body->loc, &func_scope);

      for (size_t i = 0; i < argc; ++i) {
        Local* local = NewLocal(arena, &func_scope);
        PushVar(arena, &func_scope, local);
      }

      Local* func_result = CompileExpr(arena, blocks, true, &func_scope, func_tc->body);
      ExitBlock(arena, blocks, &func_scope, true);
      LocalRelease(arena, &func_scope, func_result, true);
      FreeScope(arena, &func_scope, true);

      Local* local = NewLocal(arena, scope);
      instr->dest = local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)tc;
      Local* func = CompileExpr(arena, blocks, false, scope, apply_tc->func);

      size_t argc = apply_tc->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(arena, blocks, false, scope, apply_tc->args.xs[i]);
      }

      if (exit) {
        // Free any locals that we do not need to make the tail call.
        for (size_t i = 0; i < scope->locals.size; ++i) {
          Local* local = scope->locals.xs[i];
          if (local != NULL && local != func) {
            bool is_arg = false;
            for (size_t j = 0; j < argc; ++j) {
              if (args[j] == local) {
                is_arg = true;
                break;
              }
            }

            if (!is_arg) {
              FbleReleaseInstr* release = FbleAlloc(arena, FbleReleaseInstr);
              release->_base.tag = FBLE_RELEASE_INSTR;
              release->_base.profile_ops = NULL;
              release->value = local->index.index;
              AppendInstr(arena, scope, &release->_base);
            }
          }
        }

        AppendProfileOp(arena, scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      Local* dest = exit ? NULL : NewLocal(arena, scope);

      FbleCallInstr* call_instr = FbleAlloc(arena, FbleCallInstr);
      call_instr->_base.tag = FBLE_CALL_INSTR;
      call_instr->_base.profile_ops = NULL;
      call_instr->loc = FbleCopyLoc(tc->loc);
      call_instr->exit = exit;
      call_instr->func = func->index;
      FbleVectorInit(arena, call_instr->args);
      call_instr->dest = exit ? 0 : dest->index.index;
      AppendInstr(arena, scope, &call_instr->_base);

      LocalRelease(arena, scope, func, exit);
      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(arena, call_instr->args, args[i]->index);
        LocalRelease(arena, scope, args[i], exit);
      }

      return dest;
    }

    case FBLE_LINK_TC: {
      FbleLinkTc* link_tc = (FbleLinkTc*)tc;

      FbleLinkInstr* link = FbleAlloc(arena, FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;
      link->_base.profile_ops = NULL;

      Local* get_local = NewLocal(arena, scope);
      link->get = get_local->index.index;
      PushVar(arena, scope, get_local);

      Local* put_local = NewLocal(arena, scope);
      link->put = put_local->index.index;
      PushVar(arena, scope, put_local);

      AppendInstr(arena, scope, &link->_base);

      Local* result = CompileExpr(arena, blocks, exit, scope, link_tc->body);

      PopVar(arena, scope, exit);
      PopVar(arena, scope, exit);
      return result;
    }

    case FBLE_EXEC_TC: {
      FbleExecTc* exec_tc = (FbleExecTc*)tc;

      Local* args[exec_tc->bindings.size];
      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        args[i] = CompileExpr(arena, blocks, false, scope, exec_tc->bindings.xs[i]);
      }

      FbleForkInstr* fork = FbleAlloc(arena, FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      fork->_base.profile_ops = NULL;
      FbleVectorInit(arena, fork->args);
      fork->dests.xs = FbleArrayAlloc(arena, FbleLocalIndex, exec_tc->bindings.size);
      fork->dests.size = exec_tc->bindings.size;
      AppendInstr(arena, scope, &fork->_base);

      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        // Note: Make sure we call NewLocal before calling LocalRelease on any
        // of the arguments.
        Local* local = NewLocal(arena, scope);
        fork->dests.xs[i] = local->index.index;
        PushVar(arena, scope, local);
      }

      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        // TODO: Does this hold on to the bindings longer than we want to?
        if (args[i] != NULL) {
          FbleVectorAppend(arena, fork->args, args[i]->index);
          LocalRelease(arena, scope, args[i], false);
        }
      }

      Local* local = CompileExpr(arena, blocks, exit, scope, exec_tc->body);

      for (size_t i = 0; i < exec_tc->bindings.size; ++i) {
        PopVar(arena, scope, exit);
      }

      return local;
    }

    case FBLE_SYMBOLIC_VALUE_TC: {
      assert(false && "TODO: Compile FBLE_SYMBOLIC_VALUE_TC");
      return NULL;
    }

    case FBLE_SYMBOLIC_COMPILE_TC: {
      assert(false && "TODO: Compile FBLE_SYMBOLIC_COMPILE_TC");
      return NULL;
    }

    case FBLE_PROFILE_TC: {
      FbleProfileTc* profile_tc = (FbleProfileTc*)tc;
      EnterBlock(arena, blocks, profile_tc->name, tc->loc, scope);
      Local* result = CompileExpr(arena, blocks, exit, scope, profile_tc->body);
      ExitBlock(arena, blocks, scope, exit);
      return result;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// FbleCompile -- see documentation in fble.h
FbleCompiledProgram* FbleCompile(FbleArena* arena, FbleProgram* program, FbleProfile* profile)
{
  bool profiling_disabled = false;
  if (profile == NULL) {
    // Profiling is disabled. Allocate a new temporary profile to use during
    // compilation so we don't have to special case the code for emitting
    // profiling information.
    profiling_disabled = true;
    profile = FbleNewProfile(arena);
  }

  Blocks block_stack;
  FbleVectorInit(arena, block_stack.stack);
  block_stack.profile = profile;

  // The entry associated with FBLE_ROOT_BLOCK_ID.
  // TODO: Does it make sense to use the main program for the location?
  FbleName entry_name = {
    .name = FbleNewString(arena, ""),
    .loc = program->main->loc,
    .space = FBLE_NORMAL_NAME_SPACE,
  };

  FbleInstrBlock* code;
  Scope scope;
  InitScope(arena, &scope, &code, 0, NULL);

  EnterBlock(arena, &block_stack, entry_name, program->main->loc, &scope);
  FbleTc* tc = FbleTypeCheck(arena, program);
  if (tc != NULL) {
    CompileExpr(arena, &block_stack, true, &scope, tc);
    FbleFreeTc(arena, tc);
  }
  ExitBlock(arena, &block_stack, &scope, true);
  FbleFreeString(arena, entry_name.name);
  FreeScope(arena, &scope, true);

  assert(block_stack.stack.size == 0);
  FbleFree(arena, block_stack.stack.xs);

  if (profiling_disabled) {
    FbleFreeProfile(arena, profile);
  }

  if (tc == NULL) {
    FbleFreeInstrBlock(arena, code);
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleAlloc(arena, FbleCompiledProgram);
  compiled->code = code;
  return compiled;
}

// FbleFreeCompiledProgram -- see documentation in fble.h
void FbleFreeCompiledProgram(FbleArena* arena, FbleCompiledProgram* program)
{
  FbleFreeInstrBlock(arena, program->code);
  FbleFree(arena, program);
}
