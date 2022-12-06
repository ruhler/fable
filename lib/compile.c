// compile.c --
//   This file describes code to compile fble values into fble instructions.

#include <fble/fble-compile.h>

#include <assert.h>   // for assert
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>     // for FbleAlloc, etc.
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

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
  FbleDebugInfo* pending_debug_info;
  FbleProfileOp* pending_profile_ops;
  struct Scope* parent;
} Scope;

static Local* NewLocal(Scope* scope);
static void ReleaseLocal(Scope* scope, Local* local, bool exit);
static void PushVar(Scope* scope, FbleName name, Local* local);
static void PopVar(Scope* scope, bool exit);
static Local* GetVar(Scope* scope, FbleVarIndex index);
static void SetVar(Scope* scope, size_t index, FbleName name, Local* local);

static void InitScope(Scope* scope, FbleCode** code, size_t args, size_t statics, FbleBlockId block, Scope* parent);
static void FreeScope(Scope* scope);
static void AppendInstr(Scope* scope, FbleInstr* instr);
static void AppendDebugInfo(Scope* scope, FbleDebugInfo* info);
static void AppendProfileOp(Scope* scope, FbleProfileOpTag tag, FbleBlockId block);

// Blocks --
//   A stack of block frames tracking the current block for profiling.
//
// Fields:
//   stack - The stack of black frames representing the current location.
//   profile - The names of profile blocks to append to.
typedef struct {
  FbleBlockIdV stack;
  FbleNameV profile;
} Blocks;

static FbleBlockId PushBlock(Blocks* blocks, FbleName name, FbleLoc loc);
static FbleBlockId PushBodyBlock(Blocks* blocks, FbleLoc loc);
static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope, bool replace);
static void PopBlock(Blocks* blocks);
static void ExitBlock(Blocks* blocks, Scope* scope, bool exit);

static void CompileExit(bool exit, Scope* scope, Local* result);
static Local* CompileExpr(Blocks* blocks, bool stmt, bool exit, Scope* scope, FbleTc* tc);
static FbleCode* Compile(FbleNameV args, FbleTc* tc, FbleName name, FbleNameV* profile_blocks);
static FbleCompiledModule* CompileModule(FbleLoadedModule* module, FbleTc* tc);

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
//   be freed with ReleaseLocal when no longer in use.
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
    scope->code->_base.num_locals = scope->locals.size;
  }

  Local* local = FbleAlloc(Local);
  local->index.section = FBLE_LOCALS_FRAME_SECTION;
  local->index.index = index;
  local->refcount = 1;

  scope->locals.xs[index] = local;
  return local;
}

// ReleaseLocal --
//   Decrement the reference count on a local and free it if appropriate.
//
// Inputs:
//   scope - the scope that the local belongs to
//   local - the local to release. May be NULL.
//   exit - whether the stack frame has already been exited or not.
//
// Side effects:
//   Decrements the reference count on the local and frees it if the refcount
//   drops to 0. Emits a RELEASE instruction if this is the last reference the
//   local and the stack frame hasn't already been exited.
static void ReleaseLocal(Scope* scope, Local* local, bool exit)
{
  if (local == NULL) {
    return;
  }

  local->refcount--;
  if (local->refcount == 0) {
    if (!exit) {
      FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
      release_instr->target = local->index.index;
      AppendInstr(scope, &release_instr->_base);
    }

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
//   name - the name of the variable. Borrowed.
//   local - the local address of the variable. Consumed.
//
// Results:
//   None.
//
// Side effects:
//   Pushes a new variable onto the scope. Takes ownership of the given local,
//   which will be released when the variable is freed by a call to PopVar or
//   FreeScope.
static void PushVar(Scope* scope, FbleName name, Local* local)
{
  if (local != NULL) {
    FbleVarDebugInfo* info = FbleAlloc(FbleVarDebugInfo);
    info->_base.tag = FBLE_VAR_DEBUG_INFO;
    info->_base.next = NULL;
    info->var = FbleCopyName(name);
    info->index = local->index;
    AppendDebugInfo(scope, &info->_base);
  }

  FbleVectorAppend(scope->vars, local);
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   scope - the scope to pop from.
//   exit - whether the stack frame has already been exited or not.
//
// Side effects:
//   Pops the top var off the scope.
static void PopVar(Scope* scope, bool exit)
{
  scope->vars.size--;
  Local* var = scope->vars.xs[scope->vars.size];
  ReleaseLocal(scope, var, exit);
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
//   name - the name of the variable.
//   local - the new value for the local variable
//
// Side effects:
// * Frees the existing local value of the variable.
// * Sets the value of the variable to the given local.
// * Takes ownership of the given local.
static void SetVar(Scope* scope, size_t index, FbleName name, Local* local)
{
  assert(index < scope->vars.size);
  ReleaseLocal(scope, scope->vars.xs[index], false);
  scope->vars.xs[index] = local;

  FbleVarDebugInfo* info = FbleAlloc(FbleVarDebugInfo);
  info->_base.tag = FBLE_VAR_DEBUG_INFO;
  info->_base.next = NULL;
  info->var = FbleCopyName(name);
  info->index = local->index;
  AppendDebugInfo(scope, &info->_base);
}

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   scope - the scope to initialize.
//   code - a pointer to store the allocated code block for this scope.
//   args - the number of arguments to the function the scope is for.
//   statics - the number of statics captured by the scope (??).
//   block - the profile block id to enter when executing this scope.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Side effects:
// * Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetimes of the code block
//   and the parent scope must exceed the lifetime of this scope.
// * The caller is responsible for calling FbleFreeCode on *code when it
//   is no longer needed.
static void InitScope(Scope* scope, FbleCode** code, size_t args, size_t statics, FbleBlockId block, Scope* parent)
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

  scope->code = FbleNewCode(args, statics, 0, block);
  scope->pending_debug_info = NULL;
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
  FbleVectorFree(scope->statics);

  while (scope->vars.size > 0) {
    PopVar(scope, true);
  }
  FbleVectorFree(scope->vars);

  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] != NULL) {
      FbleFree(scope->locals.xs[i]);
    }
  }
  FbleVectorFree(scope->locals);
  FbleFreeDebugInfo(scope->pending_debug_info);

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
  assert(instr->debug_info == NULL);
  instr->debug_info = scope->pending_debug_info;
  scope->pending_debug_info = NULL;

  assert(instr->profile_ops == NULL);
  instr->profile_ops = scope->pending_profile_ops;
  scope->pending_profile_ops = NULL;

  FbleVectorAppend(scope->code->instrs, instr);
}

// AppendDebugInfo --
//   Append a single debug info entry to the code block for the given scope.
//
// Inputs:
//   scope - the scope to append the instruction to.
//   info - the debug info entry whose next field must be NULL. Consumed.
//
// Side effects:
//   Appends the debug info to the code block for the given scope, taking
//   ownership of the allocated debug info object.
static void AppendDebugInfo(Scope* scope, FbleDebugInfo* info)
{
  assert(info->next == NULL);
  if (scope->pending_debug_info == NULL) {
    scope->pending_debug_info = info;
  } else {
    FbleDebugInfo* curr = scope->pending_debug_info;
    while (curr->next != NULL) {
      curr = curr->next;
    }
    curr->next = info;
  } 
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

// PushBlock --
//   Push a new profiling block onto the block stack.
//
// Inputs:
//   blocks - the blocks stack.
//   name - name to add to the current block path for naming the new block.
//   loc - the location of the block.
//
// Results:
//   The id of the newly pushed profiling block.
//
// Side effects:
// * Pushes a new block to the blocks stack.
// * The block should be popped from the stack using ExitBlock or one of the
//   other functions that exit a block when no longer needed.
static FbleBlockId PushBlock(Blocks* blocks, FbleName name, FbleLoc loc)
{
  const char* curr = NULL;
  size_t currlen = 0;
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile.xs[curr_id].name->str;
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
  size_t id = blocks->profile.size;
  FbleVectorAppend(blocks->profile, nm);
  FbleVectorAppend(blocks->stack, id);
  return id;
}

// PushBodyBlock --
//   Add a new body profiling block to the block stack. This is used for the
//   body of functions and processes that are executed when they are called,
//   not when they are defined.
//
// Inputs:
//   blocks - the blocks stack.
//   loc - The location of the new block.
//
// Returns:
//   The id of the newly pushed block.
//
// Side effects:
// * Adds a new block to the blocks stack.
// * The block should be popped from the stack using ExitBlock or one of the
//   other functions that exit a block when no longer needed.
static FbleBlockId PushBodyBlock(Blocks* blocks, FbleLoc loc)
{
  const char* curr = "";
  if (blocks->stack.size > 0) {
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1];
    curr = blocks->profile.xs[curr_id].name->str;
  }

  // Append "!" to the current block name to figure out the new block name.
  FbleString* str = FbleAllocExtra(FbleString, strlen(curr) + 2);
  str->refcount = 1;
  str->magic = FBLE_STRING_MAGIC;
  str->str[0] = '\0';
  strcat(str->str, curr);
  strcat(str->str, "!");

  FbleName nm = { .name = str, .loc = FbleCopyLoc(loc) };
  size_t id = blocks->profile.size;
  FbleVectorAppend(blocks->profile, nm);
  FbleVectorAppend(blocks->stack, id);
  return id;
}

// EnterBlock --
//   Enter a new profiling block.
//
// Inputs:
//   blocks - the blocks stack.
//   name - name to add to the current block path for naming the new block.
//   loc - the location of the block.
//   scope - where to add the ENTER_BLOCK instruction to.
//   replace - if true, emit a REPLACE_BLOCK instruction instead of ENTER_BLOCK.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock.
static void EnterBlock(Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope, bool replace)
{
  FbleBlockId id = PushBlock(blocks, name, loc);
  AppendProfileOp(scope, replace ? FBLE_PROFILE_REPLACE_OP : FBLE_PROFILE_ENTER_OP, id);
}

// PopBlock --
//   Pop the current profiling block frame.
//
// Inputs:
//   blocks - the blocks stack.
//
// Side effects:
//   Pops the top block frame off the blocks stack.
static void PopBlock(Blocks* blocks)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;
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
//   Pops the top block frame off the blocks stack.
//   profile exit op to the scope if exit is false.
static void ExitBlock(Blocks* blocks, Scope* scope, bool exit)
{
  PopBlock(blocks);
  if (!exit) {
    AppendProfileOp(scope, FBLE_PROFILE_EXIT_OP, 0);
  }
}

// CompileExit --
//   If exit is true, appends a return instruction to instrs.
//
// Inputs:
//   blocks - the blocks stack.
//   exit - whether we actually want to exit.
//   scope - the scope to append the instructions to.
//   result - the result to return when exiting. May be NULL.
//
// Side effects:
//   If exit is true, appends a return instruction to instrs
static void CompileExit(bool exit, Scope* scope, Local* result)
{
  if (exit && result != NULL) {
    // Release any remaining local variables before returning.
    for (size_t i = 0; i < scope->locals.size; ++i) {
      Local* local = scope->locals.xs[i];
      if (local != NULL && local != result) {
        FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
        release_instr->target = local->index.index;
        AppendInstr(scope, &release_instr->_base);
      }
    }

    FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
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
//   stmt - true if this marks the beginning of a statement, for debug
//     purposes.
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
// * The caller should call ReleaseLocal when the returned results are no
//   longer needed. Note that FreeScope calls ReleaseLocal for all locals
//   allocated to the scope, so that can also be used to clean up the local.
static Local* CompileExpr(Blocks* blocks, bool stmt, bool exit, Scope* scope, FbleTc* v)
{
  if (stmt) {
    FbleStatementDebugInfo* info = FbleAlloc(FbleStatementDebugInfo);
    info->_base.tag = FBLE_STATEMENT_DEBUG_INFO;
    info->_base.next = NULL;
    info->loc = FbleCopyLoc(v->loc);
    AppendDebugInfo(scope, &info->_base);
  }

  switch (v->tag) {
    case FBLE_TYPE_VALUE_TC: {
      Local* local = NewLocal(scope);
      FbleTypeInstr* instr = FbleAllocInstr(FbleTypeInstr, FBLE_TYPE_INSTR);
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
          FbleRefValueInstr* ref_instr = FbleAllocInstr(FbleRefValueInstr, FBLE_REF_VALUE_INSTR);
          ref_instr->dest = vars[i]->index.index;
          AppendInstr(scope, &ref_instr->_base);
        }
        PushVar(scope, let_tc->bindings.xs[i].name, vars[i]);
      }

      // Compile the values of the variables.
      Local* defs[let_tc->bindings.size];
      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        EnterBlock(blocks, let_tc->bindings.xs[i].name, let_tc->bindings.xs[i].loc, scope, false);
        defs[i] = CompileExpr(blocks, false, false, scope, let_tc->bindings.xs[i].tc);
        ExitBlock(blocks, scope, false);
      }

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        if (let_tc->recursive) {
          FbleRefDefInstr* ref_def_instr = FbleAllocInstr(FbleRefDefInstr, FBLE_REF_DEF_INSTR);
          ref_def_instr->loc = FbleCopyLoc(let_tc->bindings.xs[i].name.loc);
          ref_def_instr->ref = vars[i]->index.index;
          ref_def_instr->value = defs[i]->index;
          AppendInstr(scope, &ref_def_instr->_base);
        }
        SetVar(scope, base_index + i, let_tc->bindings.xs[i].name, defs[i]);
      }

      Local* body = CompileExpr(blocks, true, exit, scope, let_tc->body);

      for (size_t i = 0; i < let_tc->bindings.size; ++i) {
        PopVar(scope, exit);
      }

      return body;
    }

    case FBLE_STRUCT_VALUE_TC: {
      FbleStructValueTc* struct_tc = (FbleStructValueTc*)v;

      size_t argc = struct_tc->fieldc;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, false, scope, struct_tc->fields[i]);
      }

      Local* local = NewLocal(scope);
      FbleStructValueInstr* struct_instr = FbleAllocInstr(FbleStructValueInstr, FBLE_STRUCT_VALUE_INSTR);
      struct_instr->dest = local->index.index;
      FbleVectorInit(struct_instr->args);
      AppendInstr(scope, &struct_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(struct_instr->args, args[i]->index);
        ReleaseLocal(scope, args[i], exit);
      }

      return local;
    }

    case FBLE_UNION_VALUE_TC: {
      FbleUnionValueTc* union_tc = (FbleUnionValueTc*)v;
      Local* arg = CompileExpr(blocks, false, false, scope, union_tc->arg);

      Local* local = NewLocal(scope);
      FbleUnionValueInstr* union_instr = FbleAllocInstr(FbleUnionValueInstr, FBLE_UNION_VALUE_INSTR);
      union_instr->tag = union_tc->tag;
      union_instr->arg = arg->index;
      union_instr->dest = local->index.index;
      AppendInstr(scope, &union_instr->_base);
      CompileExit(exit, scope, local);
      ReleaseLocal(scope, arg, exit);
      return local;
    }

    case FBLE_UNION_SELECT_TC: {
      FbleUnionSelectTc* select_tc = (FbleUnionSelectTc*)v;
      Local* condition = CompileExpr(blocks, false, false, scope, select_tc->condition);

      FbleUnionSelectInstr* select_instr = FbleAllocInstr(FbleUnionSelectInstr, FBLE_UNION_SELECT_INSTR);
      select_instr->loc = FbleCopyLoc(select_tc->_base.loc);
      select_instr->condition = condition->index;
      FbleVectorInit(select_instr->jumps);
      AppendInstr(scope, &select_instr->_base);

      size_t select_instr_pc = scope->code->instrs.size;
      size_t branch_offsets[select_tc->choices.size];
      Local* target = exit ? NULL : NewLocal(scope);
      FbleJumpInstr* exit_jumps[select_tc->choices.size];

      for (size_t i = 0; i < select_tc->choices.size; ++i) {
        exit_jumps[i] = NULL;

        // Check if we have already generated the code for this branch.
        bool already_done = false;
        for (size_t j = 0; j < i; ++j) {
          if (select_tc->choices.xs[i].tc == select_tc->choices.xs[j].tc) {
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
          EnterBlock(blocks, select_tc->choices.xs[i].name, select_tc->choices.xs[i].loc, scope, exit);
          Local* result = CompileExpr(blocks, true, exit, scope, select_tc->choices.xs[i].tc);

          if (!exit) {
            FbleCopyInstr* copy = FbleAllocInstr(FbleCopyInstr, FBLE_COPY_INSTR);
            copy->source = result->index;
            copy->dest = target->index.index;
            AppendInstr(scope, &copy->_base);
          }
          ExitBlock(blocks, scope, exit);

          ReleaseLocal(scope, result, exit);

          if (!exit) {
            exit_jumps[i] = FbleAllocInstr(FbleJumpInstr, FBLE_JUMP_INSTR);
            exit_jumps[i]->count = scope->code->instrs.size + 1;
            AppendInstr(scope, &exit_jumps[i]->_base);
          }
        }

        FbleVectorAppend(select_instr->jumps, branch_offsets[i]);
      }

      // Fix up exit jumps now that all the branch code is generated.
      if (!exit) {
        for (size_t i = 0; i < select_tc->choices.size; ++i) {
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
      ReleaseLocal(scope, condition, exit);
      return target;
    }

    case FBLE_DATA_ACCESS_TC: {
      FbleDataAccessTc* access_tc = (FbleDataAccessTc*)v;
      Local* obj = CompileExpr(blocks, false, false, scope, access_tc->obj);

      FbleInstrTag tag;
      if (access_tc->datatype == FBLE_STRUCT_DATATYPE) {
        tag = FBLE_STRUCT_ACCESS_INSTR;
      } else {
        assert(access_tc->datatype == FBLE_UNION_DATATYPE);
        tag = FBLE_UNION_ACCESS_INSTR;
      }

      FbleAccessInstr* access = FbleAllocInstr(FbleAccessInstr, tag);
      access->obj = obj->index;
      access->tag = access_tc->tag;
      access->loc = FbleCopyLoc(access_tc->loc);
      AppendInstr(scope, &access->_base);

      Local* local = NewLocal(scope);
      access->dest = local->index.index;
      CompileExit(exit, scope, local);
      ReleaseLocal(scope, obj, exit);
      return local;
    }

    case FBLE_FUNC_VALUE_TC: {
      FbleFuncValueTc* func_tc = (FbleFuncValueTc*)v;
      size_t argc = func_tc->args.size;

      FbleFuncValueInstr* instr = FbleAllocInstr(FbleFuncValueInstr, FBLE_FUNC_VALUE_INSTR);

      FbleVectorInit(instr->scope);
      for (size_t i = 0; i < func_tc->scope.size; ++i) {
        Local* local = GetVar(scope, func_tc->scope.xs[i]);
        FbleVectorAppend(instr->scope, local->index);
      }

      Scope func_scope;
      FbleBlockId scope_block = PushBodyBlock(blocks, func_tc->body_loc);
      InitScope(&func_scope, &instr->code, argc, func_tc->scope.size, scope_block, scope);

      for (size_t i = 0; i < argc; ++i) {
        Local* local = NewLocal(&func_scope);
        PushVar(&func_scope, func_tc->args.xs[i], local);
      }

      Local* func_result = CompileExpr(blocks, true, true, &func_scope, func_tc->body);
      ExitBlock(blocks, &func_scope, true);
      ReleaseLocal(&func_scope, func_result, true);
      FreeScope(&func_scope);

      Local* local = NewLocal(scope);
      instr->dest = local->index.index;
      AppendInstr(scope, &instr->_base);
      CompileExit(exit, scope, local);
      return local;
    }

    case FBLE_FUNC_APPLY_TC: {
      FbleFuncApplyTc* apply_tc = (FbleFuncApplyTc*)v;
      Local* func = CompileExpr(blocks, false, false, scope, apply_tc->func);

      size_t argc = apply_tc->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, false, scope, apply_tc->args.xs[i]);
      }

      if (exit) {
        // Release any remaining unused locals before tail calling.
        for (size_t i = 0; i < scope->locals.size; ++i) {
          Local* local = scope->locals.xs[i];
          if (local != NULL) {
            bool used = (local == func);
            for (size_t j = 0; !used && j < argc; ++j) {
              used = (local == args[j]);
            }

            if (!used) {
              FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
              release_instr->target = local->index.index;
              AppendInstr(scope, &release_instr->_base);
            }
          }
        }
      }

      Local* dest = exit ? NULL : NewLocal(scope);

      FbleCallInstr* call_instr = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
      call_instr->loc = FbleCopyLoc(apply_tc->_base.loc);
      call_instr->exit = exit;
      call_instr->func = func->index;
      FbleVectorInit(call_instr->args);
      call_instr->dest = exit ? 0 : dest->index.index;
      AppendInstr(scope, &call_instr->_base);

      ReleaseLocal(scope, func, exit);
      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(call_instr->args, args[i]->index);
        ReleaseLocal(scope, args[i], exit);
      }

      return dest;
    }

    case FBLE_LIST_TC: {
      FbleListTc* list_tc = (FbleListTc*)v;

      size_t argc = list_tc->fieldc;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(blocks, false, false, scope, list_tc->fields[i]);
      }

      Local* local = NewLocal(scope);
      FbleListInstr* list_instr = FbleAllocInstr(FbleListInstr, FBLE_LIST_INSTR);
      list_instr->dest = local->index.index;
      FbleVectorInit(list_instr->args);
      AppendInstr(scope, &list_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(list_instr->args, args[i]->index);
        ReleaseLocal(scope, args[i], exit);
      }

      return local;
    }

    case FBLE_LITERAL_TC: {
      FbleLiteralTc* literal_tc = (FbleLiteralTc*)v;

      Local* local = NewLocal(scope);
      FbleLiteralInstr* literal_instr = FbleAllocInstr(FbleLiteralInstr, FBLE_LITERAL_INSTR);
      literal_instr->dest = local->index.index;
      FbleVectorInit(literal_instr->letters);
      AppendInstr(scope, &literal_instr->_base);
      CompileExit(exit, scope, local);

      for (size_t i = 0; i < literal_tc->letterc; ++i) {
        FbleVectorAppend(literal_instr->letters, literal_tc->letters[i]);
      }

      return local;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

/**
 * Compiles a type-checked expression.
 *
 * @param args            Local variables to reserve for arguments.
 * @param tc              The type-checked expression to compile.
 * @param name            The name of the expression to use in profiling. Borrowed.
 * @param profile_blocks  Uninitialized vector to store profile blocks to.
 *
 * @returns The compiled program.
 *
 * @sideeffects
 * * Initializes and fills profile_blocks vector. The caller should free the
 *   elements and vector itself when no longer needed.
 * * Adds blocks to the given profile.
 * * The caller should call FbleFreeCode to release resources
 *   associated with the returned program when it is no longer needed.
 */
static FbleCode* Compile(FbleNameV args, FbleTc* tc, FbleName name, FbleNameV* profile_blocks)
{
  Blocks blocks;
  FbleVectorInit(blocks.stack);
  FbleVectorInit(blocks.profile);

  FbleCode* code;
  Scope scope;
  FbleBlockId scope_block = PushBlock(&blocks, name, name.loc);
  InitScope(&scope, &code, args.size, 0, scope_block, NULL);

  for (size_t i = 0; i < args.size; ++i) {
    Local* local = NewLocal(&scope);
    PushVar(&scope, args.xs[i], local);
  }

  CompileExpr(&blocks, true, true, &scope, tc);
  ExitBlock(&blocks, &scope, true);

  FreeScope(&scope);
  assert(blocks.stack.size == 0);
  FbleVectorFree(blocks.stack);
  *profile_blocks = blocks.profile;
  return code;
}

// CompileModule --
//   Compile a single module.
//
// Inputs:
//   module - meta info about the module and its dependencies.
//   tc - the typechecked value of the module.
//
// Results:
//   The compiled module.
//
// Side effects:
//   The user should call FbleFreeCompiledModule on the resulting module when
//   it is no longer needed.
static FbleCompiledModule* CompileModule(FbleLoadedModule* module, FbleTc* tc)
{
  FbleCompiledModule* compiled = FbleAlloc(FbleCompiledModule);
  compiled->path = FbleCopyModulePath(module->path);
  FbleVectorInit(compiled->deps);
  FbleNameV args;
  FbleVectorInit(args);
  for (size_t d = 0; d < module->deps.size; ++d) {
    FbleVectorAppend(compiled->deps, FbleCopyModulePath(module->deps.xs[d]));
    FbleVectorAppend(args, FbleModulePathName(module->deps.xs[d]));
  }

  FbleName label = FbleModulePathName(module->path);
  compiled->code = Compile(args, tc, label, &compiled->profile_blocks);
  for (size_t i = 0; i < args.size; ++i) {
    FbleFreeName(args.xs[i]);
  }
  FbleVectorFree(args);
  FbleFreeName(label);
  return compiled;
}

// FbleFreeCompiledModule -- see documentation in fble-compile.h
void FbleFreeCompiledModule(FbleCompiledModule* module)
{
  FbleFreeModulePath(module->path);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleFreeModulePath(module->deps.xs[i]);
  }
  FbleVectorFree(module->deps);
  FbleFreeCode(module->code);
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleFreeName(module->profile_blocks.xs[i]);
  }
  FbleVectorFree(module->profile_blocks);

  FbleFree(module);
}

// FbleFreeCompiledProgram -- see documentation in fble-compile.h
void FbleFreeCompiledProgram(FbleCompiledProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeCompiledModule(program->modules.xs[i]);
    }
    FbleVectorFree(program->modules);
    FbleFree(program);
  }
}

// FbleCompileModule -- see documentation in fble-compile.h
FbleCompiledModule* FbleCompileModule(FbleLoadedProgram* program)
{
  FbleTc* tc = FbleTypeCheckModule(program);
  if (!tc) {
    return NULL;
  }

  assert(program->modules.size > 0);
  FbleLoadedModule* module = program->modules.xs + program->modules.size - 1;
  FbleCompiledModule* compiled = CompileModule(module, tc);

  FbleFreeTc(tc);
  return compiled;
}

// FbleCompileProgram -- see documentation in fble-compile.h
FbleCompiledProgram* FbleCompileProgram(FbleLoadedProgram* program)
{
  FbleTc** typechecked = FbleTypeCheckProgram(program);
  if (!typechecked) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleAlloc(FbleCompiledProgram);
  FbleVectorInit(compiled->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleLoadedModule* module = program->modules.xs + i;
    FbleVectorAppend(compiled->modules, CompileModule(module, typechecked[i]));
    FbleFreeTc(typechecked[i]);
  }
  FbleFree(typechecked);
  return compiled;
}
