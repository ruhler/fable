// compile.c --
//   This file implements the fble compilation routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "fble-type.h"
#include "internal.h"

#define UNREACHABLE(x) assert(false && x)

// Var --
//   Information about a variable visible during type checking.
//
// name - the name of the variable.
// type - the type of the variable.
// index - the index of the variable in the current stack frame.
// used  - true if the variable is used anywhere, false otherwise.
typedef struct {
  FbleName name;
  FbleType* type;
  FbleFrameIndex index;
  bool used;
} Var;

// Var --
//   A vector of pointers to vars.
typedef struct {
  size_t size;
  Var** xs;
} VarV;

// Scope --
//   Scope of variables visible during type checking.
//
// Fields:
//   statics - variables captured from the parent scope.
//   locals - local variables.
//   code - the instruction block for this scope.
//   phantom - if true, operations on this scope should not have any side
//             effects on the parent scope. Used in the case when we are
//             compiling for types only, not for instructions.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  VarV statics;
  VarV locals;
  FbleInstrBlock* code;
  bool phantom;
  struct Scope* parent;
} Scope;

// BlockFrame --
//   Represents a profiling block.
//
// Fields:
//   id - the id of the block.
//   time - pointer to the static shallow profile time for the block.
typedef struct {
  size_t id;
  size_t* time;
} BlockFrame;

// BlockFrameV --
//   A stack of block frames. 
typedef struct {
  size_t size;
  BlockFrame* xs;
} BlockFrameV;

// Blocks --
//   A stack of block frames tracking the current block for profiling.
//
// Fields:
//   stack - The stack of black frames representing the current location.
//   blocks - Mapping from block id to block name and location, which is an
//            output of compilation.
typedef struct {
  BlockFrameV stack;
  FbleNameV blocks;
} Blocks;

static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...);

static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type);
static void PopVar(FbleArena* arena, Scope* scope);
static Var* GetVar(FbleArena* arena, Scope* scope, FbleName name, bool phantom);

static void EnterBlock(FbleArena* arena, Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope);
static void EnterBodyBlock(FbleArena* arena, Blocks* blocks, FbleLoc loc, Scope* scope);
static void ExitBlock(FbleArena* arena, Blocks* blocks, Scope* scope);
static void AddBlockTime(Blocks* blocks, size_t time);

static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock* code, bool phantom, Scope* parent);
static void FinishScope(FbleArena* arena, Scope* scope);
static void AppendInstr(FbleArena* arena, Scope* scope, FbleInstr* instr);

static FbleInstrBlock* NewInstrBlock(FbleArena* arena);
static void FreeInstr(FbleArena* arena, FbleInstr* instr);

static bool CheckNameSpace(FbleTypeArena* arena, FbleName* name, FbleType* type);

static FbleType* ValueOfType(FbleTypeArena* arena, FbleType* typeof);

static void CompileExit(FbleArena* arena, bool exit, Scope* scope);
static FbleType* CompileExpr(FbleTypeArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr);
static FbleType* CompileList(FbleTypeArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleLoc loc, FbleExprV args);
static FbleType* CompileExprNoInstrs(FbleTypeArena* arena, Scope* scope, FbleExpr* expr);
static FbleType* CompileType(FbleTypeArena* arena, Scope* scope, FbleTypeExpr* type);
static FbleType* CompileProgram(FbleTypeArena* arena, Blocks* blocks, Scope* scope, FbleProgram* prgm);

// ReportError --
//   Report a compiler error.
//
//   This uses a printf-like format string. The following format specifiers
//   are supported:
//     %i - size_t
//     %k - FbleKind*
//     %n - FbleName*
//     %t - FbleType*
//   Please add additional format specifiers as needed.
//
// Inputs:
//   arena - arena to use for allocations.
//   loc - the location of the error.
//   fmt - the format string.
//   ... - The var-args for the associated conversion specifiers in fmt.
//
// Results:
//   none.
//
// Side effects:
//   Prints a message to stderr as described by fmt and provided arguments.
static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...)
{
  FbleReportError("", loc);

  va_list ap;
  va_start(ap, fmt);

  for (const char* p = strchr(fmt, '%'); p != NULL; p = strchr(fmt, '%')) {
    fprintf(stderr, "%.*s", p - fmt, fmt);

    switch (*(p + 1)) {
      case 'i': {
        size_t x = va_arg(ap, size_t);
        fprintf(stderr, "%d", x);
        break;
      }

      case 'k': {
        FbleKind* kind = va_arg(ap, FbleKind*);
        FblePrintKind(kind);
        break;
      }

      case 'n': {
        FbleName* name = va_arg(ap, FbleName*);
        FblePrintName(stderr, name);
        break;
      }

      case 't': {
        FbleType* type = va_arg(ap, FbleType*);
        FblePrintType(arena, type);
        break;
      }

      default: {
        UNREACHABLE("Unsupported format conversion.");
        break;
      }
    }
    fmt = p + 2;
  }
  fprintf(stderr, "%s", fmt);
  va_end(ap);
}

// PushVar --
//   Push a variable onto the current scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to push the variable on to.
//   name - the name of the variable.
//   type - the type of the variable.
//
// Results:
//   A pointer to the newly pushed variable. The pointer is owned by the
//   scope. It remains valid until a corresponding PopVar or FinishScope
//   occurs.
//
// Side effects:
//   Pushes a new variable with given name and type onto the scope. It is the
//   callers responsibility to ensure that the type stays alive as long as is
//   needed.
static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type)
{
  Var* var = FbleAlloc(arena, Var);
  var->name = name;
  var->type = type;
  var->index.section = FBLE_LOCALS_FRAME_SECTION;
  var->index.index = scope->locals.size;
  var->used = false;
  FbleVectorAppend(arena, scope->locals, var);
  if (scope->locals.size > scope->code->locals) {
    scope->code->locals = scope->locals.size;
  }
  return var;
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to pop from.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope. Invalidates the pointer to the variable
//   originally returned in PushVar.
static void PopVar(FbleArena* arena, Scope* scope)
{
  scope->locals.size--;
  FbleFree(arena, scope->locals.xs[scope->locals.size]);
}

// GetVar --
//   Lookup a var in the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to look in.
//   name - the name of the variable.
//   phantom - if true, do not consider the variable to be accessed.
//
// Result:
//   The variable from the scope, or NULL if no such variable was found. The
//   variable is owned by the scope and remains valid until either PopVar is
//   called or the scope is finished.
//
// Side effects:
//   Marks variable as used and for capture if necessary and not phantom.
static Var* GetVar(FbleArena* arena, Scope* scope, FbleName name, bool phantom)
{
  for (size_t i = 0; i < scope->locals.size; ++i) {
    size_t j = scope->locals.size - i - 1;
    Var* var = scope->locals.xs[j];
    if (FbleNamesEqual(&name, &var->name)) {
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    Var* var = scope->statics.xs[i];
    if (FbleNamesEqual(&name, &var->name)) {
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  if (scope->parent != NULL) {
    Var* var = GetVar(arena, scope->parent, name, scope->phantom || phantom);
    if (var != NULL) {
      if (phantom) {
        // It doesn't matter that we are returning a variable for the wrong
        // scope here. phantom means we won't actually use it ever.
        return var;
      }

      Var* captured = FbleAlloc(arena, Var);
      captured->name = var->name;
      captured->type = var->type;
      captured->index.section = FBLE_STATICS_FRAME_SECTION;
      captured->index.index = scope->statics.size;
      captured->used = !phantom;

      FbleVectorAppend(arena, scope->statics, captured);
      if (scope->statics.size > scope->code->statics) {
        scope->code->statics = scope->statics.size;
      }
      return captured;
    }
  }

  return NULL;
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
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1].id;
    curr = blocks->blocks.xs[curr_id].name;
    currlen = strlen(curr);
  }

  size_t id = blocks->blocks.size;

  FbleProfileEnterBlockInstr* enter = FbleAlloc(arena, FbleProfileEnterBlockInstr);
  enter->_base.tag = FBLE_PROFILE_ENTER_BLOCK_INSTR;
  enter->block = id;
  enter->time = 0;
  AppendInstr(arena, scope, &enter->_base);

  BlockFrame* frame = FbleVectorExtend(arena, blocks->stack);
  frame->id = id;
  frame->time = &enter->time;

  // Append ".name" to the current block name to figure out the new block
  // name.
  char* str = FbleArrayAlloc(arena, char, currlen + strlen(name.name) + 3);
  str[0] = '\0';
  if (currlen > 0) {
    strcat(str, curr);
    strcat(str, ".");
  }
  strcat(str, name.name);
  switch (name.space) {
    case FBLE_NORMAL_NAME_SPACE: break;
    case FBLE_TYPE_NAME_SPACE: strcat(str, "@"); break;
    case FBLE_MODULE_NAME_SPACE: strcat(str, "%"); break;
  }

  FbleName* nm = FbleVectorExtend(arena, blocks->blocks);
  nm->name = str;
  nm->loc = loc;
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
    size_t curr_id = blocks->stack.xs[blocks->stack.size-1].id;
    curr = blocks->blocks.xs[curr_id].name;
  }

  size_t id = blocks->blocks.size;

  FbleProfileEnterBlockInstr* enter = FbleAlloc(arena, FbleProfileEnterBlockInstr);
  enter->_base.tag = FBLE_PROFILE_ENTER_BLOCK_INSTR;
  enter->block = id;
  enter->time = 0;
  AppendInstr(arena, scope, &enter->_base);

  BlockFrame* frame = FbleVectorExtend(arena, blocks->stack);
  frame->id = id;
  frame->time = &enter->time;

  // Append "!" to the current block name to figure out the new block name.
  char* str = FbleArrayAlloc(arena, char, strlen(curr) + 2);
  str[0] = '\0';
  strcat(str, curr);
  strcat(str, "!");

  FbleName* nm = FbleVectorExtend(arena, blocks->blocks);
  nm->name = str;
  nm->loc = loc;
}

// ExitBlock --
//   Exit the current profiling block frame.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   scope - where to generate the PROFILE_EXIT_BLOCK instruction, or NULL to
//           indicate that no instruction should be generated.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top block frame off the blocks stack and append a
//   PROFILE_EXIT_BLOCK instruction to instrs if instrs is non-null.
static void ExitBlock(FbleArena* arena, Blocks* blocks, Scope* scope)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;

  if (scope != NULL) {
    FbleProfileExitBlockInstr* exit_instr = FbleAlloc(arena, FbleProfileExitBlockInstr);
    exit_instr->_base.tag = FBLE_PROFILE_EXIT_BLOCK_INSTR;
    AppendInstr(arena, scope, &exit_instr->_base);
  }
}

// AddBlockTime --
//   Adds profile time to the current block frame.
//
// Inputs:
//   blocks - the blocks stack.
//   time - the amount of time to add to the current block frame.
//
// Results:
//   none.
//
// Side effects:
//   Adds 'time' profile time to the current block frame.
static void AddBlockTime(Blocks* blocks, size_t time)
{
  if (blocks->stack.size > 0) {
    *(blocks->stack.xs[blocks->stack.size-1].time) += time;
  }
}

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to initialize.
//   code - a pointer to the code block for this scope.
//   phantom - whether the scope is a phantom scope or not.
//   parent - the parent of the scope to initialize. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Initializes scope based on parent. FinishScope should be
//   called to free the allocations for scope. The lifetimes of the code block
//   and the parent scope must exceed the lifetime of this scope.
static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock* code, bool phantom, Scope* parent)
{
  FbleVectorInit(arena, scope->statics);
  FbleVectorInit(arena, scope->locals);
  scope->code = code;
  scope->code->statics = 0;
  scope->code->locals = 0;
  scope->phantom = phantom;
  scope->parent = parent;
}

// FinishScope --
//   Free memory associated with a Scope, and generate instructions to capture
//   the variables used by the scope.
//
// Inputs:
//   arena - arena to use for allocations
//   scope - the scope to finish.
//
// Results:
//   None.
//
// Side effects:
//   * Frees memory associated with scope.
//   * Appends instructions to the parent scope's code to push captured
//     variables to the data stack if this is not a phantom scope.
static void FinishScope(FbleArena* arena, Scope* scope)
{
  if (scope->parent != NULL && !scope->phantom) {
    for (size_t i = 0; i < scope->statics.size; ++i) {
      // Copy the captured var to the data stack for capturing.
      Var* captured = scope->statics.xs[i];
      FbleVarInstr* get_var = FbleAlloc(arena, FbleVarInstr);
      get_var->_base.tag = FBLE_VAR_INSTR;
      Var* var = GetVar(arena, scope->parent, captured->name, false);
      assert(var != NULL);
      get_var->index = var->index;
      AppendInstr(arena, scope->parent, &get_var->_base);
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleFree(arena, scope->statics.xs[i]);
  }
  FbleFree(arena, scope->statics.xs);

  for (size_t i = 0; i < scope->locals.size; ++i) {
    FbleFree(arena, scope->locals.xs[i]);
  }
  FbleFree(arena, scope->locals.xs);
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
  FbleVectorAppend(arena, scope->code->instrs, instr);
}

// NewInstrBlock --
//   Allocate and initialize a new instruction block.
//
// Inputs:
//   arena - arena to use for allocations.
//
// Results:
//   A newly allocated and initialized instruction block.
//
// Side effects:
//   Allocates a new instruction block that should be freed with
//   FbleFreeInstrBlock when no longer needed.
static FbleInstrBlock* NewInstrBlock(FbleArena* arena)
{
  FbleInstrBlock* instr_block = FbleAlloc(arena, FbleInstrBlock);
  instr_block->refcount = 1;
  instr_block->statics = 0;
  instr_block->locals = 0;
  FbleVectorInit(arena, instr_block->instrs);
  return instr_block;
}

// FreeInstr --
//   Free the given instruction.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instr - the instruction to free.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the given instruction.
static void FreeInstr(FbleArena* arena, FbleInstr* instr)
{
  assert(instr != NULL);
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR:
    case FBLE_UNION_VALUE_INSTR:
    case FBLE_STRUCT_ACCESS_INSTR:
    case FBLE_UNION_ACCESS_INSTR:
    case FBLE_UNION_SELECT_INSTR:
    case FBLE_GOTO_INSTR:
    case FBLE_DESCOPE_INSTR:
    case FBLE_FUNC_APPLY_INSTR:
    case FBLE_VAR_INSTR:
    case FBLE_GET_INSTR:
    case FBLE_PUT_INSTR:
    case FBLE_LINK_INSTR:
    case FBLE_PROC_INSTR:
    case FBLE_JOIN_INSTR:
    case FBLE_REF_VALUE_INSTR:
    case FBLE_REF_DEF_INSTR:
    case FBLE_EXIT_SCOPE_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_VPUSH_INSTR:
    case FBLE_PROFILE_ENTER_BLOCK_INSTR:
    case FBLE_PROFILE_EXIT_BLOCK_INSTR:
    case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR: {
      FbleFree(arena, instr);
      return;
    }

    case FBLE_STRUCT_IMPORT_INSTR: {
      FbleStructImportInstr* import_instr = (FbleStructImportInstr*)instr;
      FbleFree(arena, import_instr->fields.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeInstrBlock(arena, func_value_instr->code);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_FORK_INSTR: {
      FbleForkInstr* fork_instr = (FbleForkInstr*)instr;
      FbleFree(arena, fork_instr->args.xs);
      FbleFree(arena, instr);
      return;
    }

    case FBLE_PROC_VALUE_INSTR: {
      FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
      FbleFreeInstrBlock(arena, proc_value_instr->code);
      FbleFree(arena, proc_value_instr);
      return;
    }
  }

  UNREACHABLE("invalid instruction");
}

// CheckNameSpace --
//   Verify that the namespace of the given name is appropriate for the type
//   of value the name refers to.
//
// Inputs:
//   name - the name in question
//   type - the type of the value refered to be the name.
//
// Results:
//   true if the namespace of the name is consistent with the type. false
//   otherwise.
//
// Side effects:
//   Prints a message to stderr if the namespace and type don't match.
static bool CheckNameSpace(FbleTypeArena* arena, FbleName* name, FbleType* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleKind* kind = FbleGetKind(arena_, type);
  size_t kind_level = FbleGetKindLevel(kind);
  FbleKindRelease(arena_, kind);

  if (name->space == FBLE_TYPE_NAME_SPACE && kind_level != 2) {
    ReportError(FbleRefArenaArena(arena), &name->loc,
        "expected a type type for field named '%n', but found type %t\n",
        name, type);
    return false;
  }

  if (name->space == FBLE_NORMAL_NAME_SPACE && kind_level != 1) {
    ReportError(FbleRefArenaArena(arena), &name->loc,
        "expected a normal type for field named '%n', but found type %t\n",
        name, type);
    return false;
  }

  return true;
}

// ValueOfType --
//   Returns the value of a type given the type of the type.
//
// Inputs:
//   arena - type arena for allocations.
//   typeof - the type of the type to get the value of.
//
// Results:
//   The value of the type to get the value of. Or NULL if the value is not a
//   type.
//
// Side effects:
//   The returned type must be released using FbleTypeRelease when no longer
//   needed.
static FbleType* ValueOfType(FbleTypeArena* arena, FbleType* typeof)
{
  if (typeof->tag == FBLE_TYPE_TYPE) {
    FbleTypeType* tt = (FbleTypeType*)typeof;
    FbleTypeRetain(arena, tt->type);
    return tt->type;
  }
  return NULL;
}


// CompileExit --
//   If exit is true, appends an exit scope instruction to instrs.
//
// Inputs:
//   arena - arena for allocations.
//   blocks - the blocks stack.
//   exit - whether we actually want to exit.
//   scope - the scope to append the instructions to.
//
// Results:
//   none.
//
// Side effects:
//   If exit is true, appends an exit scope instruction to instrs
static void CompileExit(FbleArena* arena, bool exit, Scope* scope)
{
  if (exit) {
    FbleExitScopeInstr* exit_scope = FbleAlloc(arena, FbleExitScopeInstr);
    exit_scope->_base.tag = FBLE_EXIT_SCOPE_INSTR;
    AppendInstr(arena, scope, &exit_scope->_base);
  }
}

// CompileExpr --
//   Type check and compile the given expression. Returns the type of the
//   expression and generates instructions to compute the value of that
//   expression at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   exit - if true, generate instructions to exit the current scope.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Updates the blocks stack with with compiled block information.
//   Appends instructions to 'instrs' for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the expression fails to compile.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static FbleType* CompileExpr(FbleTypeArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      AddBlockTime(blocks, 1);

      FbleType* type = CompileType(arena, scope, expr);
      if (type == NULL) {
        return NULL;
      }

      FbleTypeType* type_type = FbleAlloc(arena_, FbleTypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = FBLE_TYPE_TYPE;
      type_type->_base.loc = expr->loc;
      type_type->_base.id = (uintptr_t)type_type;
      type_type->type = type;
      FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
      FbleTypeRelease(arena, type);

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      AppendInstr(arena_, scope, &instr->_base);

      CompileExit(arena_, exit, scope);
      return &type_type->_base;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* misc_apply_expr = (FbleMiscApplyExpr*)expr;
      bool error = false;

      size_t argc = misc_apply_expr->args.size;
      AddBlockTime(blocks, 1 + argc);
      FbleType* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - 1 - i;
        arg_types[j] = CompileExpr(arena, blocks, false, scope, misc_apply_expr->args.xs[j]);
        error = error || (arg_types[j] == NULL);
      }

      FbleType* type = CompileExpr(arena, blocks, false, scope, misc_apply_expr->misc);
      error = error || (type == NULL);

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(arena, arg_types[i]);
        }
        FbleTypeRelease(arena, type);
        return NULL;
      }

      FbleType* normal = FbleNormalType(arena, type);
      switch (normal->tag) {
        case FBLE_FUNC_TYPE: {
          // FUNC_APPLY
          FbleTypeRelease(arena, type);
          for (size_t i = 0; i < argc; ++i) {
            if (normal->tag != FBLE_FUNC_TYPE) {
              ReportError(arena_, &expr->loc, "too many arguments to function\n");
              for (size_t i = 0; i < argc; ++i) {
                FbleTypeRelease(arena, arg_types[i]);
              }
              FbleTypeRelease(arena, normal);
              return NULL;
            }

            FbleFuncType* func_type = (FbleFuncType*)normal;
            if (!FbleTypesEqual(arena, func_type->arg, arg_types[i])) {
              ReportError(arena_, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  func_type->arg, arg_types[i]);
              for (size_t j = 0; j < argc; ++j) {
                FbleTypeRelease(arena, arg_types[j]);
              }
              FbleTypeRelease(arena, normal);
              return NULL;
            }

            FbleFuncApplyInstr* apply_instr = FbleAlloc(arena_, FbleFuncApplyInstr);
            apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
            apply_instr->loc = misc_apply_expr->misc->loc;
            apply_instr->exit = exit && (i+1 == argc);
            AppendInstr(arena_, scope, &apply_instr->_base);

            FbleType* tmp = normal;
            normal = FbleNormalType(arena, func_type->rtype);
            FbleTypeRelease(arena, tmp);
          }

          for (size_t i = 0; i < argc; ++i) {
            FbleTypeRelease(arena, arg_types[i]);
          }

          return normal;
        }

        case FBLE_TYPE_TYPE: {
          // FBLE_STRUCT_VALUE_EXPR
          FbleTypeType* type_type = (FbleTypeType*)normal;
          FbleType* vtype = FbleTypeRetain(arena, type_type->type);
          FbleTypeRelease(arena, normal);
          FbleTypeRelease(arena, type);

          FbleStructType* struct_type = (FbleStructType*)FbleNormalType(arena, vtype);
          if (struct_type->_base.tag != FBLE_STRUCT_TYPE) {
            ReportError(arena_, &misc_apply_expr->misc->loc,
                "expected a struct type, but found %t\n",
                vtype);
            for (size_t i = 0; i < argc; ++i) {
              FbleTypeRelease(arena, arg_types[i]);
            }
            FbleTypeRelease(arena, &struct_type->_base);
            FbleTypeRelease(arena, vtype);
            return NULL;
          }

          if (struct_type->fields.size != argc) {
            // TODO: Where should the error message go?
            ReportError(arena_, &expr->loc,
                "expected %i args, but %i were provided\n",
                 struct_type->fields.size, argc);
            for (size_t i = 0; i < argc; ++i) {
              FbleTypeRelease(arena, arg_types[i]);
            }
            FbleTypeRelease(arena, &struct_type->_base);
            FbleTypeRelease(arena, vtype);
            return NULL;
          }

          bool error = false;
          for (size_t i = 0; i < argc; ++i) {
            FbleTaggedType* field = struct_type->fields.xs + i;

            if (!FbleTypesEqual(arena, field->type, arg_types[i])) {
              ReportError(arena_, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  field->type, arg_types[i]);
              error = true;
            }
            FbleTypeRelease(arena, arg_types[i]);
          }

          if (error) {
            FbleTypeRelease(arena, &struct_type->_base);
            FbleTypeRelease(arena, vtype);
            return NULL;
          }

          FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
          struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
          struct_instr->argc = struct_type->fields.size;
          AppendInstr(arena_, scope, &struct_instr->_base);
          CompileExit(arena_, exit, scope);
          FbleTypeRelease(arena, &struct_type->_base);
          return vtype;
        }

        default: {
          ReportError(arena_, &expr->loc,
              "expecting a function or struct type, but found something of type %t\n",
              type);
          for (size_t i = 0; i < argc; ++i) {
            FbleTypeRelease(arena, arg_types[i]);
          }
          FbleTypeRelease(arena, normal);
          FbleTypeRelease(arena, type);
          return NULL;
        }
      }

      UNREACHABLE("Should never get here");
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      AddBlockTime(blocks, 1);

      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      FbleStructType* struct_type = FbleAlloc(arena_, FbleStructType);
      FbleRefInit(arena, &struct_type->_base.ref);
      struct_type->_base.tag = FBLE_STRUCT_TYPE;
      struct_type->_base.loc = expr->loc;
      struct_type->_base.id = (uintptr_t)struct_type;
      FbleVectorInit(arena_, struct_type->fields);

      size_t argc = struct_expr->args.size;
      FbleType* arg_types[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        EnterBlock(arena_, blocks, arg->name, arg->expr->loc, scope);
        arg_types[j] = CompileExpr(arena, blocks, false, scope, arg->expr);
        ExitBlock(arena_, blocks, scope);
        error = error || (arg_types[j] == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (arg_types[i] != NULL) {
          if (!CheckNameSpace(arena, &arg->name, arg_types[i])) {
            error = true;
          }

          FbleTaggedType* cfield = FbleVectorExtend(arena_, struct_type->fields);
          cfield->name = arg->name;
          cfield->type = arg_types[i];
          FbleRefAdd(arena, &struct_type->_base.ref, &cfield->type->ref);
          FbleTypeRelease(arena, arg_types[i]);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&arg->name, &struct_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena_, &arg->name.loc,
                "duplicate field name '%n'\n",
                &struct_expr->args.xs[j].name);
          }
        }
      }

      if (error) {
        FbleTypeRelease(arena, &struct_type->_base);
        return NULL;
      }

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      AppendInstr(arena_, scope, &instr->_base);

      FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->argc = struct_expr->args.size;
      AppendInstr(arena_, scope, &struct_instr->_base);
      CompileExit(arena_, exit, scope);
      return &struct_type->_base;
    }

    case FBLE_UNION_VALUE_EXPR: {
      AddBlockTime(blocks, 1);
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleType* type = CompileType(arena, scope, union_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      FbleUnionType* union_type = (FbleUnionType*)FbleNormalType(arena, type);
      if (union_type->_base.tag != FBLE_UNION_TYPE) {
        ReportError(arena_, &union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
        FbleTypeRelease(arena, &union_type->_base);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      FbleType* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleTaggedType* field = union_type->fields.xs + i;
        if (FbleNamesEqual(&field->name, &union_value_expr->field)) {
          tag = i;
          field_type = field->type;
          break;
        }
      }

      if (field_type == NULL) {
        ReportError(arena_, &union_value_expr->field.loc,
            "'%n' is not a field of type %t\n",
            &union_value_expr->field, type);
        FbleTypeRelease(arena, &union_type->_base);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      FbleType* arg_type = CompileExpr(arena, blocks, false, scope, union_value_expr->arg);
      if (arg_type == NULL) {
        FbleTypeRelease(arena, &union_type->_base);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      if (!FbleTypesEqual(arena, field_type, arg_type)) {
        ReportError(arena_, &union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg_type);
        FbleTypeRelease(arena, type);
        FbleTypeRelease(arena, &union_type->_base);
        FbleTypeRelease(arena, arg_type);
        return NULL;
      }
      FbleTypeRelease(arena, &union_type->_base);
      FbleTypeRelease(arena, arg_type);

      FbleUnionValueInstr* union_instr = FbleAlloc(arena_, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->tag = tag;
      AppendInstr(arena_, scope, &union_instr->_base);
      CompileExit(arena_, exit, scope);
      return type;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      AddBlockTime(blocks, 1);

      FbleMiscAccessExpr* access_expr = (FbleMiscAccessExpr*)expr;

      FbleType* type = CompileExpr(arena, blocks, false, scope, access_expr->object);
      if (type == NULL) {
        return NULL;
      }

      FbleAccessInstr* access = FbleAlloc(arena_, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->loc = access_expr->field.loc;
      AppendInstr(arena_, scope, &access->_base);

      CompileExit(arena_, exit, scope);

      FbleType* normal = FbleNormalType(arena, type);
      FbleTaggedTypeV* fields = NULL;
      if (normal->tag == FBLE_STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
        fields = &((FbleStructType*)normal)->fields;
      } else if (normal->tag == FBLE_UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
        fields = &((FbleUnionType*)normal)->fields;
      } else {
        ReportError(arena_, &access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            type);

        FbleTypeRelease(arena, normal);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(&access_expr->field, &fields->xs[i].name)) {
          access->tag = i;
          FbleType* rtype = FbleTypeRetain(arena, fields->xs[i].type);
          FbleTypeRelease(arena, normal);
          FbleTypeRelease(arena, type);
          return rtype;
        }
      }

      ReportError(arena_, &access_expr->field.loc,
          "'%n' is not a field of type %t\n",
          &access_expr->field, type);
      FbleTypeRelease(arena, normal);
      FbleTypeRelease(arena, type);
      return NULL;
    }

    case FBLE_UNION_SELECT_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      AddBlockTime(blocks, 1);

      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      FbleType* type = CompileExpr(arena, blocks, false, scope, select_expr->condition);
      if (type == NULL) {
        return NULL;
      }

      FbleUnionType* union_type = (FbleUnionType*)FbleNormalType(arena, type);
      if (union_type->_base.tag != FBLE_UNION_TYPE) {
        ReportError(arena_, &select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            type);
        FbleTypeRelease(arena, &union_type->_base);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      if (exit) {
        FbleProfileAutoExitBlockInstr* exit_instr = FbleAlloc(arena_, FbleProfileAutoExitBlockInstr);
        exit_instr->_base.tag = FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR;
        AppendInstr(arena_, scope, &exit_instr->_base);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(arena_, FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->loc = select_expr->condition->loc;
      AppendInstr(arena_, scope, &select_instr->_base);

      FbleType* return_type = NULL;
      FbleGotoInstr* enter_gotos[union_type->fields.size];
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        enter_gotos[i] = FbleAlloc(arena_, FbleGotoInstr);
        enter_gotos[i]->_base.tag = FBLE_GOTO_INSTR;
        AppendInstr(arena_, scope, &enter_gotos[i]->_base);
      }

      size_t default_pc = scope->code->instrs.size;
      FbleGotoInstr* exit_goto_default = NULL;
      if (select_expr->default_ != NULL) {
        FbleName name = {
          .name = ":",
          .loc = select_expr->default_->loc,  // unused.
          .space = FBLE_NORMAL_NAME_SPACE
        };
        EnterBlock(arena_, blocks, name, select_expr->default_->loc, scope);
        return_type = CompileExpr(arena, blocks, exit, scope, select_expr->default_);
        ExitBlock(arena_, blocks, exit ? NULL : scope);

        if (return_type == NULL) {
          FbleTypeRelease(arena, &union_type->_base);
          FbleTypeRelease(arena, type);
          return NULL;
        }

        if (!exit) {
          exit_goto_default = FbleAlloc(arena_, FbleGotoInstr);
          exit_goto_default->_base.tag = FBLE_GOTO_INSTR;
          AppendInstr(arena_, scope, &exit_goto_default->_base);
        }
      }

      FbleGotoInstr* exit_gotos[select_expr->choices.size];
      size_t choice = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (choice < select_expr->choices.size && FbleNamesEqual(&select_expr->choices.xs[choice].name, &union_type->fields.xs[i].name)) {
          enter_gotos[i]->pc = scope->code->instrs.size;

          EnterBlock(arena_, blocks,
              select_expr->choices.xs[choice].name,
              select_expr->choices.xs[choice].expr->loc,
              scope);
          AddBlockTime(blocks, 1);
          FbleType* arg_type = CompileExpr(arena, blocks, exit, scope, select_expr->choices.xs[choice].expr);
          ExitBlock(arena_, blocks, exit ? NULL : scope);

          if (!exit) {
            exit_gotos[choice] = FbleAlloc(arena_, FbleGotoInstr);
            exit_gotos[choice]->_base.tag = FBLE_GOTO_INSTR;
            AppendInstr(arena_, scope, &exit_gotos[choice]->_base);
          }

          if (arg_type == NULL) {
            FbleTypeRelease(arena, return_type);
            FbleTypeRelease(arena, &union_type->_base);
            FbleTypeRelease(arena, type);
            return NULL;
          }

          if (return_type == NULL) {
            return_type = arg_type;
          } else {
            if (!FbleTypesEqual(arena, return_type, arg_type)) {
              ReportError(arena_, &select_expr->choices.xs[choice].expr->loc,
                  "expected type %t, but found %t\n",
                  return_type, arg_type);

              FbleTypeRelease(arena, type);
              FbleTypeRelease(arena, &union_type->_base);
              FbleTypeRelease(arena, return_type);
              FbleTypeRelease(arena, arg_type);
              return NULL;
            }
            FbleTypeRelease(arena, arg_type);
          }
          choice++;
        } else if (select_expr->default_ == NULL) {
          if (choice < select_expr->choices.size) {
            ReportError(arena_, &select_expr->choices.xs[choice].name.loc,
                "expected tag '%n', but found '%n'\n",
                &union_type->fields.xs[i].name, &select_expr->choices.xs[choice].name);
          } else {
            ReportError(arena_, &expr->loc,
                "missing tag '%n'\n",
                &union_type->fields.xs[i].name);
          }
          FbleTypeRelease(arena, &union_type->_base);
          FbleTypeRelease(arena, return_type);
          FbleTypeRelease(arena, type);
          return NULL;
        } else {
          enter_gotos[i]->pc = default_pc;
        }
      }
      FbleTypeRelease(arena, &union_type->_base);
      FbleTypeRelease(arena, type);

      if (choice < select_expr->choices.size) {
        ReportError(arena_, &select_expr->choices.xs[choice].name.loc,
            "unexpected tag '%n'\n",
            &select_expr->choices.xs[choice]);
        FbleTypeRelease(arena, return_type);
        return NULL;
      }

      if (!exit) {
        if (exit_goto_default != NULL) {
          exit_goto_default->pc = scope->code->instrs.size;
        }
        for (size_t i = 0; i < select_expr->choices.size; ++i) {
          exit_gotos[i]->pc = scope->code->instrs.size;
        }
      }
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      FbleType* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        arg_types[i] = CompileType(arena, scope, func_value_expr->args.xs[i].type);
        error = error || arg_types[i] == NULL;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&func_value_expr->args.xs[i].name, &func_value_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena_, &func_value_expr->args.xs[i].name.loc,
                "duplicate arg name '%n'\n",
                &func_value_expr->args.xs[i].name);
          }
        }
      }

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(arena, arg_types[i]);
        }
        return NULL;
      }

      FbleFuncValueInstr* instr = FbleAlloc(arena_, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena_);
      instr->argc = argc;

      Scope func_scope;
      InitScope(arena_, &func_scope, instr->code, false, scope);
      EnterBodyBlock(arena_, blocks, func_value_expr->body->loc, &func_scope);

      for (size_t i = 0; i < argc; ++i) {
        PushVar(arena_, &func_scope, func_value_expr->args.xs[i].name, arg_types[i]);
      }

      FbleType* type = CompileExpr(arena, blocks, true, &func_scope, func_value_expr->body);
      ExitBlock(arena_, blocks, NULL);
      if (type == NULL) {
        FinishScope(arena_, &func_scope);
        FreeInstr(arena_, &instr->_base);
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(arena, arg_types[i]);
        }
        return NULL;
      }

      FinishScope(arena_, &func_scope);

      // TODO: Is it right for time to be proportional to number of captured
      // variables?
      AddBlockTime(blocks, instr->code->statics);
      AppendInstr(arena_, scope, &instr->_base);

      CompileExit(arena_, exit, scope);

      for (size_t i = 0; i < argc; ++i) {
        FbleType* arg_type = arg_types[argc - 1 - i];
        FbleFuncType* ft = FbleAlloc(arena_, FbleFuncType);
        FbleRefInit(arena, &ft->_base.ref);
        ft->_base.tag = FBLE_FUNC_TYPE;
        ft->_base.loc = expr->loc;
        ft->_base.id = (uintptr_t)ft;
        ft->arg = arg_type;
        ft->rtype = type;

        FbleRefAdd(arena, &ft->_base.ref, &ft->arg->ref);
        FbleTypeRelease(arena, ft->arg);

        FbleRefAdd(arena, &ft->_base.ref, &ft->rtype->ref);
        FbleTypeRelease(arena, ft->rtype);
        type = &ft->_base;
      }

      return type;
    }

    case FBLE_EVAL_EXPR: {
      AddBlockTime(blocks, 1);
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;


      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena_);

      Scope eval_scope;
      InitScope(arena_, &eval_scope, instr->code, false, scope);
      EnterBodyBlock(arena_, blocks, expr->loc, &eval_scope);

      FbleType* type = CompileExpr(arena, blocks, false, &eval_scope, eval_expr->body);

      CompileExit(arena_, true, &eval_scope);
      ExitBlock(arena_, blocks, NULL);

      FinishScope(arena_, &eval_scope);
      AppendInstr(arena_, scope, &instr->_base);
      CompileExit(arena_, exit, scope);

      if (type == NULL) {
        return NULL;
      }

      FbleProcType* proc_type = FbleAlloc(arena_, FbleProcType);
      FbleRefInit(arena, &proc_type->_base.ref);
      proc_type->_base.tag = FBLE_PROC_TYPE;
      proc_type->_base.loc = expr->loc;
      proc_type->_base.id = (uintptr_t)proc_type;
      proc_type->type = type;
      FbleRefAdd(arena, &proc_type->_base.ref, &proc_type->type->ref);
      FbleTypeRelease(arena, proc_type->type);
      return &proc_type->_base;
    }

    case FBLE_LINK_EXPR: {
      AddBlockTime(blocks, 1);
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      if (FbleNamesEqual(&link_expr->get, &link_expr->put)) {
        ReportError(arena_, &link_expr->put.loc,
            "duplicate port name '%n'\n",
            &link_expr->put);
        return NULL;
      }

      FbleType* port_type = CompileType(arena, scope, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      FbleProcType* get_type = FbleAlloc(arena_, FbleProcType);
      FbleRefInit(arena, &get_type->_base.ref);
      get_type->_base.tag = FBLE_PROC_TYPE;
      get_type->_base.loc = port_type->loc;
      get_type->_base.id = (uintptr_t)get_type;
      get_type->type = port_type;
      FbleRefAdd(arena, &get_type->_base.ref, &get_type->type->ref);

      FbleStructType* unit_type = FbleAlloc(arena_, FbleStructType);
      FbleRefInit(arena, &unit_type->_base.ref);
      unit_type->_base.tag = FBLE_STRUCT_TYPE;
      unit_type->_base.loc = expr->loc;
      unit_type->_base.id = (uintptr_t)unit_type;
      FbleVectorInit(arena_, unit_type->fields);

      FbleProcType* unit_proc_type = FbleAlloc(arena_, FbleProcType);
      FbleRefInit(arena, &unit_proc_type->_base.ref);
      unit_proc_type->_base.tag = FBLE_PROC_TYPE;
      unit_proc_type->_base.loc = expr->loc;
      unit_proc_type->_base.id = (uintptr_t)unit_proc_type;
      unit_proc_type->type = &unit_type->_base;
      FbleRefAdd(arena, &unit_proc_type->_base.ref, &unit_proc_type->type->ref);
      FbleTypeRelease(arena, &unit_type->_base);

      FbleFuncType* put_type = FbleAlloc(arena_, FbleFuncType);
      FbleRefInit(arena, &put_type->_base.ref);
      put_type->_base.tag = FBLE_FUNC_TYPE;
      put_type->_base.loc = expr->loc;
      put_type->_base.id = (uintptr_t)put_type;
      put_type->arg = port_type;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->arg->ref);
      put_type->rtype = &unit_proc_type->_base;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->rtype->ref);
      FbleTypeRelease(arena, &unit_proc_type->_base);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena_);

      Scope body_scope;
      InitScope(arena_, &body_scope, instr->code, false, scope);
      EnterBodyBlock(arena_, blocks, link_expr->body->loc, &body_scope);

      FbleLinkInstr* link = FbleAlloc(arena_, FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;

      Var* get_var = PushVar(arena_, &body_scope, link_expr->get, &get_type->_base);
      link->get_index = get_var->index.index;

      Var* put_var = PushVar(arena_, &body_scope, link_expr->put, &put_type->_base);
      link->put_index = put_var->index.index;

      AppendInstr(arena_, &body_scope, &link->_base);

      FbleType* type = CompileExpr(arena, blocks, false, &body_scope, link_expr->body);

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      proc->exit = true;
      AppendInstr(arena_, &body_scope, &proc->_base);
      ExitBlock(arena_, blocks, NULL);

      FinishScope(arena_, &body_scope);
      AppendInstr(arena_, scope, &instr->_base);
      CompileExit(arena_, exit, scope);

      FbleTypeRelease(arena, port_type);
      FbleTypeRelease(arena, &get_type->_base);
      FbleTypeRelease(arena, &put_type->_base);

      if (type == NULL) {
        FbleTypeRelease(arena, type);
        return NULL;
      }

      FbleType* proc_type = FbleNormalType(arena, type);
      if (proc_type->tag != FBLE_PROC_TYPE) {
        ReportError(arena_, &link_expr->body->loc,
            "expected a value of type proc, but found %t\n",
            type);
        FbleTypeRelease(arena, proc_type);
        FbleTypeRelease(arena, type);
        return NULL;
      }
      FbleTypeRelease(arena, proc_type);
      return type;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
      bool error = false;

      AddBlockTime(blocks, 1 + exec_expr->bindings.size);

      // Evaluate the types of the bindings and set up the new vars.
      Var nvd[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        nvd[i].name = exec_expr->bindings.xs[i].name;
        nvd[i].type = CompileType(arena, scope, exec_expr->bindings.xs[i].type);
        error = error || (nvd[i].type == NULL);
      }

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena_);

      Scope body_scope;
      InitScope(arena_, &body_scope, instr->code, false, scope);
      EnterBodyBlock(arena_, blocks, exec_expr->body->loc, &body_scope);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        FbleProcValueInstr* binstr = FbleAlloc(arena_, FbleProcValueInstr);
        binstr->_base.tag = FBLE_PROC_VALUE_INSTR;
        binstr->code = NewInstrBlock(arena_);

        Scope binding_scope;
        InitScope(arena_, &binding_scope, binstr->code, false, &body_scope);
        EnterBodyBlock(arena_, blocks, exec_expr->bindings.xs[i].expr->loc, &binding_scope);

        FbleType* type = CompileExpr(arena, blocks, false, &binding_scope, exec_expr->bindings.xs[i].expr);

        FbleProcInstr* bproc = FbleAlloc(arena_, FbleProcInstr);
        bproc->_base.tag = FBLE_PROC_INSTR;
        bproc->exit = true;
        AppendInstr(arena_, &binding_scope, &bproc->_base);
        ExitBlock(arena_, blocks, NULL);

        FinishScope(arena_, &binding_scope);
        AppendInstr(arena_, &body_scope, &binstr->_base);

        error = error || (type == NULL);

        if (type != NULL) {
          FbleProcType* proc_type = (FbleProcType*)FbleNormalType(arena, type);
          if (proc_type->_base.tag == FBLE_PROC_TYPE) {
            if (nvd[i].type != NULL && !FbleTypesEqual(arena, nvd[i].type, proc_type->type)) {
              error = true;
              ReportError(arena_, &exec_expr->bindings.xs[i].expr->loc,
                  "expected type %t!, but found %t\n",
                  nvd[i].type, type);
            }
          } else {
            error = true;
            ReportError(arena_, &exec_expr->bindings.xs[i].expr->loc,
                "expected process, but found expression of type %t\n",
                type);
          }
          FbleTypeRelease(arena, &proc_type->_base);
          FbleTypeRelease(arena, type);
        }
      }

      FbleForkInstr* fork = FbleAlloc(arena_, FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      AppendInstr(arena_, &body_scope, &fork->_base);
      fork->args.xs = FbleArrayAlloc(arena_, FbleLocalIndex, exec_expr->bindings.size);
      fork->args.size = exec_expr->bindings.size;

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        Var* v = PushVar(arena_, &body_scope, nvd[i].name, nvd[i].type);
        fork->args.xs[i] = v->index.index;
      }

      FbleJoinInstr* join = FbleAlloc(arena_, FbleJoinInstr);
      join->_base.tag = FBLE_JOIN_INSTR;
      AppendInstr(arena_, &body_scope, &join->_base);


      FbleType* rtype = NULL;
      if (!error) {
        rtype = CompileExpr(arena, blocks, false, &body_scope, exec_expr->body);
        error = (rtype == NULL);
      }

      if (rtype != NULL) {
        FbleType* normal = FbleNormalType(arena, rtype);
        if (normal->tag != FBLE_PROC_TYPE) {
          error = true;
          ReportError(arena_, &exec_expr->body->loc,
              "expected a value of type proc, but found %t\n",
              rtype);
        }
        FbleTypeRelease(arena, normal);
      }

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      proc->exit = true;
      AppendInstr(arena_, &body_scope, &proc->_base);
      ExitBlock(arena_, blocks, NULL);

      FinishScope(arena_, &body_scope);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        FbleTypeRelease(arena, nvd[i].type);
      }

      AppendInstr(arena_, scope, &instr->_base);
      CompileExit(arena_, exit, scope);

      if (error) {
        FbleTypeRelease(arena, rtype);
        return NULL;
      }

      return rtype;
    }

    case FBLE_VAR_EXPR: {
      AddBlockTime(blocks, 1);
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      Var* var = GetVar(arena_, scope, var_expr->var, false);
      if (var == NULL) {
        ReportError(arena_, &var_expr->var.loc, "variable '%n' not defined\n",
            &var_expr->var);
        return NULL;
      }

      FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
      instr->_base.tag = FBLE_VAR_INSTR;
      instr->index = var->index;
      AppendInstr(arena_, scope, &instr->_base);
      CompileExit(arena_, exit, scope);
      return FbleTypeRetain(arena, var->type);
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;
      AddBlockTime(blocks, 1 + let_expr->bindings.size);

      // Evaluate the types of the bindings and set up the new vars.
      Var nvd[let_expr->bindings.size];
      FbleVarType* var_types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;
        var_types[i] = NULL;

        nvd[i].name = let_expr->bindings.xs[i].name;
        if (binding->type == NULL) {
          assert(binding->kind != NULL);
          FbleVarType* var = FbleAlloc(arena_, FbleVarType);
          FbleRefInit(arena, &var->_base.ref);
          var->_base.tag = FBLE_VAR_TYPE;
          var->_base.loc = binding->name.loc;
          var->_base.id = (uintptr_t)var;
          var->name = let_expr->bindings.xs[i].name;
          var->kind = FbleKindRetain(arena_, binding->kind);
          var->value = NULL;

          FbleTypeType* type_type = FbleAlloc(arena_, FbleTypeType);
          FbleRefInit(arena, &type_type->_base.ref);
          type_type->_base.tag = FBLE_TYPE_TYPE;
          type_type->_base.loc = binding->name.loc;
          type_type->_base.id = (uintptr_t)type_type;
          type_type->type = &var->_base;
          FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
          FbleTypeRelease(arena, &var->_base);

          nvd[i].type = &type_type->_base;
          var_types[i] = var;
        } else {
          assert(binding->kind == NULL);
          nvd[i].type = CompileType(arena, scope, binding->type);
          error = error || (nvd[i].type == NULL);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&let_expr->bindings.xs[i].name, &let_expr->bindings.xs[j].name)) {
            ReportError(arena_, &let_expr->bindings.xs[i].name.loc,
                "duplicate variable name '%n'\n",
                &let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Var* vars[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        vars[i] = PushVar(arena_, scope, nvd[i].name, nvd[i].type);
        FbleRefValueInstr* ref_instr = FbleAlloc(arena_, FbleRefValueInstr);
        ref_instr->_base.tag = FBLE_REF_VALUE_INSTR;
        ref_instr->index = vars[i]->index.index;
        AppendInstr(arena_, scope, &ref_instr->_base);
      }

      // Compile the values of the variables.
      FbleType* var_type_values[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        var_type_values[i] = NULL;
        FbleBinding* binding = let_expr->bindings.xs + i;

        FbleType* type = NULL;
        if (!error) {
          EnterBlock(arena_, blocks, binding->name, binding->expr->loc, scope);
          type = CompileExpr(arena, blocks, false, scope, binding->expr);
          ExitBlock(arena_, blocks, scope);
        }
        error = error || (type == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(arena, nvd[i].type, type)) {
          error = true;
          ReportError(arena_, &binding->expr->loc,
              "expected type %t, but found %t\n",
              nvd[i].type, type);
        } else if (!error && binding->type == NULL) {
          FbleVarType* var = var_types[i];
          var_type_values[i] = ValueOfType(arena, type);
          if (var_type_values[i] == NULL) {
            ReportError(arena_, &binding->expr->loc,
                "expected type, but found something of type %t\n",
                type);
            error = true;
          } else {
            FbleKind* expected_kind = var->kind;
            FbleKind* actual_kind = FbleGetKind(arena_, var_type_values[i]);
            if (!FbleKindsEqual(expected_kind, actual_kind)) {
              ReportError(arena_, &binding->expr->loc,
                  "expected kind %k, but found %k\n",
                  expected_kind, actual_kind);
              error = true;
            }
            FbleKindRelease(arena_, actual_kind);
          }
        }

        FbleTypeRelease(arena, type);
      }

      // Check to see if this is a recursive let block.
      bool recursive = false;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        recursive = recursive || vars[i]->used;
      }

      // Apply the newly computed type values for variables whose types were
      // previously unknown.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (var_type_values[i] != NULL) {
          FbleVarType* var = var_types[i];
          assert(var != NULL);
          var->value = var_type_values[i];
          FbleRefAdd(arena, &var->_base.ref, &var->value->ref);
          FbleTypeRelease(arena, var->value);
        }
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (var_type_values[i] != NULL && FbleTypeIsVacuous(arena, &var_types[i]->_base)) {
          ReportError(arena_, &let_expr->bindings.xs[i].name.loc,
              "%n is vacuous\n", &let_expr->bindings.xs[i].name);
          error = true;
        }

        FbleRefDefInstr* ref_def_instr = FbleAlloc(arena_, FbleRefDefInstr);
        ref_def_instr->_base.tag = FBLE_REF_DEF_INSTR;
        ref_def_instr->index = vars[let_expr->bindings.size - i - 1]->index.index;
        ref_def_instr->recursive = recursive;
        AppendInstr(arena_, scope, &ref_def_instr->_base);
      }


      FbleType* rtype = NULL;
      if (!error) {
        rtype = CompileExpr(arena, blocks, exit, scope, let_expr->body);
        error = (rtype == NULL);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (!exit) {
          FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
          descope->_base.tag = FBLE_DESCOPE_INSTR;
          descope->index = vars[let_expr->bindings.size - i - 1]->index.index;
          AppendInstr(arena_, scope, &descope->_base);
        }
        PopVar(arena_, scope);
        FbleTypeRelease(arena, nvd[i].type);
      }

      if (error) {
        FbleTypeRelease(arena, rtype);
        return NULL;
      }
      return rtype;
    }

    case FBLE_MODULE_REF_EXPR: {
      AddBlockTime(blocks, 1);
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      Var* var = GetVar(arena_, scope, module_ref_expr->ref.resolved, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");

      FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
      instr->_base.tag = FBLE_VAR_INSTR;
      instr->index = var->index;
      AppendInstr(arena_, scope, &instr->_base);
      CompileExit(arena_, exit, scope);
      return FbleTypeRetain(arena, var->type);
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      FbleVarType* arg = FbleAlloc(arena_, FbleVarType);
      FbleRefInit(arena, &arg->_base.ref);
      arg->_base.tag = FBLE_VAR_TYPE;
      arg->_base.loc = poly->arg.name.loc;
      arg->_base.id = (uintptr_t)arg;
      arg->name = poly->arg.name;
      arg->kind = FbleKindRetain(arena_, poly->arg.kind);
      arg->value = NULL;

      FbleTypeType* type_type = FbleAlloc(arena_, FbleTypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = FBLE_TYPE_TYPE;
      type_type->_base.loc = poly->arg.name.loc;
      type_type->_base.id = (uintptr_t)type_type;
      type_type->type = &arg->_base;
      FbleRefAdd(arena, &type_type->_base.ref, &arg->_base.ref);

      // TODO: It's a little silly that we are pushing an empty type value
      // here. Oh well. Maybe in the future we'll optimize those away or
      // add support for non-type poly args too.
      AddBlockTime(blocks, 1);
      FbleTypeInstr* type_instr = FbleAlloc(arena_, FbleTypeInstr);
      type_instr->_base.tag = FBLE_TYPE_INSTR;
      AppendInstr(arena_, scope, &type_instr->_base);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      AppendInstr(arena_, scope, &vpush->_base);
      Var* var = PushVar(arena_, scope, poly->arg.name, &type_type->_base);
      vpush->index = var->index.index;

      FbleType* body = CompileExpr(arena, blocks, exit, scope, poly->body);

      if (!exit) {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->index = var->index.index;
        AppendInstr(arena_, scope, &descope->_base);
      }

      PopVar(arena_, scope);
      FbleTypeRelease(arena, &type_type->_base);

      if (body == NULL) {
        FbleTypeRelease(arena, &arg->_base);
        return NULL;
      }

      FbleType* pt = FbleNewPolyType(arena, expr->loc, &arg->_base, body);
      FbleTypeRelease(arena, &arg->_base);
      FbleTypeRelease(arena, body);
      return pt;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // CompileExpr gives us typeof(poly)
      FbleType* poly = CompileExpr(arena, blocks, exit, scope, apply->poly);
      if (poly == NULL) {
        return NULL;
      }

      FblePolyKind* poly_kind = (FblePolyKind*)FbleGetKind(arena_, poly);
      if (poly_kind->_base.tag != FBLE_POLY_KIND) {
        ReportError(arena_, &expr->loc,
            "cannot apply poly args to a basic kinded entity\n");
        FbleKindRelease(arena_, &poly_kind->_base);
        FbleTypeRelease(arena, poly);
        return NULL;
      }

      // Note: CompileType gives us the value of arg
      FbleType* arg = CompileType(arena, scope, apply->arg);
      if (arg == NULL) {
        FbleKindRelease(arena_, &poly_kind->_base);
        FbleTypeRelease(arena, poly);
        return NULL;
      }

      FbleKind* expected_kind = poly_kind->arg;
      FbleKind* actual_kind = FbleGetKind(arena_, arg);
      if (!FbleKindsEqual(expected_kind, actual_kind)) {
        ReportError(arena_, &apply->arg->loc,
            "expected kind %k, but found %k\n",
            expected_kind, actual_kind);
        FbleKindRelease(arena_, &poly_kind->_base);
        FbleKindRelease(arena_, actual_kind);
        FbleTypeRelease(arena, poly);
        FbleTypeRelease(arena, arg);
        return NULL;
      }
      FbleKindRelease(arena_, actual_kind);
      FbleKindRelease(arena_, &poly_kind->_base);

      FbleType* pat = FbleNewPolyApplyType(arena, expr->loc, poly, arg);
      FbleTypeRelease(arena, poly);
      FbleTypeRelease(arena, arg);
      return pat;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      return CompileList(arena, blocks, exit, scope, expr->loc, list_expr->args);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      // TODO: Check that the type compiles before desugaring the literal
      // expression? Otherwise we could end up giving a lot of duplicate error
      // messages.

      // TODO: Don't re-evaluate the type for each element of the literal!

      size_t n = strlen(literal->word);

      if (n == 0) {
        ReportError(arena_, &literal->word_loc,
            "literals must not be empty\n");
        return NULL;
      }

      FbleMiscAccessExpr letters[n];
      FbleExpr* xs[n];
      char word_letters[2*n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        word_letters[2*i] = literal->word[i];
        word_letters[2*i + 1] = '\0';

        letters[i]._base.tag = FBLE_MISC_ACCESS_EXPR;
        letters[i]._base.loc = loc;
        letters[i].object = literal->type;
        letters[i].field.name = word_letters + 2*i;
        letters[i].field.space = FBLE_NORMAL_NAME_SPACE;
        letters[i].field.loc = loc;

        xs[i] = &letters[i]._base;

        if (literal->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      FbleExprV args = { .size = n, .xs = xs, };
      return CompileList(arena, blocks, exit, scope, literal->word_loc, args);
    }

    case FBLE_STRUCT_IMPORT_EXPR: {
      AddBlockTime(blocks, 1);
      FbleStructImportExpr* struct_import_expr = (FbleStructImportExpr*)expr;

      FbleType* type = CompileExpr(arena, blocks, false, scope, struct_import_expr->nspace);
      if (type == NULL) {
        return NULL;
      }

      FbleStructType* struct_type = (FbleStructType*)FbleNormalType(arena, type);
      if (struct_type->_base.tag != FBLE_STRUCT_TYPE) {
        ReportError(arena_, &struct_import_expr->nspace->loc,
            "expected value of type struct, but found value of type %t\n",
            type);

        FbleTypeRelease(arena, &struct_type->_base);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      AddBlockTime(blocks, 1 + struct_type->fields.size);

      FbleStructImportInstr* struct_import = FbleAlloc(arena_, FbleStructImportInstr);
      struct_import->_base.tag = FBLE_STRUCT_IMPORT_INSTR;
      struct_import->loc = struct_import_expr->nspace->loc;
      struct_import->fields.xs = FbleArrayAlloc(arena_, FbleLocalIndex, struct_type->fields.size);
      struct_import->fields.size = struct_type->fields.size;
      AppendInstr(arena_, scope, &struct_import->_base);

      Var* vars[struct_type->fields.size];
      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        vars[i] = PushVar(arena_, scope, struct_type->fields.xs[i].name, struct_type->fields.xs[i].type);
        struct_import->fields.xs[i] = vars[i]->index.index;
      }

      FbleType* rtype = CompileExpr(arena, blocks, exit, scope, struct_import_expr->body);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        if (!exit) {
          FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
          descope->_base.tag = FBLE_DESCOPE_INSTR;
          descope->index = vars[struct_type->fields.size - i - 1]->index.index;
          AppendInstr(arena_, scope, &descope->_base);
        }
        PopVar(arena_, scope);
      }

      FbleTypeRelease(arena, &struct_type->_base);
      FbleTypeRelease(arena, type);
      return rtype;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileList --
//   Type check and compile a list expression. Returns the type of the
//   expression and generates instructions to compute the value of that
//   expression at runtime.
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
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Updates blocks with compiled block information.
//   Appends instructions to scope for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   scope if the expression fails to compile.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
//   Behavior is undefined if there is not at least one list argument.
static FbleType* CompileList(FbleTypeArena* arena, Blocks* blocks, bool exit, Scope* scope, FbleLoc loc, FbleExprV args)
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

  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleBasicKind* basic_kind = FbleAlloc(arena_, FbleBasicKind);
  basic_kind->_base.tag = FBLE_BASIC_KIND;
  basic_kind->_base.loc = loc;
  basic_kind->_base.refcount = 1;
  basic_kind->level = 1;

  FbleName elem_type_name = {
    .name = "T",
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
    char* name = FbleArrayAlloc(arena_, char, num_digits + 2);
    name[0] = 'x';
    name[num_digits+1] = '\0';
    for (size_t j = 0, x = i; j < num_digits; j++, x /= 10) {
      name[num_digits - j] = (x % 10) + '0';
    }
    arg_names[i].name = name;
    arg_names[i].space = FBLE_NORMAL_NAME_SPACE;
    arg_names[i].loc = loc;

    arg_values[i]._base.tag = FBLE_VAR_EXPR;
    arg_values[i]._base.loc = loc;
    arg_values[i].var = arg_names[i];
  }

  FbleName list_type_name = {
    .name = "L",
    .space = FBLE_TYPE_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr list_type = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = list_type_name,
  };

  FbleField inner_args[2];
  FbleName cons_name = {
    .name = "cons",
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr cons = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = cons_name,
  };

  // L@ -> L@
  FbleFuncTypeExpr cons_type_inner = {
    ._base = { .tag = FBLE_FUNC_TYPE_EXPR, .loc = loc, },
    .arg = &list_type._base,
    .rtype = &list_type._base,
  };

  // T@ -> (L@ -> L@)
  FbleFuncTypeExpr cons_type = {
    ._base = { .tag = FBLE_FUNC_TYPE_EXPR, .loc = loc, },
    .arg = &elem_type._base,
    .rtype = &cons_type_inner._base,
  };

  inner_args[0].type = &cons_type._base;
  inner_args[0].name = cons_name;

  FbleName nil_name = {
    .name = "nil",
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = loc,
  };

  FbleVarExpr nil = {
    ._base = { .tag = FBLE_VAR_EXPR, .loc = loc, },
    .var = nil_name,
  };

  inner_args[1].type = &list_type._base;
  inner_args[1].name = nil_name;

  FbleMiscApplyExpr applys[args.size];
  FbleExpr* all_args[args.size * 2];
  for (size_t i = 0; i < args.size; ++i) {
    applys[i]._base.tag = FBLE_MISC_APPLY_EXPR;
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

  FbleField outer_args[args.size];
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

  FbleMiscApplyExpr apply_elems = {
    ._base = { .tag = FBLE_MISC_APPLY_EXPR, .loc = loc },
    .misc = &apply_type._base,
    .args = args,
  };

  FbleExpr* expr = &apply_elems._base;

  FbleType* result = CompileExpr(arena, blocks, exit, scope, expr);

  FbleKindRelease(arena_, &basic_kind->_base);
  for (size_t i = 0; i < args.size; i++) {
    FbleFree(arena_, (void*)arg_names[i].name);
  }
  return result;
}

// CompileExprNoInstrs --
//   Type check and compile the given expression. Returns the type of the
//   expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static FbleType* CompileExprNoInstrs(FbleTypeArena* arena, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleInstrBlock* code = NewInstrBlock(arena_);

  Scope nscope;
  InitScope(arena_, &nscope, code, true, scope);

  Blocks blocks;
  FbleVectorInit(arena_, blocks.stack);
  FbleVectorInit(arena_, blocks.blocks);
  FbleType* type = CompileExpr(arena, &blocks, true, &nscope, expr);
  FbleFree(arena_, blocks.stack.xs);
  FbleFreeBlockNames(arena_, &blocks.blocks);
  FinishScope(arena_, &nscope);
  FbleFreeInstrBlock(arena_, code);
  return type;
}

// CompileType --
//   Compile a type, returning its value.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the value of variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static FbleType* CompileType(FbleTypeArena* arena, Scope* scope, FbleTypeExpr* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* struct_type = (FbleStructTypeExpr*)type;
      FbleStructType* st = FbleAlloc(arena_, FbleStructType);
      FbleRefInit(arena, &st->_base.ref);
      st->_base.tag = FBLE_STRUCT_TYPE;
      st->_base.loc = type->loc;
      st->_base.id = (uintptr_t)st;
      FbleVectorInit(arena_, st->fields);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        FbleType* compiled = CompileType(arena, scope, field->type);
        if (compiled == NULL) {
          FbleTypeRelease(arena, &st->_base);
          return NULL;
        }

        if (!CheckNameSpace(arena, &field->name, compiled)) {
          FbleTypeRelease(arena, compiled);
          FbleTypeRelease(arena, &st->_base);
          return NULL;
        }

        FbleTaggedType* cfield = FbleVectorExtend(arena_, st->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        FbleRefAdd(arena, &st->_base.ref, &cfield->type->ref);
        FbleTypeRelease(arena, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&field->name, &struct_type->fields.xs[j].name)) {
            ReportError(arena_, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
            FbleTypeRelease(arena, &st->_base);
            return NULL;
          }
        }
      }
      return &st->_base;
    }

    case FBLE_UNION_TYPE_EXPR: {
      FbleUnionType* ut = FbleAlloc(arena_, FbleUnionType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.tag = FBLE_UNION_TYPE;
      ut->_base.loc = type->loc;
      ut->_base.id = (uintptr_t)ut;
      FbleVectorInit(arena_, ut->fields);

      FbleUnionTypeExpr* union_type = (FbleUnionTypeExpr*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        FbleType* compiled = CompileType(arena, scope, field->type);
        if (compiled == NULL) {
          FbleTypeRelease(arena, &ut->_base);
          return NULL;
        }
        FbleTaggedType* cfield = FbleVectorExtend(arena_, ut->fields);
        cfield->name = field->name;
        cfield->type = compiled;
        FbleRefAdd(arena, &ut->_base.ref, &cfield->type->ref);
        FbleTypeRelease(arena, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&field->name, &union_type->fields.xs[j].name)) {
            ReportError(arena_, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
            FbleTypeRelease(arena, &ut->_base);
            return NULL;
          }
        }
      }
      return &ut->_base;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncType* ft = FbleAlloc(arena_, FbleFuncType);
      FbleRefInit(arena, &ft->_base.ref);
      ft->_base.tag = FBLE_FUNC_TYPE;
      ft->_base.loc = type->loc;
      ft->_base.id = (uintptr_t)ft;
      ft->arg = NULL;
      ft->rtype = NULL;

      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;
      ft->arg = CompileType(arena, scope, func_type->arg);
      if (ft->arg == NULL) {
        FbleTypeRelease(arena, &ft->_base);
        return NULL;
      }
      FbleRefAdd(arena, &ft->_base.ref, &ft->arg->ref);
      FbleTypeRelease(arena, ft->arg);

      FbleType* rtype = CompileType(arena, scope, func_type->rtype);
      if (rtype == NULL) {
        FbleTypeRelease(arena, &ft->_base);
        return NULL;
      }
      ft->rtype = rtype;
      FbleRefAdd(arena, &ft->_base.ref, &ft->rtype->ref);
      FbleTypeRelease(arena, ft->rtype);
      return &ft->_base;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcType* ut = FbleAlloc(arena_, FbleProcType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.tag = FBLE_PROC_TYPE;
      ut->_base.loc = type->loc;
      ut->_base.id = (uintptr_t)ut;
      ut->type = NULL;

      FbleProcTypeExpr* unary_type = (FbleProcTypeExpr*)type;
      ut->type = CompileType(arena, scope, unary_type->type);
      if (ut->type == NULL) {
        FbleTypeRelease(arena, &ut->_base);
        return NULL;
      }
      FbleRefAdd(arena, &ut->_base.ref, &ut->type->ref);
      FbleTypeRelease(arena, ut->type);
      return &ut->_base;
    }

    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* typeof = (FbleTypeofExpr*)type;
      return CompileExprNoInstrs(arena, scope, typeof->expr);
    }

    case FBLE_MISC_APPLY_EXPR:
    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR:
    case FBLE_UNION_VALUE_EXPR:
    case FBLE_MISC_ACCESS_EXPR:
    case FBLE_UNION_SELECT_EXPR:
    case FBLE_FUNC_VALUE_EXPR:
    case FBLE_EVAL_EXPR:
    case FBLE_LINK_EXPR:
    case FBLE_EXEC_EXPR:
    case FBLE_VAR_EXPR:
    case FBLE_MODULE_REF_EXPR:
    case FBLE_LET_EXPR:
    case FBLE_POLY_EXPR:
    case FBLE_POLY_APPLY_EXPR:
    case FBLE_LIST_EXPR:
    case FBLE_LITERAL_EXPR:
    case FBLE_STRUCT_IMPORT_EXPR: {
      FbleExpr* expr = type;
      FbleType* type = CompileExprNoInstrs(arena, scope, expr);
      if (type == NULL) {
        return NULL;
      }

      FbleType* type_value = ValueOfType(arena, type);
      FbleTypeRelease(arena, type);
      if (type_value == NULL) {
        ReportError(arena_, &expr->loc,
            "expected a type, but found value of type %t\n",
            type);
        return NULL;
      }
      return type_value;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileProgram --
//   Type check and compile the given program. Returns the type of the
//   program and generates instructions to compute the value of that
//   program at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   scope - the list of variables in scope.
//   prgm - the program to compile.
//
// Results:
//   The type of the program, or NULL if the program is not well typed.
//
// Side effects:
//   Updates 'blocks' with compiled block information. Exits the current
//   block.
//   Appends instructions to scope for executing the given program.
//   There is no gaurentee about what instructions have been appended to
//   scope if the program fails to compile.
//   Prints a message to stderr if the program fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static FbleType* CompileProgram(FbleTypeArena* arena, Blocks* blocks, Scope* scope, FbleProgram* prgm)
{
  AddBlockTime(blocks, 1 + prgm->modules.size);

  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleType* types[prgm->modules.size];
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    EnterBlock(arena_, blocks, prgm->modules.xs[i].name, prgm->modules.xs[i].value->loc, scope);
    types[i] = CompileExpr(arena, blocks, false, scope, prgm->modules.xs[i].value);
    ExitBlock(arena_, blocks, scope);

    if (types[i] == NULL) {
      for (size_t j = 0; j < i; ++j) {
        FbleTypeRelease(arena, types[j]);
      }
      return NULL;
    }

    FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
    vpush->_base.tag = FBLE_VPUSH_INSTR;
    AppendInstr(arena_, scope, &vpush->_base);
    Var* v = PushVar(arena_, scope, prgm->modules.xs[i].name, types[i]);
    vpush->index = v->index.index;
  }

  FbleType* rtype = CompileExpr(arena, blocks, true, scope, prgm->main);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    PopVar(arena_, scope);
    FbleTypeRelease(arena, types[i]);
  }

  if (rtype == NULL) {
    return NULL;
  }

  return rtype;
}

// FbleFreeInstrBlock -- see documentation in internal.h
void FbleFreeInstrBlock(FbleArena* arena, FbleInstrBlock* block)
{
  if (block == NULL) {
    return;
  }

  assert(block->refcount > 0);
  block->refcount--;
  if (block->refcount == 0) {
    for (size_t i = 0; i < block->instrs.size; ++i) {
      FreeInstr(arena, block->instrs.xs[i]);
    }
    FbleFree(arena, block->instrs.xs);
    FbleFree(arena, block);
  }
}

// FbleCompile -- see documentation in internal.h
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleNameV* blocks, FbleProgram* program)
{
  Blocks block_stack;
  FbleVectorInit(arena, block_stack.stack);
  FbleVectorInit(arena, block_stack.blocks);

  FbleInstrBlock* code = NewInstrBlock(arena);

  FbleName entry_name = {
    .name = "",
    .loc = program->main->loc,
    .space = FBLE_NORMAL_NAME_SPACE
  };

  Scope scope;
  InitScope(arena, &scope, code, false, NULL);

  FbleTypeArena* type_arena = FbleNewTypeArena(arena);
  EnterBlock(arena, &block_stack, entry_name, program->main->loc, &scope);
  FbleType* type = CompileProgram(type_arena, &block_stack, &scope, program);
  ExitBlock(arena, &block_stack, NULL);
  FbleTypeRelease(type_arena, type);
  FbleFreeTypeArena(type_arena);

  assert(block_stack.stack.size == 0);
  FbleFree(arena, block_stack.stack.xs);
  *blocks = block_stack.blocks;

  FinishScope(arena, &scope);
  if (type == NULL) {
    FbleFreeInstrBlock(arena, code);
    return NULL;
  }
  return code;
}
