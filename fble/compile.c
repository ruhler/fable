// compile.c --
//   This file implements the fble compilation routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_arg, va_end
#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for strlen, strcat
#include <stdlib.h>   // for NULL

#include "fble-type.h"
#include "instr.h"

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
//   Information about a variable visible during type checking.
//
// name - the name of the variable.
// type - the type of the variable.
//   A reference to the type is owned by this Var.
// local - the type and location of the variable in the stack frame.
//   A reference to the local is owned by this Var.
// used  - true if the variable is used anywhere at runtime, false otherwise.
// accessed - true if the variable is referenced anywhere, false otherwise.
typedef struct {
  FbleName name;
  FbleType* type;
  Local* local;
  bool used;
  bool accessed;
} Var;

// VarV --
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
//     Takes ownership of the Vars.
//   vars - stack of local variables in scope order.
//     Takes ownership of the Vars.
//   locals - local values. Entries may be NULL to indicate a free slot.
//     Owns the Local.
//   code - the instruction block for this scope.
//   capture - A vector of variables captured from the parent scope. If NULL,
//             operations on this scope should not have any side
//             effects on the parent scope, which is used in the case when we
//             are compiling for types only, not for instructions.
//   parent - the parent of this scope. May be NULL.
typedef struct Scope {
  VarV statics;
  VarV vars;
  LocalV locals;
  FbleInstrBlock* code;
  FbleFrameIndexV* capture;
  struct Scope* parent;
} Scope;

static Local* NewLocal(FbleArena* arena, Scope* scope);
static void LocalRelease(FbleArena* arena, Scope* scope, Local* local);
static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type, Local* local);
static void PopVar(FbleTypeHeap* heap, Scope* scope);
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, FbleName name, bool phantom);

static FbleInstrBlock* NewInstrBlock(FbleArena* arena);
static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock* code, FbleFrameIndexV* capture, Scope* parent);
static void FreeScope(FbleTypeHeap* heap, Scope* scope);
static void AppendInstr(FbleArena* arena, Scope* scope, FbleInstr* instr);

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

static void EnterBlock(FbleArena* arena, Blocks* blocks, FbleName name, FbleLoc loc, Scope* scope);
static void EnterBodyBlock(FbleArena* arena, Blocks* blocks, FbleLoc loc, Scope* scope);
static void ExitBlock(FbleArena* arena, Blocks* blocks, Scope* scope);
static void AddBlockTime(Blocks* blocks, size_t time);

// Compiled --
//   A pair of type and local returned from compilation.
typedef struct {
  FbleType* type;
  Local* local;
} Compiled;

static Compiled COMPILE_FAILED = { .type = NULL, .local = NULL };

static void ReportError(FbleArena* arena, FbleLoc* loc, const char* fmt, ...);
static bool CheckNameSpace(FbleArena* arena, FbleName* name, FbleType* type);

static void CompileExit(FbleArena* arena, bool exit, Scope* scope, Local* result);
static Compiled CompileExpr(FbleTypeHeap* heap, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr);
static Compiled CompileList(FbleTypeHeap* heap, Blocks* blocks, bool exit, Scope* scope, FbleLoc loc, FbleExprV args);
static FbleType* CompileExprNoInstrs(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr);
static FbleType* CompileType(FbleTypeHeap* arena, Scope* scope, FbleTypeExpr* type);
static bool CompileProgram(FbleTypeHeap* arena, Blocks* blocks, Scope* scope, FbleProgram* prgm);

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
//
// Results:
//   none
//
// Side effects:
//   Decrements the reference count on the local and frees it if the refcount
//   drops to 0. Generates instructions to free the value at runtime as
//   appropriate.
static void LocalRelease(FbleArena* arena, Scope* scope, Local* local)
{
  local->refcount--;
  if (local->refcount == 0) {
    assert(local->index.section == FBLE_LOCALS_FRAME_SECTION);
    FbleReleaseInstr* release = FbleAlloc(arena, FbleReleaseInstr);
    release->_base.tag = FBLE_RELEASE_INSTR;
    release->value = local->index.index;
    AppendInstr(arena, scope, &release->_base);

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
//   type - the type of the variable.
//   local - the value of the variable.
//
// Results:
//   A pointer to the newly pushed variable. The pointer is owned by the
//   scope. It remains valid until a corresponding PopVar or FreeScope
//   occurs.
//
// Side effects:
//   Pushes a new variable with given name and type onto the scope. Takes
//   ownership of the given type and local, which will be released when the
//   variable is freed.
static Var* PushVar(FbleArena* arena, Scope* scope, FbleName name, FbleType* type, Local* local)
{
  Var* var = FbleAlloc(arena, Var);
  var->name = name;
  var->type = type;
  var->local = local;
  var->used = false;
  var->accessed = false;
  FbleVectorAppend(arena, scope->vars, var);
  return var;
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   heap - heap to use for allocations.
//   scope - the scope to pop from.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope. Invalidates the pointer to the variable
//   originally returned in PushVar.
static void PopVar(FbleTypeHeap* heap, Scope* scope)
{
  FbleArena* arena = heap->arena;

  scope->vars.size--;
  Var* var = scope->vars.xs[scope->vars.size];
  FbleTypeRelease(heap, var->type);
  LocalRelease(arena, scope, var->local);
  FbleFree(arena, var);
}

// GetVar --
//   Lookup a var in the given scope.
//
// Inputs:
//   heap - heap to use for allocations.
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
static Var* GetVar(FbleTypeHeap* heap, Scope* scope, FbleName name, bool phantom)
{
  for (size_t i = 0; i < scope->vars.size; ++i) {
    size_t j = scope->vars.size - i - 1;
    Var* var = scope->vars.xs[j];
    if (FbleNamesEqual(&name, &var->name)) {
      var->accessed = true;
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  for (size_t i = 0; i < scope->statics.size; ++i) {
    Var* var = scope->statics.xs[i];
    if (FbleNamesEqual(&name, &var->name)) {
      var->accessed = true;
      if (!phantom) {
        var->used = true;
      }
      return var;
    }
  }

  if (scope->parent != NULL) {
    Var* var = GetVar(heap, scope->parent, name, scope->capture == NULL || phantom);
    if (var != NULL) {
      if (phantom) {
        // It doesn't matter that we are returning a variable for the wrong
        // scope here. phantom means we won't actually use it ever.
        return var;
      }

      FbleArena* arena = heap->arena;
      Local* local = FbleAlloc(arena, Local);
      local->index.section = FBLE_STATICS_FRAME_SECTION;
      local->index.index = scope->statics.size;
      local->refcount = 1;

      Var* captured = FbleAlloc(arena, Var);
      captured->name = var->name;
      captured->type = FbleTypeRetain(heap, var->type);
      captured->local = local;
      captured->used = !phantom;
      captured->accessed = true;

      FbleVectorAppend(arena, scope->statics, captured);
      if (scope->statics.size > scope->code->statics) {
        scope->code->statics = scope->statics.size;
      }
      if (scope->capture) {
        FbleVectorAppend(arena, *scope->capture, var->local->index);
      }
      return captured;
    }
  }

  return NULL;
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

// InitScope --
//   Initialize a new scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   scope - the scope to initialize.
//   code - a pointer to the code block for this scope.
//   capture - vector to store capture variables from the parent scope in. May
//             be NULL.
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
//   captured from the parent scope. capture may be NULL, in which case the
//   scope is treated as a phantom scope that does not cause any changes to be
//   made to the parent scope.
static void InitScope(FbleArena* arena, Scope* scope, FbleInstrBlock* code, FbleFrameIndexV* capture, Scope* parent)
{
  FbleVectorInit(arena, scope->statics);
  FbleVectorInit(arena, scope->vars);
  FbleVectorInit(arena, scope->locals);
  if (capture) {
    FbleVectorInit(arena, *capture);
  }

  scope->code = code;
  scope->code->statics = 0;
  scope->code->locals = 0;
  scope->capture = capture;
  scope->parent = parent;
}

// FreeScope --
//   Free memory associated with a Scope.
//
// Inputs:
//   heap - heap to use for allocations
//   scope - the scope to finish.
//
// Results:
//   None.
//
// Side effects:
//   * Frees memory associated with scope.
static void FreeScope(FbleTypeHeap* heap, Scope* scope)
{
  FbleArena* arena = heap->arena;
  for (size_t i = 0; i < scope->statics.size; ++i) {
    FbleTypeRelease(heap, scope->statics.xs[i]->type);
    FbleFree(arena, scope->statics.xs[i]->local);
    FbleFree(arena, scope->statics.xs[i]);
  }
  FbleFree(arena, scope->statics.xs);

  while (scope->vars.size > 0) {
    PopVar(heap, scope);
  }
  FbleFree(arena, scope->vars.xs);

  for (size_t i = 0; i < scope->locals.size; ++i) {
    if (scope->locals.xs[i] != NULL) {
      FbleFree(arena, scope->locals.xs[i]);
    }
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

// CheckNameSpace --
//   Verify that the namespace of the given name is appropriate for the type
//   of value the name refers to.
//
// Inputs:
//   arena - arena to use for allocations.
//   name - the name in question
//   type - the type of the value refered to by the name.
//
// Results:
//   true if the namespace of the name is consistent with the type. false
//   otherwise.
//
// Side effects:
//   Prints a message to stderr if the namespace and type don't match.
static bool CheckNameSpace(FbleArena* arena, FbleName* name, FbleType* type)
{
  FbleKind* kind = FbleGetKind(arena, type);
  size_t kind_level = FbleGetKindLevel(kind);
  FbleKindRelease(arena, kind);

  bool match = (kind_level == 0 && name->space == FBLE_NORMAL_NAME_SPACE)
            || (kind_level == 1 && name->space == FBLE_TYPE_NAME_SPACE);

  if (!match) {
    ReportError(arena, &name->loc,
        "the namespace of '%n' is not appropriate for something of type %t\n", name, type);
  }
  return match;
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
    FbleReturnInstr* return_instr = FbleAlloc(arena, FbleReturnInstr);
    return_instr->_base.tag = FBLE_RETURN_INSTR;
    return_instr->result = result->index;
    AppendInstr(arena, scope, &return_instr->_base);
  }
}

// CompileExpr --
//   Type check and compile the given expression. Returns the local variable
//   that will hold the result of the expression and generates instructions to
//   compute the value of that expression at runtime.
//
// Inputs:
//   heap - heap to use for type allocations.
//   blocks - the blocks stack.
//   exit - if true, generate instructions to exit the current scope.
//   scope - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type and local of the compiled expression.
//   The type is NULL if the expression is not well typed.
//
// Side effects:
//   * Updates the blocks stack with with compiled block information.
//   * Appends instructions to the scope for executing the given expression.
//     There is no gaurentee about what instructions have been appended to
//     the scope if the expression fails to compile.
//   * Prints warning messages to stderr.
//   * Prints a message to stderr if the expression fails to compile.
//   * The caller should call FbleTypeRelease and LocalRelease when the
//     returned results are no longer needed. Note that FreeScope calls
//     LocalRelease for all locals allocated to the scope, so that can also be
//     used to clean up the local, but not the type.
static Compiled CompileExpr(FbleTypeHeap* heap, Blocks* blocks, bool exit, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = heap->arena;
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      AddBlockTime(blocks, 1);

      FbleType* type = CompileType(heap, scope, expr);
      if (type == NULL) {
        return COMPILE_FAILED;
      }

      FbleTypeType* type_type = FbleNewType(heap, FbleTypeType, FBLE_TYPE_TYPE, expr->loc);
      type_type->type = type;
      FbleTypeAddRef(heap, &type_type->_base, type_type->type);
      FbleTypeRelease(heap, type);

      Local* local = NewLocal(arena, scope);
      FbleTypeInstr* instr = FbleAlloc(arena, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      instr->dest = local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, local);

      Compiled c = {
        .type = &type_type->_base,
        .local = local
      };
      return c;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* misc_apply_expr = (FbleMiscApplyExpr*)expr;

      Compiled misc = CompileExpr(heap, blocks, false, scope, misc_apply_expr->misc);
      bool error = (misc.type == NULL);

      size_t argc = misc_apply_expr->args.size;
      AddBlockTime(blocks, 1 + argc);
      Compiled args[argc];
      for (size_t i = 0; i < argc; ++i) {
        args[i] = CompileExpr(heap, blocks, false, scope, misc_apply_expr->args.xs[i]);
        error = error || (args[i].type == NULL);
      }

      if (error) {
        FbleTypeRelease(heap, misc.type);
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(heap, args[i].type);
        }
        return COMPILE_FAILED;
      }

      FbleType* normal = FbleNormalType(heap, misc.type);
      switch (normal->tag) {
        case FBLE_FUNC_TYPE: {
          // FUNC_APPLY
          for (size_t i = 0; i < argc; ++i) {
            if (normal->tag != FBLE_FUNC_TYPE) {
              ReportError(arena, &expr->loc, "too many arguments to function\n");
              FbleTypeRelease(heap, normal);
              FbleTypeRelease(heap, misc.type);
              for (size_t j = i; j < argc; ++j) {
                FbleTypeRelease(heap, args[j].type);
              }
              return COMPILE_FAILED;
            }

            FbleFuncType* func_type = (FbleFuncType*)normal;
            if (!FbleTypesEqual(heap, func_type->arg, args[i].type)) {
              ReportError(arena, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  func_type->arg, args[i].type);
              FbleTypeRelease(heap, normal);
              FbleTypeRelease(heap, misc.type);
              for (size_t j = i; j < argc; ++j) {
                FbleTypeRelease(heap, args[j].type);
              }
              return COMPILE_FAILED;
            }
            FbleTypeRelease(heap, args[i].type);

            Local* dest = NewLocal(arena, scope);
            FbleFuncApplyInstr* apply_instr = FbleAlloc(arena, FbleFuncApplyInstr);
            apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
            apply_instr->loc = misc_apply_expr->misc->loc;
            apply_instr->exit = exit && (i+1 == argc);
            apply_instr->func = misc.local->index;
            apply_instr->arg = args[i].local->index;
            apply_instr->dest = dest->index.index;
            AppendInstr(arena, scope, &apply_instr->_base);
            LocalRelease(arena, scope, misc.local);
            LocalRelease(arena, scope, args[i].local);

            FbleTypeRelease(heap, misc.type);
            misc.type = FbleTypeRetain(heap, func_type->rtype);
            misc.local = dest;

            normal = FbleNormalType(heap, func_type->rtype);
            FbleTypeRelease(heap, &func_type->_base);
          }
          FbleTypeRelease(heap, normal);
          return misc;
        }

        case FBLE_TYPE_TYPE: {
          // FBLE_STRUCT_VALUE_EXPR
          FbleTypeType* type_type = (FbleTypeType*)normal;
          FbleType* vtype = FbleTypeRetain(heap, type_type->type);
          FbleTypeRelease(heap, normal);

          FbleTypeRelease(heap, misc.type);
          LocalRelease(arena, scope, misc.local);

          FbleStructType* struct_type = (FbleStructType*)FbleNormalType(heap, vtype);
          if (struct_type->_base.tag != FBLE_STRUCT_TYPE) {
            ReportError(arena, &misc_apply_expr->misc->loc,
                "expected a struct type, but found %t\n",
                vtype);
            FbleTypeRelease(heap, &struct_type->_base);
            FbleTypeRelease(heap, vtype);
            for (size_t i = 0; i < argc; ++i) {
              FbleTypeRelease(heap, args[i].type);
            }
            return COMPILE_FAILED;
          }

          if (struct_type->fields.size != argc) {
            // TODO: Where should the error message go?
            ReportError(arena, &expr->loc,
                "expected %i args, but %i were provided\n",
                 struct_type->fields.size, argc);
            FbleTypeRelease(heap, &struct_type->_base);
            FbleTypeRelease(heap, vtype);
            for (size_t i = 0; i < argc; ++i) {
              FbleTypeRelease(heap, args[i].type);
            }
            return COMPILE_FAILED;
          }

          bool error = false;
          for (size_t i = 0; i < argc; ++i) {
            FbleTaggedType* field = struct_type->fields.xs + i;

            if (!FbleTypesEqual(heap, field->type, args[i].type)) {
              ReportError(arena, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  field->type, args[i].type);
              error = true;
            }
            FbleTypeRelease(heap, args[i].type);
          }

          FbleTypeRelease(heap, &struct_type->_base);

          if (error) {
            FbleTypeRelease(heap, vtype);
            return COMPILE_FAILED;
          }

          Compiled c;
          c.type = vtype;
          c.local = NewLocal(arena, scope);

          FbleStructValueInstr* struct_instr = FbleAlloc(arena, FbleStructValueInstr);
          struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
          struct_instr->dest = c.local->index.index;
          FbleVectorInit(arena, struct_instr->args);
          AppendInstr(arena, scope, &struct_instr->_base);
          CompileExit(arena, exit, scope, c.local);

          for (size_t i = 0; i < argc; ++i) {
            FbleVectorAppend(arena, struct_instr->args, args[i].local->index);
            LocalRelease(arena, scope, args[i].local);
          }

          return c;
        }

        default: {
          ReportError(arena, &expr->loc,
              "expecting a function or struct type, but found something of type %t\n",
              misc.type);
          FbleTypeRelease(heap, misc.type);
          FbleTypeRelease(heap, normal);
          for (size_t i = 0; i < argc; ++i) {
            FbleTypeRelease(heap, args[i].type);
          }
          return COMPILE_FAILED;
        }
      }

      UNREACHABLE("Should never get here");
      return COMPILE_FAILED;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      AddBlockTime(blocks, 1);

      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      FbleStructType* struct_type = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, expr->loc);
      FbleVectorInit(arena, struct_type->fields);

      size_t argc = struct_expr->args.size;
      Compiled args[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        EnterBlock(arena, blocks, arg->name, arg->expr->loc, scope);
        args[j] = CompileExpr(heap, blocks, false, scope, arg->expr);
        ExitBlock(arena, blocks, scope);
        error = error || (args[j].type == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (args[i].type != NULL) {
          if (!CheckNameSpace(arena, &arg->name, args[i].type)) {
            error = true;
          }

          FbleTaggedType* cfield = FbleVectorExtend(arena, struct_type->fields);
          cfield->name = arg->name;
          cfield->type = args[i].type;
          FbleTypeAddRef(heap, &struct_type->_base, cfield->type);
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&arg->name, &struct_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena, &arg->name.loc,
                "duplicate field name '%n'\n",
                &struct_expr->args.xs[j].name);
          }
        }

        FbleTypeRelease(heap, args[i].type);
      }

      if (error) {
        FbleTypeRelease(heap, &struct_type->_base);
        return COMPILE_FAILED;
      }

      Compiled c;
      c.type = &struct_type->_base;
      c.local = NewLocal(arena, scope);
      FbleStructValueInstr* struct_instr = FbleAlloc(arena, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->dest = c.local->index.index;
      AppendInstr(arena, scope, &struct_instr->_base);
      CompileExit(arena, exit, scope, c.local);

      FbleVectorInit(arena, struct_instr->args);
      for (size_t i = 0; i < argc; ++i) {
        FbleVectorAppend(arena, struct_instr->args, args[i].local->index);
        LocalRelease(arena, scope, args[i].local);
      }
      return c;
    }

    case FBLE_UNION_VALUE_EXPR: {
      AddBlockTime(blocks, 1);
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      FbleType* type = CompileType(heap, scope, union_value_expr->type);
      if (type == NULL) {
        return COMPILE_FAILED;
      }

      FbleUnionType* union_type = (FbleUnionType*)FbleNormalType(heap, type);
      if (union_type->_base.tag != FBLE_UNION_TYPE) {
        ReportError(arena, &union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
        FbleTypeRelease(heap, &union_type->_base);
        FbleTypeRelease(heap, type);
        return COMPILE_FAILED;
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
        ReportError(arena, &union_value_expr->field.loc,
            "'%n' is not a field of type %t\n",
            &union_value_expr->field, type);
        FbleTypeRelease(heap, &union_type->_base);
        FbleTypeRelease(heap, type);
        return COMPILE_FAILED;
      }

      Compiled arg = CompileExpr(heap, blocks, false, scope, union_value_expr->arg);
      if (arg.type == NULL) {
        FbleTypeRelease(heap, &union_type->_base);
        FbleTypeRelease(heap, type);
        return COMPILE_FAILED;
      }

      if (!FbleTypesEqual(heap, field_type, arg.type)) {
        ReportError(arena, &union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg.type);
        FbleTypeRelease(heap, type);
        FbleTypeRelease(heap, &union_type->_base);
        FbleTypeRelease(heap, arg.type);
        return COMPILE_FAILED;
      }
      FbleTypeRelease(heap, arg.type);
      FbleTypeRelease(heap, &union_type->_base);

      Compiled c;
      c.type = type;
      c.local = NewLocal(arena, scope);
      FbleUnionValueInstr* union_instr = FbleAlloc(arena, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->tag = tag;
      union_instr->arg = arg.local->index;
      union_instr->dest = c.local->index.index;
      AppendInstr(arena, scope, &union_instr->_base);
      LocalRelease(arena, scope, arg.local);
      CompileExit(arena, exit, scope, c.local);
      return c;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      AddBlockTime(blocks, 1);

      FbleMiscAccessExpr* access_expr = (FbleMiscAccessExpr*)expr;

      Compiled obj = CompileExpr(heap, blocks, false, scope, access_expr->object);
      if (obj.type == NULL) {
        return COMPILE_FAILED;
      }

      FbleAccessInstr* access = FbleAlloc(arena, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->loc = access_expr->field.loc;
      access->obj = obj.local->index;
      AppendInstr(arena, scope, &access->_base);

      FbleType* normal = FbleNormalType(heap, obj.type);
      FbleTaggedTypeV* fields = NULL;
      if (normal->tag == FBLE_STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
        fields = &((FbleStructType*)normal)->fields;
      } else if (normal->tag == FBLE_UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
        fields = &((FbleUnionType*)normal)->fields;
      } else {
        ReportError(arena, &access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            obj.type);

        FbleTypeRelease(heap, obj.type);
        FbleTypeRelease(heap, normal);
        return COMPILE_FAILED;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(&access_expr->field, &fields->xs[i].name)) {
          access->tag = i;
          FbleType* rtype = FbleTypeRetain(heap, fields->xs[i].type);
          FbleTypeRelease(heap, normal);

          Compiled c;
          c.type = rtype;
          c.local = NewLocal(arena, scope);
          access->dest = c.local->index.index;
          CompileExit(arena, exit, scope, c.local);
          FbleTypeRelease(heap, obj.type);
          LocalRelease(arena, scope, obj.local);
          return c;
        }
      }

      ReportError(arena, &access_expr->field.loc,
          "'%n' is not a field of type %t\n",
          &access_expr->field, obj.type);
      FbleTypeRelease(heap, obj.type);
      FbleTypeRelease(heap, normal);
      return COMPILE_FAILED;
    }

    case FBLE_UNION_SELECT_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      AddBlockTime(blocks, 1);

      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Compiled condition = CompileExpr(heap, blocks, false, scope, select_expr->condition);
      if (condition.type == NULL) {
        return COMPILE_FAILED;
      }

      FbleUnionType* union_type = (FbleUnionType*)FbleNormalType(heap, condition.type);
      if (union_type->_base.tag != FBLE_UNION_TYPE) {
        ReportError(arena, &select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            condition.type);
        FbleTypeRelease(heap, &union_type->_base);
        FbleTypeRelease(heap, condition.type);
        return COMPILE_FAILED;
      }
      FbleTypeRelease(heap, condition.type);

      if (exit) {
        FbleProfileAutoExitBlockInstr* exit_instr = FbleAlloc(arena, FbleProfileAutoExitBlockInstr);
        exit_instr->_base.tag = FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR;
        AppendInstr(arena, scope, &exit_instr->_base);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(arena, FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->loc = select_expr->condition->loc;
      select_instr->condition = condition.local->index;
      AppendInstr(arena, scope, &select_instr->_base);

      Compiled target = COMPILE_FAILED;
      FbleGotoInstr* enter_gotos[union_type->fields.size];
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        enter_gotos[i] = FbleAlloc(arena, FbleGotoInstr);
        enter_gotos[i]->_base.tag = FBLE_GOTO_INSTR;
        AppendInstr(arena, scope, &enter_gotos[i]->_base);
      }

      size_t default_pc = scope->code->instrs.size;
      FbleGotoInstr* exit_goto_default = NULL;
      if (select_expr->default_ != NULL) {
        FbleName name = {
          .name = ":",
          .loc = select_expr->default_->loc,  // unused.
          .space = FBLE_NORMAL_NAME_SPACE
        };
        EnterBlock(arena, blocks, name, select_expr->default_->loc, scope);
        Compiled result = CompileExpr(heap, blocks, exit, scope, select_expr->default_);

        if (result.type == NULL) {
          ExitBlock(arena, blocks, NULL);
          FbleTypeRelease(heap, &union_type->_base);
          return COMPILE_FAILED;
        }

        target.type = result.type;
        target.local = NewLocal(arena, scope);

        if (!exit) {
          FbleCopyInstr* copy = FbleAlloc(arena, FbleCopyInstr);
          copy->_base.tag = FBLE_COPY_INSTR;
          copy->source = result.local->index;
          copy->dest = target.local->index.index;
          AppendInstr(arena, scope, &copy->_base);
          LocalRelease(arena, scope, result.local);
        }
        ExitBlock(arena, blocks, exit ? NULL : scope);

        if (!exit) {
          exit_goto_default = FbleAlloc(arena, FbleGotoInstr);
          exit_goto_default->_base.tag = FBLE_GOTO_INSTR;
          AppendInstr(arena, scope, &exit_goto_default->_base);
        }
      }

      FbleGotoInstr* exit_gotos[select_expr->choices.size];
      size_t choice = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (choice < select_expr->choices.size && FbleNamesEqual(&select_expr->choices.xs[choice].name, &union_type->fields.xs[i].name)) {
          enter_gotos[i]->pc = scope->code->instrs.size;

          EnterBlock(arena, blocks,
              select_expr->choices.xs[choice].name,
              select_expr->choices.xs[choice].expr->loc,
              scope);
          AddBlockTime(blocks, 1);
          Compiled result = CompileExpr(heap, blocks, exit, scope, select_expr->choices.xs[choice].expr);

          if (result.type == NULL) {
            ExitBlock(arena, blocks, NULL);
            FbleTypeRelease(heap, &union_type->_base);
            FbleTypeRelease(heap, target.type);
            return COMPILE_FAILED;
          }

          if (target.type == NULL) {
            target.type = result.type;
            target.local = NewLocal(arena, scope);
          } else {
            if (!FbleTypesEqual(heap, target.type, result.type)) {
              ReportError(arena, &select_expr->choices.xs[choice].expr->loc,
                  "expected type %t, but found %t\n",
                  target.type, result.type);

              FbleTypeRelease(heap, result.type);
              FbleTypeRelease(heap, target.type);
              FbleTypeRelease(heap, &union_type->_base);
              ExitBlock(arena, blocks, NULL);
              return COMPILE_FAILED;
            }
            FbleTypeRelease(heap, result.type);
          }

          if (!exit) {
            FbleCopyInstr* copy = FbleAlloc(arena, FbleCopyInstr);
            copy->_base.tag = FBLE_COPY_INSTR;
            copy->source = result.local->index;
            copy->dest = target.local->index.index;
            AppendInstr(arena, scope, &copy->_base);
          }

          ExitBlock(arena, blocks, exit ? NULL : scope);
          LocalRelease(arena, scope, result.local);

          if (!exit) {
            exit_gotos[choice] = FbleAlloc(arena, FbleGotoInstr);
            exit_gotos[choice]->_base.tag = FBLE_GOTO_INSTR;
            AppendInstr(arena, scope, &exit_gotos[choice]->_base);
          }

          choice++;
        } else if (select_expr->default_ == NULL) {
          if (choice < select_expr->choices.size) {
            ReportError(arena, &select_expr->choices.xs[choice].name.loc,
                "expected tag '%n', but found '%n'\n",
                &union_type->fields.xs[i].name, &select_expr->choices.xs[choice].name);
          } else {
            ReportError(arena, &expr->loc,
                "missing tag '%n'\n",
                &union_type->fields.xs[i].name);
          }
          FbleTypeRelease(heap, &union_type->_base);
          FbleTypeRelease(heap, target.type);
          return COMPILE_FAILED;
        } else {
          enter_gotos[i]->pc = default_pc;
        }
      }
      FbleTypeRelease(heap, &union_type->_base);

      if (choice < select_expr->choices.size) {
        ReportError(arena, &select_expr->choices.xs[choice].name.loc,
            "unexpected tag '%n'\n",
            &select_expr->choices.xs[choice]);
        FbleTypeRelease(heap, target.type);
        return COMPILE_FAILED;
      }

      if (!exit) {
        if (exit_goto_default != NULL) {
          exit_goto_default->pc = scope->code->instrs.size;
        }
        for (size_t i = 0; i < select_expr->choices.size; ++i) {
          exit_gotos[i]->pc = scope->code->instrs.size;
        }
      }

      // TODO: We ought to release the condition right after doing goto,
      // otherwise we'll end up unnecessarily holding on to it for the full
      // duration of the block. Technically this doesn't appear to be a
      // violation of the language spec, because it only effects constants in
      // runtime. But we probably ought to fix it anyway.
      LocalRelease(arena, scope, condition.local);
      return target;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      FbleType* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        arg_types[i] = CompileType(heap, scope, func_value_expr->args.xs[i].type);
        error = error || arg_types[i] == NULL;

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&func_value_expr->args.xs[i].name, &func_value_expr->args.xs[j].name)) {
            error = true;
            ReportError(arena, &func_value_expr->args.xs[i].name.loc,
                "duplicate arg name '%n'\n",
                &func_value_expr->args.xs[i].name);
          }
        }
      }

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(heap, arg_types[i]);
        }
        return COMPILE_FAILED;
      }

      FbleFuncValueInstr* instr = FbleAlloc(arena, FbleFuncValueInstr);
      instr->_base.tag = FBLE_FUNC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena);
      instr->argc = argc;

      Scope func_scope;
      InitScope(arena, &func_scope, instr->code, &instr->scope, scope);
      EnterBodyBlock(arena, blocks, func_value_expr->body->loc, &func_scope);

      for (size_t i = 0; i < argc; ++i) {
        Local* local = NewLocal(arena, &func_scope);
        PushVar(arena, &func_scope, func_value_expr->args.xs[i].name, arg_types[i], local);
      }

      Compiled func_result = CompileExpr(heap, blocks, true, &func_scope, func_value_expr->body);
      ExitBlock(arena, blocks, NULL);
      if (func_result.type == NULL) {
        FreeScope(heap, &func_scope);
        FbleFreeInstr(arena, &instr->_base);
        return COMPILE_FAILED;
      }
      FbleType* type = func_result.type;
      LocalRelease(arena, &func_scope, func_result.local);

      for (size_t i = 0; i < argc; ++i) {
        FbleType* arg_type = arg_types[argc - 1 - i];
        FbleFuncType* ft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
        ft->arg = arg_type;
        ft->rtype = type;

        FbleTypeAddRef(heap, &ft->_base, ft->arg);
        FbleTypeAddRef(heap, &ft->_base, ft->rtype);
        FbleTypeRelease(heap, ft->rtype);
        type = &ft->_base;
      }

      FreeScope(heap, &func_scope);

      // TODO: Is it right for time to be proportional to number of captured
      // variables?
      AddBlockTime(blocks, instr->code->statics);

      Compiled c;
      c.type = type;
      c.local = NewLocal(arena, scope);
      instr->dest = c.local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, c.local);
      return c;
    }

    case FBLE_EVAL_EXPR: {
      AddBlockTime(blocks, 1);
      FbleEvalExpr* eval_expr = (FbleEvalExpr*)expr;

      FbleProcValueInstr* instr = FbleAlloc(arena, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena);

      Scope eval_scope;
      InitScope(arena, &eval_scope, instr->code, &instr->scope, scope);
      EnterBodyBlock(arena, blocks, expr->loc, &eval_scope);

      Compiled body = CompileExpr(heap, blocks, true, &eval_scope, eval_expr->body);
      ExitBlock(arena, blocks, NULL);
      CompileExit(arena, true, &eval_scope, body.local);

      if (body.type == NULL) {
        FreeScope(heap, &eval_scope);
        FbleFreeInstr(arena, &instr->_base);
        return COMPILE_FAILED;
      }

      FbleProcType* proc_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      proc_type->type = body.type;
      FbleTypeAddRef(heap, &proc_type->_base, proc_type->type);
      FbleTypeRelease(heap, body.type);

      Compiled c;
      c.type = &proc_type->_base;
      c.local = NewLocal(arena, scope);
      instr->dest = c.local->index.index;

      FreeScope(heap, &eval_scope);
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, c.local);
      return c;
    }

    case FBLE_LINK_EXPR: {
      AddBlockTime(blocks, 1);
      FbleLinkExpr* link_expr = (FbleLinkExpr*)expr;
      if (FbleNamesEqual(&link_expr->get, &link_expr->put)) {
        ReportError(arena, &link_expr->put.loc,
            "duplicate port name '%n'\n",
            &link_expr->put);
        return COMPILE_FAILED;
      }

      FbleType* port_type = CompileType(heap, scope, link_expr->type);
      if (port_type == NULL) {
        return COMPILE_FAILED;
      }

      FbleProcType* get_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, port_type->loc);
      get_type->type = port_type;
      FbleTypeAddRef(heap, &get_type->_base, get_type->type);

      FbleStructType* unit_type = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, expr->loc);
      FbleVectorInit(arena, unit_type->fields);

      FbleProcType* unit_proc_type = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, expr->loc);
      unit_proc_type->type = &unit_type->_base;
      FbleTypeAddRef(heap, &unit_proc_type->_base, unit_proc_type->type);
      FbleTypeRelease(heap, &unit_type->_base);

      FbleFuncType* put_type = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, expr->loc);
      put_type->arg = port_type;
      FbleTypeAddRef(heap, &put_type->_base, put_type->arg);
      FbleTypeRelease(heap, port_type);
      put_type->rtype = &unit_proc_type->_base;
      FbleTypeAddRef(heap, &put_type->_base, put_type->rtype);
      FbleTypeRelease(heap, &unit_proc_type->_base);

      FbleProcValueInstr* instr = FbleAlloc(arena, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena);

      Scope body_scope;
      InitScope(arena, &body_scope, instr->code, &instr->scope, scope);
      EnterBodyBlock(arena, blocks, link_expr->body->loc, &body_scope);

      FbleLinkInstr* link = FbleAlloc(arena, FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;

      Local* get_local = NewLocal(arena, &body_scope);
      link->get = get_local->index.index;
      PushVar(arena, &body_scope, link_expr->get, &get_type->_base, get_local);

      Local* put_local = NewLocal(arena, &body_scope);
      link->put = put_local->index.index;
      PushVar(arena, &body_scope, link_expr->put, &put_type->_base, put_local);

      AppendInstr(arena, &body_scope, &link->_base);

      Compiled body = CompileExpr(heap, blocks, false, &body_scope, link_expr->body);
      FbleType* type = NULL;
      if (body.type != NULL) {
        type = body.type;
      }

      FbleProcInstr* proc = FbleAlloc(arena, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      AppendInstr(arena, &body_scope, &proc->_base);

      if (body.type != NULL) {
        proc->proc = body.local->index;
        LocalRelease(arena, &body_scope, body.local);
      }

      ExitBlock(arena, blocks, NULL);
      FreeScope(heap, &body_scope);

      if (type == NULL) {
        FbleFreeInstr(arena, &instr->_base);
        return COMPILE_FAILED;
      }

      Compiled c;
      c.type = type;
      c.local = NewLocal(arena, scope);
      instr->dest = c.local->index.index;
      AppendInstr(arena, scope, &instr->_base);

      FbleType* proc_type = FbleNormalType(heap, type);
      if (proc_type->tag != FBLE_PROC_TYPE) {
        ReportError(arena, &link_expr->body->loc,
            "expected a value of type proc, but found %t\n",
            type);
        FbleTypeRelease(heap, proc_type);
        FbleTypeRelease(heap, c.type);
        return COMPILE_FAILED;
      }
      FbleTypeRelease(heap, proc_type);
      CompileExit(arena, exit, scope, c.local);
      return c;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* exec_expr = (FbleExecExpr*)expr;
      bool error = false;

      AddBlockTime(blocks, 1 + exec_expr->bindings.size);

      // Evaluate the types of the bindings and set up the new vars.
      FbleType* types[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        types[i] = CompileType(heap, scope, exec_expr->bindings.xs[i].type);
        error = error || (types[i] == NULL);
      }

      FbleProcValueInstr* instr = FbleAlloc(arena, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->code = NewInstrBlock(arena);

      Scope body_scope;
      InitScope(arena, &body_scope, instr->code, &instr->scope, scope);
      EnterBodyBlock(arena, blocks, exec_expr->body->loc, &body_scope);

      Local* args[exec_expr->bindings.size];
      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        FbleProcValueInstr* binstr = FbleAlloc(arena, FbleProcValueInstr);
        binstr->_base.tag = FBLE_PROC_VALUE_INSTR;
        binstr->code = NewInstrBlock(arena);

        Scope binding_scope;
        InitScope(arena, &binding_scope, binstr->code, &binstr->scope, &body_scope);
        EnterBodyBlock(arena, blocks, exec_expr->bindings.xs[i].expr->loc, &binding_scope);

        Compiled binding =  CompileExpr(heap, blocks, false, &binding_scope, exec_expr->bindings.xs[i].expr);
        if (binding.type == NULL) {
          error = true;
        }

        FbleProcInstr* bproc = FbleAlloc(arena, FbleProcInstr);
        bproc->_base.tag = FBLE_PROC_INSTR;

        AppendInstr(arena, &binding_scope, &bproc->_base);
        if (binding.type != NULL) {
          bproc->proc = binding.local->index;
          LocalRelease(arena, &binding_scope, binding.local);
        }

        ExitBlock(arena, blocks, NULL);
        FreeScope(heap, &binding_scope);

        args[i] = NewLocal(arena, &body_scope);
        binstr->dest = args[i]->index.index;
        AppendInstr(arena, &body_scope, &binstr->_base);

        if (binding.type != NULL) {
          FbleProcType* proc_type = (FbleProcType*)FbleNormalType(heap, binding.type);
          if (proc_type->_base.tag == FBLE_PROC_TYPE) {
            if (types[i] != NULL && !FbleTypesEqual(heap, types[i], proc_type->type)) {
              error = true;
              ReportError(arena, &exec_expr->bindings.xs[i].expr->loc,
                  "expected type %t!, but found %t\n",
                  types[i], binding.type);
            }
          } else {
            error = true;
            ReportError(arena, &exec_expr->bindings.xs[i].expr->loc,
                "expected process, but found expression of type %t\n",
                binding.type);
          }
          FbleTypeRelease(heap, &proc_type->_base);
        }
        FbleTypeRelease(heap, binding.type);
      }

      FbleForkInstr* fork = FbleAlloc(arena, FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      FbleVectorInit(arena, fork->args);
      fork->dests.xs = FbleArrayAlloc(arena, FbleLocalIndex, exec_expr->bindings.size);
      fork->dests.size = exec_expr->bindings.size;
      AppendInstr(arena, &body_scope, &fork->_base);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        // Note: Make sure we call NewLocal before calling LocalRelease on any
        // of the arguments.
        Local* local = NewLocal(arena, &body_scope);
        fork->dests.xs[i] = local->index.index;
        PushVar(arena, &body_scope, exec_expr->bindings.xs[i].name, types[i], local);
      }

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        // TODO: Does this hold on to the bindings longer than we want to?
        FbleVectorAppend(arena, fork->args, args[i]->index);
        LocalRelease(arena, &body_scope, args[i]);
      }

      FbleJoinInstr* join = FbleAlloc(arena, FbleJoinInstr);
      join->_base.tag = FBLE_JOIN_INSTR;
      AppendInstr(arena, &body_scope, &join->_base);

      Compiled body = COMPILE_FAILED;
      if (!error) {
        body = CompileExpr(heap, blocks, false, &body_scope, exec_expr->body);
        error = (body.type == NULL);
      }

      if (body.type != NULL) {
        FbleType* normal = FbleNormalType(heap, body.type);
        if (normal->tag != FBLE_PROC_TYPE) {
          error = true;
          ReportError(arena, &exec_expr->body->loc,
              "expected a value of type proc, but found %t\n",
              body.type);
        }
        FbleTypeRelease(heap, normal);
      }

      FbleProcInstr* proc = FbleAlloc(arena, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      AppendInstr(arena, &body_scope, &proc->_base);
      if (body.type != NULL) {
        proc->proc = body.local->index;
        LocalRelease(arena, &body_scope, body.local);
      }
      ExitBlock(arena, blocks, NULL);

      body.local = NewLocal(arena, scope);
      FreeScope(heap, &body_scope);

      instr->dest = body.local->index.index;
      AppendInstr(arena, scope, &instr->_base);
      CompileExit(arena, exit, scope, body.local);
      if (error) {
        FbleTypeRelease(heap, body.type);
        return COMPILE_FAILED;
      }
      return body;
    }

    case FBLE_VAR_EXPR: {
      AddBlockTime(blocks, 1);
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      Var* var = GetVar(heap, scope, var_expr->var, false);
      if (var == NULL) {
        ReportError(arena, &var_expr->var.loc, "variable '%n' not defined\n",
            &var_expr->var);
        return COMPILE_FAILED;
      }

      Compiled c;
      c.type = FbleTypeRetain(heap, var->type);
      c.local = var->local;
      c.local->refcount++;
      CompileExit(arena, exit, scope, c.local);
      return c;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;
      AddBlockTime(blocks, 1 + let_expr->bindings.size);

      // Evaluate the types of the bindings and set up the new vars.
      FbleType* types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        if (binding->type == NULL) {
          assert(binding->kind != NULL);

          // We don't know the type, so create an abstract type variable to
          // represent the type.
          // TODO: It would be nice to pick a more descriptive type for kind
          // level 0 variables. Perhaps: __name@?
          FbleName type_name = binding->name;
          type_name.space = FBLE_TYPE_NAME_SPACE;
          types[i] = FbleNewVarType(heap, binding->name.loc, binding->kind, type_name);
        } else {
          assert(binding->kind == NULL);
          types[i] = CompileType(heap, scope, binding->type);
          error = error || (types[i] == NULL);
        }
        
        if (types[i] != NULL && !CheckNameSpace(arena, &binding->name, types[i])) {
          error = true;
        }

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&let_expr->bindings.xs[i].name, &let_expr->bindings.xs[j].name)) {
            ReportError(arena, &let_expr->bindings.xs[i].name.loc,
                "duplicate variable name '%n'\n",
                &let_expr->bindings.xs[i].name);
            error = true;
          }
        }
      }

      Var* vars[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Local* local = NewLocal(arena, scope);
        vars[i] = PushVar(arena, scope, let_expr->bindings.xs[i].name, types[i], local);
        FbleRefValueInstr* ref_instr = FbleAlloc(arena, FbleRefValueInstr);
        ref_instr->_base.tag = FBLE_REF_VALUE_INSTR;
        ref_instr->dest = local->index.index;
        AppendInstr(arena, scope, &ref_instr->_base);
      }

      // Compile the values of the variables.
      Compiled defs[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;

        defs[i] = COMPILE_FAILED;
        if (!error) {
          EnterBlock(arena, blocks, binding->name, binding->expr->loc, scope);
          defs[i] = CompileExpr(heap, blocks, false, scope, binding->expr);
          ExitBlock(arena, blocks, scope);
        }
        error = error || (defs[i].type == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(heap, types[i], defs[i].type)) {
          error = true;
          ReportError(arena, &binding->expr->loc,
              "expected type %t, but found something of type %t\n",
              types[i], defs[i].type);
        } else if (!error && binding->type == NULL) {
          FbleKind* expected_kind = FbleGetKind(arena, types[i]);
          FbleKind* actual_kind = FbleGetKind(arena, defs[i].type);
          if (!FbleKindsEqual(expected_kind, actual_kind)) {
            ReportError(arena, &binding->expr->loc,
                "expected kind %k, but found something of kind %k\n",
                expected_kind, actual_kind);
            error = true;
          }
          FbleKindRelease(arena, expected_kind);
          FbleKindRelease(arena, actual_kind);
        }
      }

      // Check to see if this is a recursive let block.
      bool recursive = false;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        recursive = recursive || vars[i]->used;
      }

      // Apply the newly computed type values for variables whose types were
      // previously unknown.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (!error && let_expr->bindings.xs[i].type == NULL) {
          FbleAssignVarType(heap, types[i], defs[i].type);
        }
        FbleTypeRelease(heap, defs[i].type);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (defs[i].type != NULL) {
          if (let_expr->bindings.xs[i].type == NULL && FbleTypeIsVacuous(heap, types[i])) {
            ReportError(arena, &let_expr->bindings.xs[i].name.loc,
                "%n is vacuous\n", &let_expr->bindings.xs[i].name);
            error = true;
          }

          if (vars[i]->local == defs[i].local) {
            ReportError(arena, &let_expr->bindings.xs[i].name.loc,
                "%n is vacuous\n", &let_expr->bindings.xs[i].name);
            error = true;
          }

          if (recursive) {
            FbleRefDefInstr* ref_def_instr = FbleAlloc(arena, FbleRefDefInstr);
            ref_def_instr->_base.tag = FBLE_REF_DEF_INSTR;
            ref_def_instr->ref = vars[i]->local->index.index;
            ref_def_instr->value = defs[i].local->index;
            AppendInstr(arena, scope, &ref_def_instr->_base);
          }
          LocalRelease(arena, scope, vars[i]->local);
          vars[i]->local = defs[i].local;
        }
      }

      Compiled body = COMPILE_FAILED;
      if (!error) {
        body = CompileExpr(heap, blocks, exit, scope, let_expr->body);
      }

      if (body.type != NULL) {
        for (size_t i = 0; i < let_expr->bindings.size; ++i) {
          if (!vars[i]->accessed && vars[i]->name.name[0] != '_') {
            FbleReportWarning("variable '", &vars[i]->name.loc);
            FblePrintName(stderr, &vars[i]->name);
            fprintf(stderr, "' defined but not used\n");
          }
        }
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(heap, scope);
      }

      return body;
    }

    case FBLE_MODULE_REF_EXPR: {
      AddBlockTime(blocks, 1);
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      Var* var = GetVar(heap, scope, module_ref_expr->ref.resolved, false);

      // We should have resolved all modules at program load time.
      assert(var != NULL && "module not in scope");

      Compiled c;
      c.type = FbleTypeRetain(heap, var->type);
      c.local = var->local;
      c.local->refcount++;
      CompileExit(arena, exit, scope, c.local);
      return c;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      if (FbleGetKindLevel(poly->arg.kind) != 1) {
        ReportError(arena, &poly->arg.kind->loc,
            "expected a type kind, but found %k\n",
            poly->arg.kind);
        return COMPILE_FAILED;
      }

      if (poly->arg.name.space != FBLE_TYPE_NAME_SPACE) {
        ReportError(arena, &poly->arg.name.loc,
            "the namespace of '%n' is not appropriate for kind %k\n",
            &poly->arg.name, poly->arg.kind);
        return COMPILE_FAILED;
      }

      FbleType* arg_type = FbleNewVarType(heap, poly->arg.name.loc, poly->arg.kind, poly->arg.name);
      FbleType* arg = FbleValueOfType(heap, arg_type);
      assert(arg != NULL);

      // TODO: It's a little silly that we are pushing an empty type value
      // here. Oh well. Maybe in the future we'll optimize those away or
      // add support for non-type poly args too.
      AddBlockTime(blocks, 1);

      Local* local = NewLocal(arena, scope);
      FbleTypeInstr* type_instr = FbleAlloc(arena, FbleTypeInstr);
      type_instr->_base.tag = FBLE_TYPE_INSTR;
      type_instr->dest = local->index.index;
      AppendInstr(arena, scope, &type_instr->_base);

      PushVar(arena, scope, poly->arg.name, arg_type, local);
      Compiled body = CompileExpr(heap, blocks, exit, scope, poly->body);
      PopVar(heap, scope);

      if (body.type == NULL) {
        FbleTypeRelease(heap, arg);
        return COMPILE_FAILED;
      }

      FbleType* pt = FbleNewPolyType(heap, expr->loc, arg, body.type);
      FbleTypeRelease(heap, arg);
      FbleTypeRelease(heap, body.type);
      body.type = pt;
      return body;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // CompileExpr gives us typeof(poly)
      Compiled poly = CompileExpr(heap, blocks, exit, scope, apply->poly);
      if (poly.type == NULL) {
        return COMPILE_FAILED;
      }

      FblePolyKind* poly_kind = (FblePolyKind*)FbleGetKind(arena, poly.type);
      if (poly_kind->_base.tag != FBLE_POLY_KIND) {
        ReportError(arena, &expr->loc,
            "cannot apply poly args to a basic kinded entity\n");
        FbleKindRelease(arena, &poly_kind->_base);
        FbleTypeRelease(heap, poly.type);
        return COMPILE_FAILED;
      }

      // Note: arg_type is typeof(arg)
      FbleType* arg_type = CompileExprNoInstrs(heap, scope, apply->arg);
      if (arg_type == NULL) {
        FbleKindRelease(arena, &poly_kind->_base);
        FbleTypeRelease(heap, poly.type);
        return COMPILE_FAILED;
      }

      FbleKind* expected_kind = poly_kind->arg;
      FbleKind* actual_kind = FbleGetKind(arena, arg_type);
      if (!FbleKindsEqual(expected_kind, actual_kind)) {
        ReportError(arena, &apply->arg->loc,
            "expected kind %k, but found something of kind %k\n",
            expected_kind, actual_kind);
        FbleKindRelease(arena, &poly_kind->_base);
        FbleKindRelease(arena, actual_kind);
        FbleTypeRelease(heap, arg_type);
        FbleTypeRelease(heap, poly.type);
        return COMPILE_FAILED;
      }
      FbleKindRelease(arena, actual_kind);
      FbleKindRelease(arena, &poly_kind->_base);

      FbleType* arg = FbleValueOfType(heap, arg_type);
      assert(arg != NULL && "TODO: poly apply arg is a value?");
      FbleTypeRelease(heap, arg_type);

      FbleType* pat = FbleNewPolyApplyType(heap, expr->loc, poly.type, arg);
      FbleTypeRelease(heap, arg);
      FbleTypeRelease(heap, poly.type);
      poly.type = pat;
      return poly;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      return CompileList(heap, blocks, exit, scope, expr->loc, list_expr->args);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      Compiled spec = CompileExpr(heap, blocks, false, scope, literal->spec);
      if (spec.type == NULL) {
        return COMPILE_FAILED;
      }

      FbleStructType* normal = (FbleStructType*)FbleNormalType(heap, spec.type);
      if (normal->_base.tag != FBLE_STRUCT_TYPE) {
        ReportError(arena, &literal->spec->loc,
            "expected a struct value, but literal spec has type %t\n",
            spec.type);
        FbleTypeRelease(heap, spec.type);
        FbleTypeRelease(heap, &normal->_base);
        return COMPILE_FAILED;
      }
      FbleTypeRelease(heap, &normal->_base);

      size_t n = strlen(literal->word);
      if (n == 0) {
        ReportError(arena, &literal->word_loc,
            "literals must not be empty\n");
        FbleTypeRelease(heap, spec.type);
        return COMPILE_FAILED;
      }

      FbleName spec_name = {
        .name = "__literal_spec",
        .space = FBLE_NORMAL_NAME_SPACE,
        .loc = literal->spec->loc,
      };
      PushVar(arena, scope, spec_name, spec.type, spec.local);

      FbleVarExpr spec_var = {
        ._base = { .tag = FBLE_VAR_EXPR, .loc = literal->spec->loc },
        .var = spec_name
      };

      FbleMiscAccessExpr letters[n];
      FbleExpr* xs[n];
      char word_letters[2*n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        word_letters[2*i] = literal->word[i];
        word_letters[2*i + 1] = '\0';

        letters[i]._base.tag = FBLE_MISC_ACCESS_EXPR;
        letters[i]._base.loc = loc;
        letters[i].object = &spec_var._base;
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
      Compiled result = CompileList(heap, blocks, exit, scope, literal->word_loc, args);
      PopVar(heap, scope);
      return result;
    }
  }

  UNREACHABLE("should already have returned");
  return COMPILE_FAILED;
}

// CompileList --
//   Type check and compile a list expression. Returns the type of the
//   expression and generates instructions to compute the value of that
//   expression at runtime.
//
// Inputs:
//   heap - heap to use for allocations.
//   blocks - the blocks stack.
//   exit - if true, generate instructions to exit the current scope.
//   scope - the list of variables in scope.
//   loc - the location of the list expression.
//   args - the elements of the list expression to compile.
//
// Results:
//   The type and local of the compiled expression.
//   The type is NULL if the expression is not well typed.
//
// Side effects:
//   * Updates blocks with compiled block information.
//   * Appends instructions to scope for executing the given expression.
//     There is no gaurentee about what instructions have been appended to
//     scope if the expression fails to compile.
//   * Prints a message to stderr if the expression fails to compile.
//   * Allocates a reference-counted type that must be freed using
//     FbleTypeRelease when it is no longer needed.
//   * Behavior is undefined if there is not at least one list argument.
static Compiled CompileList(FbleTypeHeap* heap, Blocks* blocks, bool exit, Scope* scope, FbleLoc loc, FbleExprV args)
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

  FbleArena* arena = heap->arena;
  FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
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
    char* name = FbleArrayAlloc(arena, char, num_digits + 2);
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

  Compiled result = CompileExpr(heap, blocks, exit, scope, expr);

  FbleKindRelease(arena, &basic_kind->_base);
  for (size_t i = 0; i < args.size; i++) {
    FbleFree(arena, (void*)arg_names[i].name);
  }
  return result;
}

// CompileExprNoInstrs --
//   Type check and compile the given expression. Returns the type of the
//   expression.
//
// Inputs:
//   heap - heap to use for allocations.
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
static FbleType* CompileExprNoInstrs(FbleTypeHeap* heap, Scope* scope, FbleExpr* expr)
{
  FbleArena* arena = heap->arena;
  FbleInstrBlock* code = NewInstrBlock(arena);

  Scope nscope;
  InitScope(arena, &nscope, code, NULL, scope);

  Blocks blocks;
  FbleVectorInit(arena, blocks.stack);
  FbleVectorInit(arena, blocks.blocks);
  Compiled result = CompileExpr(heap, &blocks, true, &nscope, expr);
  FbleFree(arena, blocks.stack.xs);
  FbleFreeBlockNames(arena, &blocks.blocks);
  FreeScope(heap, &nscope);
  FbleFreeInstrBlock(arena, code);
  return result.type;
}

// CompileType --
//   Compile a type, returning its value.
//
// Inputs:
//   heap - heap to use for allocations.
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
static FbleType* CompileType(FbleTypeHeap* heap, Scope* scope, FbleTypeExpr* type)
{
  FbleArena* arena = heap->arena;
  switch (type->tag) {
    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* struct_type = (FbleStructTypeExpr*)type;
      FbleStructType* st = FbleNewType(heap, FbleStructType, FBLE_STRUCT_TYPE, type->loc);
      FbleVectorInit(arena, st->fields);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        FbleType* compiled = CompileType(heap, scope, field->type);
        if (compiled == NULL) {
          FbleTypeRelease(heap, &st->_base);
          return NULL;
        }

        if (!CheckNameSpace(arena, &field->name, compiled)) {
          FbleTypeRelease(heap, compiled);
          FbleTypeRelease(heap, &st->_base);
          return NULL;
        }

        FbleTaggedType* cfield = FbleVectorExtend(arena, st->fields);
        cfield->name = field->name;
        cfield->type = compiled;

        FbleTypeAddRef(heap, &st->_base, cfield->type);
        FbleTypeRelease(heap, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&field->name, &struct_type->fields.xs[j].name)) {
            ReportError(arena, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
            FbleTypeRelease(heap, &st->_base);
            return NULL;
          }
        }
      }
      return &st->_base;
    }

    case FBLE_UNION_TYPE_EXPR: {
      FbleUnionType* ut = FbleNewType(heap, FbleUnionType, FBLE_UNION_TYPE, type->loc);
      FbleVectorInit(arena, ut->fields);

      FbleUnionTypeExpr* union_type = (FbleUnionTypeExpr*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        FbleType* compiled = CompileType(heap, scope, field->type);
        if (compiled == NULL) {
          FbleTypeRelease(heap, &ut->_base);
          return NULL;
        }
        FbleTaggedType* cfield = FbleVectorExtend(arena, ut->fields);
        cfield->name = field->name;
        cfield->type = compiled;
        FbleTypeAddRef(heap, &ut->_base, cfield->type);
        FbleTypeRelease(heap, cfield->type);

        for (size_t j = 0; j < i; ++j) {
          if (FbleNamesEqual(&field->name, &union_type->fields.xs[j].name)) {
            ReportError(arena, &field->name.loc,
                "duplicate field name '%n'\n",
                &field->name);
            FbleTypeRelease(heap, &ut->_base);
            return NULL;
          }
        }
      }
      return &ut->_base;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncType* ft = FbleNewType(heap, FbleFuncType, FBLE_FUNC_TYPE, type->loc);
      ft->arg = NULL;
      ft->rtype = NULL;

      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;
      ft->arg = CompileType(heap, scope, func_type->arg);
      if (ft->arg == NULL) {
        FbleTypeRelease(heap, &ft->_base);
        return NULL;
      }
      FbleTypeAddRef(heap, &ft->_base, ft->arg);
      FbleTypeRelease(heap, ft->arg);

      FbleType* rtype = CompileType(heap, scope, func_type->rtype);
      if (rtype == NULL) {
        FbleTypeRelease(heap, &ft->_base);
        return NULL;
      }
      ft->rtype = rtype;
      FbleTypeAddRef(heap, &ft->_base, ft->rtype);
      FbleTypeRelease(heap, ft->rtype);
      return &ft->_base;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcType* ut = FbleNewType(heap, FbleProcType, FBLE_PROC_TYPE, type->loc);
      ut->type = NULL;

      FbleProcTypeExpr* unary_type = (FbleProcTypeExpr*)type;
      ut->type = CompileType(heap, scope, unary_type->type);
      if (ut->type == NULL) {
        FbleTypeRelease(heap, &ut->_base);
        return NULL;
      }
      FbleTypeAddRef(heap, &ut->_base, ut->type);
      FbleTypeRelease(heap, ut->type);
      return &ut->_base;
    }

    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* typeof = (FbleTypeofExpr*)type;
      return CompileExprNoInstrs(heap, scope, typeof->expr);
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
    case FBLE_LITERAL_EXPR: {
      FbleExpr* expr = type;
      FbleType* type = CompileExprNoInstrs(heap, scope, expr);
      if (type == NULL) {
        return NULL;
      }

      FbleType* type_value = FbleValueOfType(heap, type);
      if (type_value == NULL) {
        ReportError(arena, &expr->loc,
            "expected a type, but found value of type %t\n",
            type);
        FbleTypeRelease(heap, type);
        return NULL;
      }
      FbleTypeRelease(heap, type);
      return type_value;
    }
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// CompileProgram --
//   Type check and compile the given program.
//
// Inputs:
//   heap - heap to use for allocations.
//   blocks - the blocks stack.
//   scope - the list of variables in scope.
//   prgm - the program to compile.
//
// Results:
//   true if the program compiled successfully, false otherwise.
//
// Side effects:
//   Updates 'blocks' with compiled block information. Exits the current
//   block.
//   Appends instructions to scope for executing the given program.
//   There is no gaurentee about what instructions have been appended to
//   scope if the program fails to compile.
//   Prints warning messages to stderr.
//   Prints a message to stderr if the program fails to compile.
static bool CompileProgram(FbleTypeHeap* heap, Blocks* blocks, Scope* scope, FbleProgram* prgm)
{
  AddBlockTime(blocks, 1 + prgm->modules.size);

  FbleArena* arena = heap->arena;
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    EnterBlock(arena, blocks, prgm->modules.xs[i].name, prgm->modules.xs[i].value->loc, scope);
    Compiled module = CompileExpr(heap, blocks, false, scope, prgm->modules.xs[i].value);
    ExitBlock(arena, blocks, scope);

    if (module.type == NULL) {
      return false;
    }

    PushVar(arena, scope, prgm->modules.xs[i].name, module.type, module.local);
  }

  Compiled result = CompileExpr(heap, blocks, true, scope, prgm->main);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    PopVar(heap, scope);
  }

  FbleTypeRelease(heap, result.type);
  return result.type != NULL;
}

// FbleCompile -- see documentation in instr.h
FbleInstrBlock* FbleCompile(FbleArena* arena, FbleNameV* blocks, FbleProgram* program)
{
  Blocks block_stack;
  FbleVectorInit(arena, block_stack.stack);
  FbleVectorInit(arena, block_stack.blocks);

  FbleInstrBlock* code = NewInstrBlock(arena);

  // The entry associated with FBLE_ROOT_BLOCK_ID.
  FbleName entry_name = {
    .name = "",
    .loc = program->main->loc,
    .space = FBLE_NORMAL_NAME_SPACE
  };

  Scope scope;
  InitScope(arena, &scope, code, NULL, NULL);

  FbleTypeHeap* heap = FbleNewTypeHeap(arena);
  EnterBlock(arena, &block_stack, entry_name, program->main->loc, &scope);
  bool ok = CompileProgram(heap, &block_stack, &scope, program);
  ExitBlock(arena, &block_stack, NULL);
  FreeScope(heap, &scope);
  FbleFreeTypeHeap(heap);

  assert(block_stack.stack.size == 0);
  FbleFree(arena, block_stack.stack.xs);
  *blocks = block_stack.blocks;

  if (!ok) {
    FbleFreeInstrBlock(arena, code);
    return NULL;
  }
  return code;
}
