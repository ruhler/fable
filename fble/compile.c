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

// Var --
//   Information about a variable visible during compilation.
//
// name - the name of the variable.
// local - the type and location of the variable in the stack frame.
//   A reference to the local is owned by this Var.
typedef struct {
  FbleName name;
  Local* local;
} Var;

// VarV --
//   A vector of pointers to vars.
typedef struct {
  size_t size;
  Var** xs;
} VarV;

// Scope --
//   Scope of variables visible during compilation.
//
// Fields:
//   statics - variables captured from the parent scope.
//     Takes ownership of the Vars.
//   vars - stack of local variables in scope order.
//     Takes ownership of the Vars.
//   locals - local values. Entries may be NULL to indicate a free slot.
//     Owns the Local.
//   code - the instruction block for this scope.
//   pending_profile_ops - profiling ops to associated with the next
//                         instruction added.
//   capture - A vector of variables captured from the parent scope.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  VarV statics;
  VarV vars;
  LocalV locals;
  FbleInstrBlock* code;
  FbleProfileOp* pending_profile_ops;
  FbleFrameIndexV* capture;
  struct Scope* parent;
} Scope;

static Local* NewLocal(FbleArena* arena, Scope* scope);
static void LocalRelease(FbleArena* arena, Scope* scope, Local* local, bool exit);
static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, Local* local);
static void PopVar(FbleArena* arena, Scope* scope, bool exit);
static Var* GetVar(FbleArena* arena, Scope* scope, FbleName name);

static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock** code, FbleFrameIndexV* capture, Scope* parent);
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
static Local* CompileExpr(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr);
static Local* CompileList(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleLoc loc, FbleExprV args);
static Local* CompileExec(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr);
static void CompileProgram(FbleArena* arena, Blocks* blocks, Scope* scope, FbleProgram* prgm);

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
//   local - the local to release.
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
//   name - the name of the variable.
//   local - the value of the variable.
//
// Results:
//   A pointer to the newly pushed variable. The pointer is owned by the
//   scope. It remains valid until a corresponding PopVar or FreeScope
//   occurs.
//
// Side effects:
//   Pushes a new variable with given name and type onto the scope. Takes
//   ownership of the given local, which will be released when the variable is
//   freed.
static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, Local* local)
{
  Var* var = FbleAlloc(arena, Var);
  var->name = name;
  var->local = local;
  FbleVectorAppend(arena, scope->vars, var);
  return var;
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
//   Pops the top var off the scope. Invalidates the pointer to the variable
//   originally returned in PushVar.
static void PopVar(FbleArena* arena, Scope* scope, bool exit)
{
  scope->vars.size--;
  Var* var = scope->vars.xs[scope->vars.size];
  LocalRelease(arena, scope, var->local, exit);
  FbleFree(arena, var);
}

// GetVar --
//   Lookup a var in the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to look in.
//   name - the name of the variable.
//
// Result:
//   The variable from the scope, or NULL if no such variable was found. The
//   variable is owned by the scope and remains valid until either PopVar is
//   called or the scope is finished.
//
// Side effects:
//   Marks variable as used and for capture if necessary.
static Var* GetVar(FbleArena* arena, Scope* scope, FbleName name)
{
  for (size_t i = 0; i < scope->vars.size; ++i) {
    size_t j = scope->vars.size - i - 1;
    Var* var = scope->vars.xs[j];
    if (FbleNamesEqual(name, var->name)) {
      return var;
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    Var* var = scope->statics.xs[i];
    if (FbleNamesEqual(name, var->name)) {
      return var;
    }
  }

  if (scope->parent != NULL) {
    Var* var = GetVar(arena, scope->parent, name);
    if (var != NULL) {
      Local* local = FbleAlloc(arena, Local);
      local->index.section = FBLE_STATICS_FRAME_SECTION;
      local->index.index = scope->statics.size;
      local->refcount = 1;

      Var* captured = FbleAlloc(arena, Var);
      captured->name = var->name;
      captured->local = local;

      FbleVectorAppend(arena, scope->statics, captured);
      if (scope->statics.size > scope->code->statics) {
        scope->code->statics = scope->statics.size;
      }
      FbleVectorAppend(arena, *scope->capture, var->local->index);
      return captured;
    }
  }

  UNREACHABLE("Vars should have been resolved at type check");
  return NULL;
}

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to initialize.
//   code - a pointer to store the allocated code block for this scope.
//   capture - vector to store capture variables from the parent scope in.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Initializes scope based on parent. FreeScope should be
//   called to free the allocations for scope. The lifetimes of the code block
//   and the parent scope must exceed the lifetime of this scope.
//   Initializes capture and appends to it the variables that need to be
//   captured from the parent scope.
//   The caller is responsible for calling FbleFreeInstrBlock on *code when it
//   is no longer needed.
static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock** code, FbleFrameIndexV* capture, Scope* parent)
{
  FbleVectorInit(arena, scope->statics);
  FbleVectorInit(arena, scope->vars);
  FbleVectorInit(arena, scope->locals);
  FbleVectorInit(arena, *capture);

  scope->code = FbleAlloc(arena, FbleInstrBlock);
  scope->code->refcount = 1;
  scope->code->magic = FBLE_INSTR_BLOCK_MAGIC;
  scope->code->statics = 0;
  scope->code->locals = 0;
  FbleVectorInit(arena, scope->code->instrs);
  scope->code->statics = 0;
  scope->code->locals = 0;
  scope->pending_profile_ops = NULL;
  scope->capture = capture;
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
    FbleFree(arena, scope->statics.xs[i]->local);
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
//   expr - the expression to compile.
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
static Local* CompileExpr(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr)
{
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      Local* local = NewLocal(arena, scope);
      FbleTypeInstr* instr = FbleAlloc(arena, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      instr->_base.profile_ops = NULL;
      instr->dest = local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_MISC_APPLY_EXPR: {
      UNREACHABLE("Unresolved misc apply expr");
      return NULL;
    }

    case FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;

      size_t argc = apply_expr->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(arena, blocks, false, scope, apply_expr->args.xs[i]);
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

    case FBLE_FUNC_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;
      Local* misc = CompileExpr(arena, blocks, false, scope, apply_expr->misc);

      size_t argc = apply_expr->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(arena, blocks, false, scope, apply_expr->args.xs[i]);
      }

      if (exit) {
        AppendProfileOp(arena, scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      Local* dest = NewLocal(arena, scope);

      FbleCallInstr* call_instr = FbleAlloc(arena, FbleCallInstr);
      call_instr->_base.tag = FBLE_CALL_INSTR;
      call_instr->_base.profile_ops = NULL;
      call_instr->loc = FbleCopyLoc(apply_expr->misc->loc);
      call_instr->exit = exit;
      call_instr->func = misc->index;
      FbleVectorInit(arena, call_instr->args);
      call_instr->dest = dest->index.index;
      AppendInstr(arena, scope, &call_instr->_base);

      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(arena, call_instr->args, args[i]->index);
        LocalRelease(arena, scope, args[i], call_instr->exit);
      }

      return dest;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;

      size_t argc = struct_expr->args.size;
      Local* args[argc];
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        EnterBlock(arena, blocks, arg->name, arg->expr->loc, scope);
        args[j] = CompileExpr(arena, blocks, false, scope, arg->expr);
        ExitBlock(arena, blocks, scope, false);
      }

      Local* local = NewLocal(arena, scope);
      FbleStructValueInstr* struct_instr = FbleAlloc(arena, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->_base.profile_ops = NULL;
      struct_instr->dest = local->index.index;
      AppendInstr(arena, scope, &struct_instr->_base);
      CompileExit(arena, exit, scope, local);

      FbleVectorInit(arena, struct_instr->args);
      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(arena, struct_instr->args, args[i]->index);
        LocalRelease(arena, scope, args[i], exit);
      }
      return local;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Local* arg = CompileExpr(arena, blocks, false, scope, union_value_expr->arg);

      Local* local = NewLocal(arena, scope);
      FbleUnionValueInstr* union_instr = FbleAlloc(arena, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->_base.profile_ops = NULL;
      union_instr->tag = union_value_expr->field.tag;
      union_instr->arg = arg->index;
      union_instr->dest = local->index.index;
      AppendInstr(arena, scope, &union_instr->_base);
      CompileExit(arena, exit, scope, local);
      LocalRelease(arena, scope, arg, exit);
      return local;
    }

    case FBLE_STRUCT_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;
      Local* obj = CompileExpr(arena, blocks, false, scope, access_expr->object);

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->_base.profile_ops = NULL;
      access->loc = FbleCopyLoc(access_expr->field.name.loc);
      access->obj = obj->index;
      access->tag = access_expr->field.tag;
      AppendInstr(arena, scope, &access->_base);

      Local* local = NewLocal(arena, scope);
      access->dest = local->index.index;
      CompileExit(arena, exit, scope, local);
      LocalRelease(arena, scope, obj, exit);
      return local;
    }

    case FBLE_UNION_ACCESS_EXPR: {
      FbleAccessExpr* access_expr = (FbleAccessExpr*)expr;
      Local* obj = CompileExpr(arena, blocks, false, scope, access_expr->object);

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = FBLE_UNION_ACCESS_INSTR;
      access->_base.profile_ops = NULL;
      access->loc = FbleCopyLoc(access_expr->field.name.loc);
      access->obj = obj->index;
      AppendInstr(arena, scope, &access->_base);

      access->tag = access_expr->field.tag;
      Local* local = NewLocal(arena, scope);
      access->dest = local->index.index;
      CompileExit(arena, exit, scope, local);
      LocalRelease(arena, scope, obj, exit);
      return local;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      UNREACHABLE("Unresolved MISC_ACCESS_EXPR");
      return NULL;
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;
      Local* condition = CompileExpr(arena, blocks, false, scope, select_expr->condition);

      if (exit) {
        AppendProfileOp(arena, scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(arena, FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->_base.profile_ops = NULL;
      select_instr->loc = FbleCopyLoc(select_expr->condition->loc);
      select_instr->condition = condition->index;
      FbleVectorInit(arena, select_instr->jumps);
      AppendInstr(arena, scope, &select_instr->_base);

      size_t select_instr_pc = scope->code->instrs.size;

      Local* target = NULL;
      FbleJumpInstr* exit_jump_default = NULL;
      if (select_expr->default_ != NULL) {
        FbleName name = {
          .name = FbleNewString(arena, ":"),
          .loc = select_expr->default_->loc,  // unused.
          .space = FBLE_NORMAL_NAME_SPACE
        };
        EnterBlock(arena, blocks, name, select_expr->default_->loc, scope);
        Local* result = CompileExpr(arena, blocks, exit, scope, select_expr->default_);
        target = NewLocal(arena, scope);

        if (!exit) {
          FbleCopyInstr* copy = FbleAlloc(arena, FbleCopyInstr);
          copy->_base.tag = FBLE_COPY_INSTR;
          copy->_base.profile_ops = NULL;
          copy->source = result->index;
          copy->dest = target->index.index;
          AppendInstr(arena, scope, &copy->_base);
          LocalRelease(arena, scope, result, false);
        }
        ExitBlock(arena, blocks, scope, exit);
        FbleFreeString(arena, name.name);

        if (!exit) {
          exit_jump_default = FbleAlloc(arena, FbleJumpInstr);
          exit_jump_default->_base.tag = FBLE_JUMP_INSTR;
          exit_jump_default->_base.profile_ops = NULL;
          exit_jump_default->count = scope->code->instrs.size + 1;
          AppendInstr(arena, scope, &exit_jump_default->_base);
        }
      }

      // TODO: Make sure type check sets default choices to point to the
      // default expression.
      FbleJumpInstr* exit_jumps[select_expr->choices.size];
      for (size_t i = 0; i < select_expr->choices.size; ++i) {
        if (select_expr->choices.xs[i].expr != NULL) {
          size_t jump = scope->code->instrs.size - select_instr_pc;
          FbleVectorAppend(arena, select_instr->jumps, jump);

          EnterBlock(arena, blocks,
              select_expr->choices.xs[i].name,
              select_expr->choices.xs[i].expr->loc,
              scope);
          Local* result = CompileExpr(arena, blocks, exit, scope, select_expr->choices.xs[i].expr);

          if (target == NULL) {
            target = NewLocal(arena, scope);
          }

          if (!exit) {
            FbleCopyInstr* copy = FbleAlloc(arena, FbleCopyInstr);
            copy->_base.tag = FBLE_COPY_INSTR;
            copy->_base.profile_ops = NULL;
            copy->source = result->index;
            copy->dest = target->index.index;
            AppendInstr(arena, scope, &copy->_base);
          }

          ExitBlock(arena, blocks, scope, exit);
          LocalRelease(arena, scope, result, exit);

          if (!exit) {
            exit_jumps[i] = FbleAlloc(arena, FbleJumpInstr);
            exit_jumps[i]->_base.tag = FBLE_JUMP_INSTR;
            exit_jumps[i]->_base.profile_ops = NULL;
            exit_jumps[i]->count = scope->code->instrs.size + 1;
            AppendInstr(arena, scope, &exit_jumps[i]->_base);
          }
        } else {
          FbleVectorAppend(arena, select_instr->jumps, 0);
        }
      }

      if (!exit) {
        if (exit_jump_default != NULL) {
          exit_jump_default->count = scope->code->instrs.size - exit_jump_default->count;
        }
        for (size_t i = 0; i < select_expr->choices.size; ++i) {
          if (select_expr->choices.xs[i].expr != NULL) {
            exit_jumps[i]->count = scope->code->instrs.size - exit_jumps[i]->count;
          }
        }
      }

      // TODO: We ought to release the condition right after doing goto,
      // otherwise we'll end up unnecessarily holding on to it for the full
      // duration of the block. Technically this doesn't appear to be a
      // violation of the language spec, because it only effects constants in
      // runtime. But we probably ought to fix it anyway.
      LocalRelease(arena, scope, condition, exit);
      return target;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      FbleFuncValueInstr* instr = FbleAlloc(arena, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->_base.profile_ops = NULL;
      instr->argc = argc;

      Scope func_scope;
      InitScope(arena, &func_scope, &instr->code, &instr->scope, scope);
      EnterBodyBlock(arena, blocks, func_value_expr->body->loc, &func_scope);

      for (size_t i = 0; i < argc; ++i) {
        Local* local = NewLocal(arena, &func_scope);
        PushVar(arena, &func_scope, func_value_expr->args.xs[i].name, local);
      }

      Local* func_result = CompileExpr(arena, blocks, true, &func_scope, func_value_expr->body);
      ExitBlock(arena, blocks, &func_scope, true);
      LocalRelease(arena, &func_scope, func_result, true);
      FreeScope(arena, &func_scope, true);

      Local* local = NewLocal(arena, scope);
      instr->dest = local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR: {
      FbleProcValueInstr* instr = FbleAlloc(arena, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->_base.profile_ops = NULL;
      instr->argc = 0;

      Scope body_scope;
      InitScope(arena, &body_scope, &instr->code, &instr->scope, scope);
      EnterBodyBlock(arena, blocks, expr->loc, &body_scope);

      CompileExec(arena, blocks, true, &body_scope, expr);
      ExitBlock(arena, blocks, &body_scope, true);

      Local* local = NewLocal(arena, scope);
      instr->dest = local->index.index;

      FreeScope(arena, &body_scope, true);
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      Var* var = GetVar(arena, scope, var_expr->var);

      // Variables should have been resolved during type check.
      assert(var != NULL && "unresolved variable");

      Local* local = var->local;
      local->refcount++;
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;

      Var* vars[let_expr->bindings.size];
      if (let_expr->recursive) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          Local* local = NewLocal(arena, scope);
          vars[i] = PushVar(arena, scope, let_expr->bindings.xs[i].name, local);
          FbleRefValueInstr* ref_instr = FbleAlloc(arena, FbleRefValueInstr);
          ref_instr->_base.tag = FBLE_REF_VALUE_INSTR;
          ref_instr->_base.profile_ops = NULL;
          ref_instr->dest = local->index.index;
          AppendInstr(arena, scope, &ref_instr->_base);
        }
      }

      // Compile the values of the variables.
      Local* defs[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        EnterBlock(arena, blocks, binding->name, binding->expr->loc, scope);
        defs[i] = CompileExpr(arena, blocks, false, scope, binding->expr);
        ExitBlock(arena, blocks, scope, false);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (let_expr->recursive) {
          FbleRefDefInstr* ref_def_instr = FbleAlloc(arena, FbleRefDefInstr);
          ref_def_instr->_base.tag = FBLE_REF_DEF_INSTR;
          ref_def_instr->_base.profile_ops = NULL;
          ref_def_instr->ref = vars[i]->local->index.index;
          ref_def_instr->value = defs[i]->index;
          AppendInstr(arena, scope, &ref_def_instr->_base);
          LocalRelease(arena, scope, vars[i]->local, false);
          vars[i]->local = defs[i];
        } else {
          PushVar(arena, scope, let_expr->bindings.xs[i].name, defs[i]);
        }
      }

      Local* body = CompileExpr(arena, blocks, exit, scope, let_expr->body);

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(arena, scope, exit);
      }

      return body;
    }

    case FBLE_MODULE_REF_EXPR: {
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      Var* var = GetVar(arena, scope, module_ref_expr->ref.resolved);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");

      Local* local = var->local;
      local->refcount++;
      CompileExit(arena, exit, scope, local);
      return local;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      // TODO: It's a little silly that we are pushing an empty type value
      // here. Oh well. Maybe in the future we'll optimize those away or
      // start making use of the type info at runtime.

      Local* local = NewLocal(arena, scope);
      FbleTypeInstr* type_instr = FbleAlloc(arena, FbleTypeInstr);
      type_instr->_base.tag = FBLE_TYPE_INSTR;
      type_instr->_base.profile_ops = NULL;
      type_instr->dest = local->index.index;
      AppendInstr(arena, scope, &type_instr->_base);

      PushVar(arena, scope, poly->arg.name, local);
      Local* body = CompileExpr(arena, blocks, exit, scope, poly->body);
      PopVar(arena, scope, exit);
      return body;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;
      return CompileExpr(arena, blocks, exit, scope, apply->poly);
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      return CompileList(arena, blocks, exit, scope, expr->loc, list_expr->args);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      Local* spec = CompileExpr(arena, blocks, false, scope, literal->spec);

      size_t n = strlen(literal->word);

      FbleName spec_name = {
        .name = FbleNewString(arena, "__literal_spec"),
        .space = FBLE_NORMAL_NAME_SPACE,
        .loc = literal->spec->loc,
      };
      PushVar(arena, scope, spec_name, spec);

      FbleVarExpr spec_var = {
        ._base = { .tag = FBLE_VAR_EXPR, .loc = literal->spec->loc },
        .var = spec_name
      };

      FbleAccessExpr letters[n];
      FbleExpr* xs[n];
      FbleString* fields[n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        char field_str[2] = { literal->word[i], '\0' };
        fields[i] = FbleNewString(arena, field_str);

        letters[i]._base.tag = FBLE_STRUCT_ACCESS_EXPR;
        letters[i]._base.loc = loc;
        letters[i].object = &spec_var._base;
        letters[i].field.name.name = fields[i];
        letters[i].field.name.space = FBLE_NORMAL_NAME_SPACE;
        letters[i].field.name.loc = loc;
        letters[i].field.tag = literal->tags[i];

        xs[i] = &letters[i]._base;

        if (literal->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      FbleExprV args = { .size = n, .xs = xs, };
      Local* result = CompileList(arena, blocks, exit, scope, literal->word_loc, args);
      PopVar(arena, scope, exit);

      for (size_t i = 0; i < n; ++i) {
        FbleFreeString(arena, fields[i]);
      }
      FbleFreeString(arena, spec_name.name);

      return result;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileList --
//   Compile a list expression. Generates instructions to compute the value of
//   that expression at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   exit - if true, generate instructions to exit the current scope.
//   scope - the list of variables in scope.
//   loc - the location of the list expression.
//   args - the elements of the list expression to compile.
//
// Results:
//   The local of the compiled expression.
//
// Side effects:
//   * Updates blocks with compiled block information.
//   * Appends instructions to scope for executing the given expression.
//     There is no gaurentee about what instructions have been appended to
//     scope if the expression fails to compile.
//   * Allocates a local that must be cleaned up.
//   * Behavior is undefined if there is not at least one list argument.
static Local* CompileList(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleLoc loc, FbleExprV args)
{
  // The goal is to desugar a list expression [a, b, c, d] into the
  // following expression:
  // <@ T@>(T@ x, T@ x1, T@ x2, T@ x3)<@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }<t@>(a, b, c, d)
  assert(args.size > 0 && "empty lists not allowed");
  FbleTypeofExpr typeof_elem = {
    ._base = { .tag = FBLE_TYPEOF_EXPR, .loc = loc },
    .expr = args.xs[0],
  };
  FbleTypeExpr* type = &typeof_elem._base;

  FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
  basic_kind->_base.tag = FBLE_BASIC_KIND;
  basic_kind->_base.loc = FbleCopyLoc(loc);
  basic_kind->_base.refcount = 1;
  basic_kind->level = 1;

  FbleName elem_type_name = {
    .name = FbleNewString(arena, "T"),
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr elem_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = elem_type_name,
  };

  // Generate unique names for the variables x, x0, x1, ...
  size_t num_digits = 0;
  for (size_t x = args.size; x > 0; x /= 10) {
    num_digits++;
  }

  FbleName arg_names[args.size];
  FbleVarExpr arg_values[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    FbleString* name = FbleAllocExtra(arena, FbleString, num_digits + 2);
    name->refcount = 1;
    name->magic = FBLE_STRING_MAGIC;
    name->str[0] = 'x';
    name->str[num_digits+1] = '\0';
    for (size_t j = 0, x = i; j < num_digits; j++, x /= 10) {
      name->str[num_digits - j] = (x % 10) + '0';
    }
    arg_names[i].name = name;
    arg_names[i].space = FBLE_NORMAL_NAME_SPACE;
    arg_names[i].loc = loc;

    arg_values[i]._base.tag = FBLE_VAR_EXPR;
    arg_values[i]._base.loc = loc;
    arg_values[i].var = arg_names[i];
  }

  FbleName list_type_name = {
    .name = FbleNewString(arena, "L"),
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr list_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = list_type_name,
  };

  FbleTaggedTypeExpr inner_args[2];
  FbleName cons_name = {
    .name = FbleNewString(arena, "cons"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr cons = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = cons_name,
  };

  // T@, L@ -> L@
  FbleTypeExpr* cons_arg_types[] = {
    &elem_type._base,
    &list_type._base
  };
  FbleFuncTypeExpr cons_type = {
    ._base = { .tag = FBLE_FUNC_TYPE_EXPR, .loc = loc, },
    .args = { .size = 2, .xs = cons_arg_types },
    .rtype = &list_type._base,
  };

  inner_args[0].type = &cons_type._base;
  inner_args[0].name = cons_name;

  FbleName nil_name = {
    .name = FbleNewString(arena, "nil"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr nil = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = nil_name,
  };

  inner_args[1].type = &list_type._base;
  inner_args[1].name = nil_name;

  FbleApplyExpr applys[args.size];
  FbleExpr* all_args[args.size * 2];
  for (size_t i = 0; i < args.size; ++i) {
    applys[i]._base.tag = FBLE_FUNC_APPLY_EXPR;
    applys[i]._base.loc = loc;
    applys[i].misc = &cons._base;
    applys[i].args.size = 2;
    applys[i].args.xs = all_args + 2 * i;

    applys[i].args.xs[0] = &arg_values[i]._base;
    applys[i].args.xs[1] = (i + 1 < args.size) ? &applys[i+1]._base : &nil._base;
  }

  FbleFuncValueExpr inner_func = {
    ._base = { .tag = FBLE_FUNC_VALUE_EXPR, .loc = loc },
    .args = { .size = 2, .xs = inner_args },
    .body = (args.size == 0) ? &nil._base : &applys[0]._base,
  };

  FblePolyExpr inner_poly = {
    ._base = { .tag = FBLE_POLY_EXPR, .loc = loc },
    .arg = {
      .kind = &basic_kind->_base,
      .name = list_type_name,
    },
    .body = &inner_func._base,
  };

  FbleTaggedTypeExpr outer_args[args.size];
  for (size_t i = 0; i < args.size; ++i) {
    outer_args[i].type = &elem_type._base;
    outer_args[i].name = arg_names[i];
  }

  FbleFuncValueExpr outer_func = {
    ._base = { .tag = FBLE_FUNC_VALUE_EXPR, .loc = loc },
    .args = { .size = args.size, .xs = outer_args },
    .body = &inner_poly._base,
  };

  FblePolyExpr outer_poly = {
    ._base = { .tag = FBLE_POLY_EXPR, .loc = loc },
    .arg = {
      .kind = &basic_kind->_base,
      .name = elem_type_name,
    },
    .body = &outer_func._base,
  };

  FblePolyApplyExpr apply_type = {
    ._base = { .tag = FBLE_POLY_APPLY_EXPR, .loc = loc },
    .poly = &outer_poly._base,
    .arg = type,
  };

  FbleApplyExpr apply_elems = {
    ._base = { .tag = FBLE_FUNC_APPLY_EXPR, .loc = loc },
    .misc = &apply_type._base,
    .args = args,
  };

  FbleExpr* expr = &apply_elems._base;

  Local* result = CompileExpr(arena, blocks, exit, scope, expr);

  FbleFreeKind(arena, &basic_kind->_base);
  for (size_t i = 0; i < args.size; i++) {
    FbleFreeString(arena, arg_names[i].name);
  }
  FbleFreeString(arena, elem_type_name.name);
  FbleFreeString(arena, list_type_name.name);
  FbleFreeString(arena, cons_name.name);
  FbleFreeString(arena, nil_name.name);
  return result;
}

// CompileExec --
//   Compile the given process expression. Returns the local variable that
//   will hold the result of executing the process expression and generates
//   instructions to compute the value of that result at runtime.
//
// Inputs:
//   arena - arena to use for type allocations.
//   blocks - the blocks stack.
//   exit - if true, generate instructions to exit the current scope.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The local of the result of executing the process expression.
//
// Side effects:
// * Updates the blocks stack with with compiled block information.
// * Appends instructions to the scope for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   the scope if the expression fails to compile.
// * The caller should call LocalRelease when the returned results are no
//   longer needed. Note that FreeScope calls LocalRelease for all locals
//   allocated to the scope, so that can also be used to clean up the local.
static Local* CompileExec(FbleArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr)
{
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR:
    case FBLE_MISC_APPLY_EXPR:
    case FBLE_FUNC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_STRUCT_ACCESS_EXPR:
    case FBLE_UNION_ACCESS_EXPR:
    case FBLE_MISC_ACCESS_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_POLY_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR: {
      Local* proc = CompileExpr(arena, blocks, false, scope, expr);
      Local* local = NewLocal(arena, scope);

      if (exit) {
        AppendProfileOp(arena, scope, FBLE_PROFILE_AUTO_EXIT_OP, 0);
      }

      // A process is represented at runtime as a zero argument function. To
      // execute the process, use the FBLE_CALL_INSTR.
      FbleCallInstr* instr = FbleAlloc(arena, FbleCallInstr);
      instr->_base.tag = FBLE_CALL_INSTR;
      instr->_base.profile_ops = NULL;
      instr->loc = FbleCopyLoc(expr->loc);
      instr->exit = exit;
      instr->dest = local->index.index;
      instr->func = proc->index;
      FbleVectorInit(arena, instr->args);
      AppendInstr(arena, scope, &instr->_base);

      LocalRelease(arena, scope, proc, exit);
      return local;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;
      return CompileExpr(arena, blocks, exit, scope, eval_expr->body);
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;

      FbleLinkInstr* link = FbleAlloc(arena, FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;
      link->_base.profile_ops = NULL;

      Local* get_local = NewLocal(arena, scope);
      link->get = get_local->index.index;
      PushVar(arena, scope, link_expr->get, get_local);

      Local* put_local = NewLocal(arena, scope);
      link->put = put_local->index.index;
      PushVar(arena, scope, link_expr->put, put_local);

      AppendInstr(arena, scope, &link->_base);

      return CompileExec(arena, blocks, exit, scope, link_expr->body);
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;

      Local* args[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        args[i] = CompileExpr(arena, blocks, false, scope, exec_expr->bindings.xs[i].expr);
      }

      FbleForkInstr* fork = FbleAlloc(arena, FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      fork->_base.profile_ops = NULL;
      FbleVectorInit(arena, fork->args);
      fork->dests.xs = FbleArrayAlloc(arena, FbleLocalIndex, exec_expr->bindings.size);
      fork->dests.size = exec_expr->bindings.size;
      AppendInstr(arena, scope, &fork->_base);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        // Note: Make sure we call NewLocal before calling LocalRelease on any
        // of the arguments.
        Local* local = NewLocal(arena, scope);
        fork->dests.xs[i] = local->index.index;
        PushVar(arena, scope, exec_expr->bindings.xs[i].name, local);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        // TODO: Does this hold on to the bindings longer than we want to?
        if (args[i] != NULL) {
          FbleVectorAppend(arena, fork->args, args[i]->index);
          LocalRelease(arena, scope, args[i], false);
        }
      }

      Local* local = CompileExec(arena, blocks, exit, scope, exec_expr->body);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PopVar(arena, scope, exit);
      }

      return local;
    }
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// CompileProgram --
//   Compile the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   scope - the list of variables in scope.
//   prgm - the program to compile.
//
// Side effects:
//   Updates 'blocks' with compiled block information. Exits the current
//   block.
//   Appends instructions to scope for executing the given program.
static void CompileProgram(FbleArena* arena, Blocks* blocks, Scope* scope, FbleProgram* prgm)
{
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    EnterBlock(arena, blocks, prgm->modules.xs[i].name, prgm->modules.xs[i].value->loc, scope);
    Local* module = CompileExpr(arena, blocks, false, scope, prgm->modules.xs[i].value);
    ExitBlock(arena, blocks, scope, false);

    PushVar(arena, scope, prgm->modules.xs[i].name, module);
  }

  CompileExpr(arena, blocks, true, scope, prgm->main);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    PopVar(arena, scope, true);
  }
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

  FbleFrameIndexV capture;
  FbleInstrBlock* code;
  Scope scope;
  InitScope(arena, &scope, &code, &capture, NULL);

  EnterBlock(arena, &block_stack, entry_name, program->main->loc, &scope);
  bool ok = FbleTypeCheck(arena, program);
  if (ok) {
    CompileProgram(arena, &block_stack, &scope, program);
  }
  ExitBlock(arena, &block_stack, &scope, true);
  FbleFreeString(arena, entry_name.name);
  FreeScope(arena, &scope, true);
  assert(capture.size == 0 && "captured variables from nowhere?");
  FbleFree(arena, capture.xs);

  assert(block_stack.stack.size == 0);
  FbleFree(arena, block_stack.stack.xs);

  if (profiling_disabled) {
    FbleFreeProfile(arena, profile);
  }

  if (!ok) {
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
