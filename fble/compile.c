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
// instrs - a record of the var instructions used to access the variable.
typedef struct {
  FbleName name;
  Type* type;
  FbleVarInstrV instrs;
} Var;

// Var --
//   A vector of vars.
typedef struct {
  size_t size;
  Var* xs;
} VarV;

// Vars --
//   Scope of variables visible during type checking.
//
// Fields:
//   vars - The data for vars in scope.
//   nvars - the number of entries of vars that are valid.
typedef struct {
  VarV vars;
  size_t nvars;
} Vars;

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

static void PushVar(FbleArena* arena, Vars* vars, FbleName name, Type* type);
static void PopVar(FbleArena* arena, Vars* vars);
static void FreeVars(FbleArena* arena, Vars* vars);

static void EnterBlock(FbleArena* arena, Blocks* blocks, FbleName name, FbleLoc loc, FbleInstrV* instrs);
static void EnterBodyBlock(FbleArena* arena, Blocks* blocks, FbleLoc loc, FbleInstrV* instrs);
static void ExitBlock(FbleArena* arena, Blocks* blocks, FbleInstrV* instrs);
static void AddBlockTime(Blocks* blocks, size_t time);

static void EnterThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars);
static size_t ExitThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars, FbleInstrV* instrs);

static FbleInstrBlock* NewInstrBlock(FbleArena* arena);
static void FreeInstr(FbleArena* arena, FbleInstr* instr);

static bool CheckNameSpace(FbleTypeArena* arena, FbleName* name, Type* type);

static Type* ValueOfType(FbleTypeArena* arena, Type* typeof);

static void CompileExit(FbleArena* arena, bool exit, FbleInstrV* instrs);
static Type* CompileExpr(FbleTypeArena* arena, Blocks* blocks, bool exit, Vars* vars, FbleExpr* expr, FbleInstrV* instrs);
static Type* CompileList(FbleTypeArena* arena, Blocks* blocks, bool exit, Vars* vars, FbleLoc loc, FbleTypeExpr* type, FbleExprV args, FbleInstrV* instrs);
static Type* CompileExprNoInstrs(FbleTypeArena* arena, Vars* vars, FbleExpr* expr);
static Type* CompileType(FbleTypeArena* arena, Vars* vars, FbleTypeExpr* type);
static Kind* CompileKind(FbleArena* arena, FbleKind* kind);
static Type* CompileProgram(FbleTypeArena* arena, Blocks* blocks, Vars* vars, FbleProgram* prgm, FbleInstrV* instrs);

// ReportError --
//   Report a compiler error.
//
//   This uses a printf-like format string. The following format specifiers
//   are supported:
//     %i - size_t
//     %k - Kind*
//     %n - FbleName*
//     %t - Type*
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
        Kind* kind = va_arg(ap, Kind*);
        FblePrintKind(kind);
        break;
      }

      case 'n': {
        FbleName* name = va_arg(ap, FbleName*);
        FblePrintName(stderr, name);
        break;
      }

      case 't': {
        Type* type = va_arg(ap, Type*);
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
//   vars - the scope to push the variable on to.
//   name - the name of the variable.
//   type - the type of the variable.
//
// Results:
//   none
//
// Side effects:
//   Pushes a new variable with given name and type onto the scope. It is the
//   callers responsibility to ensure that the type stays alive as long as is
//   needed.
static void PushVar(FbleArena* arena, Vars* vars, FbleName name, Type* type)
{
  Var* var = vars->vars.xs + vars->nvars;
  if (vars->nvars == vars->vars.size) {
    var = FbleVectorExtend(arena, vars->vars);
    FbleVectorInit(arena, var->instrs);
  }
  var->name = name;
  var->type = type;
  vars->nvars++;
}

// PopVar --
//   Pops a var off the given scope.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the scope to pop from.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top var off the scope.
static void PopVar(FbleArena* arena, Vars* vars)
{
  assert(vars->nvars > 0);
  vars->nvars--;
}

// FreeVars --
//   Free memory associated with vars.
//
// Inputs:
//   arena - arena for allocations.
//   vars - the vars to free memory for.
//
// Results:
//   none.
//
// Side effects:
//   Memory allocated for vars is freed.
static void FreeVars(FbleArena* arena, Vars* vars)
{
  for (size_t i = 0; i < vars->vars.size; ++i) {
    FbleFree(arena, vars->vars.xs[i].instrs.xs);
  }
  FbleFree(arena, vars->vars.xs);
}

// EnterBlock --
//   Enter a new profiling block.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   name - name to add to the current block path for naming the new block.
//   loc - the location of the block.
//   instrs - where to add the ENTER_BLOCK instruction to.
//
// Results:
//   none.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock.
static void EnterBlock(FbleArena* arena, Blocks* blocks, FbleName name, FbleLoc loc, FbleInstrV* instrs)
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
  FbleVectorAppend(arena, *instrs, &enter->_base);

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
//   instrs - where to add the ENTER_BLOCK instruction to.
//
// Results:
//   none.
//
// Side effects:
//   Adds a new block to the blocks stack. Change the current block to the new
//   block. Outputs an ENTER_BLOCK instruction to instrs. The block should be
//   exited when no longer in scope using ExitBlock or one of the other
//   functions that exit a block.
static void EnterBodyBlock(FbleArena* arena, Blocks* blocks, FbleLoc loc, FbleInstrV* instrs)
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
  FbleVectorAppend(arena, *instrs, &enter->_base);

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
//   instrs - where to generate the PROFILE_EXIT_BLOCK instruction, or NULL to
//            indicate that no instruction should be generated.
//
// Results:
//   none.
//
// Side effects:
//   Pops the top block frame off the blocks stack and append a
//   PROFILE_EXIT_BLOCK instruction to instrs if instrs is non-null.
static void ExitBlock(FbleArena* arena, Blocks* blocks, FbleInstrV* instrs)
{
  assert(blocks->stack.size > 0);
  blocks->stack.size--;

  if (instrs != NULL) {
    FbleProfileExitBlockInstr* exit_instr = FbleAlloc(arena, FbleProfileExitBlockInstr);
    exit_instr->_base.tag = FBLE_PROFILE_EXIT_BLOCK_INSTR;
    FbleVectorAppend(arena, *instrs, &exit_instr->_base);
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

// EnterThunk --
//   Prepare to capture variables for a thunk.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the current variable scope.
//   thunk_vars - a variable scope to use in the thunk.
//
// Results:
//   None.
//
// Side effects:
//   Initializes thunk_vars based on vars. ExitThunk or FreeVars should be
//   called to free the allocations for thunk_vars.
static void EnterThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars)
{
  FbleVectorInit(arena, thunk_vars->vars);
  thunk_vars->nvars = 0;
  for (size_t i = 0; i < vars->nvars; ++i) {
    PushVar(arena, thunk_vars, vars->vars.xs[i].name, vars->vars.xs[i].type);
  }
}

// ExitThunk --
//   Generate instructions to capture just the variables accessed for a thunk.
//
// Inputs:
//   arena - arena to use for allocations
//   vars - the current variable scope.
//   thunk_vars - the thunk variable scope, created with EnterThunk.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   The number of variables accessed in the thunk.
//
// Side effects:
//   * Frees memory associated with thunk_vars.
//   * Appends instructions to instrs to push captured variables to the data
//     stack.
//   * Updates thunk instructions to point to the captured variable indicies
//     instead of the original variable indicies.
static size_t ExitThunk(FbleArena* arena, Vars* vars, Vars* thunk_vars, FbleInstrV* instrs)
{
  size_t scopec = 0;
  for (size_t i = 0; i < vars->nvars; ++i) {
    if (thunk_vars->vars.xs[i].instrs.size > 0) {
      // Copy the accessed var to the data stack for capturing.
      FbleVarInstr* get_var = FbleAlloc(arena, FbleVarInstr);
      get_var->_base.tag = FBLE_VAR_INSTR;
      get_var->position = i;
      FbleVectorAppend(arena, vars->vars.xs[i].instrs, get_var);
      FbleVectorAppend(arena, *instrs, &get_var->_base);

      // Update references to this var.
      for (size_t j = 0; j < thunk_vars->vars.xs[i].instrs.size; ++j) {
        FbleVarInstr* var_instr = thunk_vars->vars.xs[i].instrs.xs[j];
        var_instr->position = scopec;
      }
      scopec++;
    }
  }

  // Update references to all vars first introduced in the thunk scope.
  for (size_t i = vars->nvars; i < thunk_vars->vars.size; ++i) {
      for (size_t j = 0; j < thunk_vars->vars.xs[i].instrs.size; ++j) {
        FbleVarInstr* var_instr = thunk_vars->vars.xs[i].instrs.xs[j];
        var_instr->position = i - vars->nvars + scopec;
      }
  }

  FreeVars(arena, thunk_vars);
  return scopec;
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
    case FBLE_FORK_INSTR:
    case FBLE_PROC_INSTR:
    case FBLE_JOIN_INSTR:
    case FBLE_LET_PREP_INSTR:
    case FBLE_LET_DEF_INSTR:
    case FBLE_STRUCT_IMPORT_INSTR:
    case FBLE_EXIT_SCOPE_INSTR:
    case FBLE_TYPE_INSTR:
    case FBLE_VPUSH_INSTR:
    case FBLE_PROFILE_ENTER_BLOCK_INSTR:
    case FBLE_PROFILE_EXIT_BLOCK_INSTR:
    case FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR:
    {
      FbleFree(arena, instr);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
      FbleFreeInstrBlock(arena, func_value_instr->body);
      FbleFree(arena, func_value_instr);
      return;
    }

    case FBLE_PROC_VALUE_INSTR: {
      FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
      FbleFreeInstrBlock(arena, proc_value_instr->body);
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
static bool CheckNameSpace(FbleTypeArena* arena, FbleName* name, Type* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  Kind* kind = FbleGetKind(arena_, type);
  size_t kind_level = FbleGetKindLevel(kind);
  FreeKind(arena_, kind);

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
static Type* ValueOfType(FbleTypeArena* arena, Type* typeof)
{
  if (typeof->tag == TYPE_TYPE) {
    TypeType* tt = (TypeType*)typeof;
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
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   none.
//
// Side effects:
//   If exit is true, appends an exit scope instruction to instrs
static void CompileExit(FbleArena* arena, bool exit, FbleInstrV* instrs)
{
  if (exit) {
    FbleExitScopeInstr* exit_scope = FbleAlloc(arena, FbleExitScopeInstr);
    exit_scope->_base.tag = FBLE_EXIT_SCOPE_INSTR;
    FbleVectorAppend(arena, *instrs, &exit_scope->_base);
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
//   vars - the list of variables in scope.
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
static Type* CompileExpr(FbleTypeArena* arena, Blocks* blocks, bool exit, Vars* vars, FbleExpr* expr, FbleInstrV* instrs)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (expr->tag) {
    case FBLE_STRUCT_TYPE_EXPR:
    case FBLE_UNION_TYPE_EXPR:
    case FBLE_FUNC_TYPE_EXPR:
    case FBLE_PROC_TYPE_EXPR:
    case FBLE_TYPEOF_EXPR: {
      AddBlockTime(blocks, 1);

      Type* type = CompileType(arena, vars, expr);
      if (type == NULL) {
        return NULL;
      }

      TypeType* type_type = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = TYPE_TYPE;
      type_type->_base.loc = expr->loc;
      type_type->_base.evaluating = false;
      type_type->type = type;
      FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
      FbleTypeRelease(arena, type);

      FbleTypeInstr* instr = FbleAlloc(arena_, FbleTypeInstr);
      instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      CompileExit(arena_, exit, instrs);
      return &type_type->_base;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* misc_apply_expr = (FbleMiscApplyExpr*)expr;
      bool error = false;

      size_t argc = misc_apply_expr->args.size;
      AddBlockTime(blocks, 1 + argc);
      Type* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - 1 - i;
        arg_types[j] = CompileExpr(arena, blocks, false, vars, misc_apply_expr->args.xs[j], instrs);
        error = error || (arg_types[j] == NULL);
      }

      Type* type = CompileExpr(arena, blocks, false, vars, misc_apply_expr->misc, instrs);
      error = error || (type == NULL);

      if (error) {
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(arena, arg_types[i]);
        }
        FbleTypeRelease(arena, type);
        return NULL;
      }

      Type* normal = FbleNormalType(type);
      switch (normal->tag) {
        case FUNC_TYPE: {
          // FUNC_APPLY
          for (size_t i = 0; i < argc; ++i) {
            if (normal->tag != FUNC_TYPE) {
              ReportError(arena_, &expr->loc, "too many arguments to function\n");
              for (size_t i = 0; i < argc; ++i) {
                FbleTypeRelease(arena, arg_types[i]);
              }
              FbleTypeRelease(arena, type);
              return NULL;
            }

            FuncType* func_type = (FuncType*)normal;
            if (!FbleTypesEqual(func_type->arg, arg_types[i])) {
              ReportError(arena_, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  func_type->arg, arg_types[i]);
              for (size_t j = 0; j < argc; ++j) {
                FbleTypeRelease(arena, arg_types[j]);
              }
              FbleTypeRelease(arena, type);
              return NULL;
            }

            FbleFuncApplyInstr* apply_instr = FbleAlloc(arena_, FbleFuncApplyInstr);
            apply_instr->_base.tag = FBLE_FUNC_APPLY_INSTR;
            apply_instr->loc = misc_apply_expr->misc->loc;
            apply_instr->exit = exit && (i+1 == argc);
            FbleVectorAppend(arena_, *instrs, &apply_instr->_base);

            normal = FbleNormalType(func_type->rtype);
          }

          for (size_t i = 0; i < argc; ++i) {
            FbleTypeRelease(arena, arg_types[i]);
          }

          FbleTypeRetain(arena, normal);
          FbleTypeRelease(arena, type);
          return normal;
        }

        case TYPE_TYPE: {
          // FBLE_STRUCT_VALUE_EXPR
          TypeType* type_type = (TypeType*)normal;
          Type* vtype = FbleTypeRetain(arena, type_type->type);
          FbleTypeRelease(arena, type);

          StructType* struct_type = (StructType*)FbleNormalType(vtype);
          if (struct_type->_base.tag != STRUCT_TYPE) {
            ReportError(arena_, &misc_apply_expr->misc->loc,
                "expected a struct type, but found %t\n",
                vtype);
            for (size_t i = 0; i < argc; ++i) {
              FbleTypeRelease(arena, arg_types[i]);
            }
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
            FbleTypeRelease(arena, vtype);
            return NULL;
          }

          bool error = false;
          for (size_t i = 0; i < argc; ++i) {
            Field* field = struct_type->fields.xs + i;

            if (!FbleTypesEqual(field->type, arg_types[i])) {
              ReportError(arena_, &misc_apply_expr->args.xs[i]->loc,
                  "expected type %t, but found %t\n",
                  field->type, arg_types[i]);
              error = true;
            }
            FbleTypeRelease(arena, arg_types[i]);
          }

          if (error) {
            FbleTypeRelease(arena, vtype);
            return NULL;
          }

          FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
          struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
          struct_instr->argc = struct_type->fields.size;
          FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
          CompileExit(arena_, exit, instrs);
          return vtype;
        }

        default: {
          ReportError(arena_, &expr->loc,
              "expecting a function or struct type, but found something of type %t\n",
              type);
          for (size_t i = 0; i < argc; ++i) {
            FbleTypeRelease(arena, arg_types[i]);
          }
          FbleTypeRelease(arena, type);
          return NULL;
        }
      }

      UNREACHABLE("Should never get here");
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      AddBlockTime(blocks, 1);

      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      StructType* struct_type = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &struct_type->_base.ref);
      struct_type->_base.tag = STRUCT_TYPE;
      struct_type->_base.loc = expr->loc;
      struct_type->_base.evaluating = false;
      FbleVectorInit(arena_, struct_type->fields);

      size_t argc = struct_expr->args.size;
      Type* arg_types[argc];
      bool error = false;
      for (size_t i = 0; i < argc; ++i) {
        size_t j = argc - i - 1;
        FbleTaggedExpr* arg = struct_expr->args.xs + j;
        EnterBlock(arena_, blocks, arg->name, arg->expr->loc, instrs);
        arg_types[j] = CompileExpr(arena, blocks, false, vars, arg->expr, instrs);
        ExitBlock(arena_, blocks, instrs);
        error = error || (arg_types[j] == NULL);
      }

      for (size_t i = 0; i < argc; ++i) {
        FbleTaggedExpr* arg = struct_expr->args.xs + i;
        if (arg_types[i] != NULL) {
          if (!CheckNameSpace(arena, &arg->name, arg_types[i])) {
            error = true;
          }

          Field* cfield = FbleVectorExtend(arena_, struct_type->fields);
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
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      FbleStructValueInstr* struct_instr = FbleAlloc(arena_, FbleStructValueInstr);
      struct_instr->_base.tag = FBLE_STRUCT_VALUE_INSTR;
      struct_instr->argc = struct_expr->args.size;
      FbleVectorAppend(arena_, *instrs, &struct_instr->_base);
      CompileExit(arena_, exit, instrs);
      return &struct_type->_base;
    }

    case FBLE_UNION_VALUE_EXPR: {
      AddBlockTime(blocks, 1);
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Type* type = CompileType(arena, vars, union_value_expr->type);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)FbleNormalType(type);
      if (union_type->_base.tag != UNION_TYPE) {
        ReportError(arena_, &union_value_expr->type->loc,
            "expected a union type, but found %t\n", type);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      Type* field_type = NULL;
      size_t tag = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        Field* field = union_type->fields.xs + i;
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
        FbleTypeRelease(arena, type);
        return NULL;
      }

      Type* arg_type = CompileExpr(arena, blocks, false, vars, union_value_expr->arg, instrs);
      if (arg_type == NULL) {
        FbleTypeRelease(arena, type);
        return NULL;
      }

      if (!FbleTypesEqual(field_type, arg_type)) {
        ReportError(arena_, &union_value_expr->arg->loc,
            "expected type %t, but found type %t\n",
            field_type, arg_type);
        FbleTypeRelease(arena, type);
        FbleTypeRelease(arena, arg_type);
        return NULL;
      }
      FbleTypeRelease(arena, arg_type);

      FbleUnionValueInstr* union_instr = FbleAlloc(arena_, FbleUnionValueInstr);
      union_instr->_base.tag = FBLE_UNION_VALUE_INSTR;
      union_instr->tag = tag;
      FbleVectorAppend(arena_, *instrs, &union_instr->_base);
      CompileExit(arena_, exit, instrs);
      return type;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      AddBlockTime(blocks, 1);

      FbleMiscAccessExpr* access_expr = (FbleMiscAccessExpr*)expr;

      Type* type = CompileExpr(arena, blocks, false, vars, access_expr->object, instrs);
      if (type == NULL) {
        return NULL;
      }

      FbleAccessInstr* access = FbleAlloc(arena_, FbleAccessInstr);
      access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
      access->loc = access_expr->field.loc;
      FbleVectorAppend(arena_, *instrs, &access->_base);

      CompileExit(arena_, exit, instrs);

      Type* normal = FbleNormalType(type);
      FieldV* fields = NULL;
      if (normal->tag == STRUCT_TYPE) {
        access->_base.tag = FBLE_STRUCT_ACCESS_INSTR;
        fields = &((StructType*)normal)->fields;
      } else if (normal->tag == UNION_TYPE) {
        access->_base.tag = FBLE_UNION_ACCESS_INSTR;
        fields = &((UnionType*)normal)->fields;
      } else {
        ReportError(arena_, &access_expr->object->loc,
            "expected value of type struct or union, but found value of type %t\n",
            type);

        FbleTypeRelease(arena, type);
        return NULL;
      }

      for (size_t i = 0; i < fields->size; ++i) {
        if (FbleNamesEqual(&access_expr->field, &fields->xs[i].name)) {
          access->tag = i;
          Type* rtype = FbleTypeRetain(arena, fields->xs[i].type);
          FbleTypeRelease(arena, type);
          return rtype;
        }
      }

      ReportError(arena_, &access_expr->field.loc,
          "'%n' is not a field of type %t\n",
          &access_expr->field, type);
      FbleTypeRelease(arena, type);
      return NULL;
    }

    case FBLE_UNION_SELECT_EXPR: {
      // TODO: Should time be O(lg(N)) instead of O(1), where N is the number
      // of fields in the union/struct?
      AddBlockTime(blocks, 1);

      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;

      Type* type = CompileExpr(arena, blocks, false, vars, select_expr->condition, instrs);
      if (type == NULL) {
        return NULL;
      }

      UnionType* union_type = (UnionType*)FbleNormalType(type);
      if (union_type->_base.tag != UNION_TYPE) {
        ReportError(arena_, &select_expr->condition->loc,
            "expected value of union type, but found value of type %t\n",
            type);
        FbleTypeRelease(arena, type);
        return NULL;
      }

      if (exit) {
        FbleProfileAutoExitBlockInstr* exit_instr = FbleAlloc(arena_, FbleProfileAutoExitBlockInstr);
        exit_instr->_base.tag = FBLE_PROFILE_AUTO_EXIT_BLOCK_INSTR;
        FbleVectorAppend(arena_, *instrs, &exit_instr->_base);
      }

      FbleUnionSelectInstr* select_instr = FbleAlloc(arena_, FbleUnionSelectInstr);
      select_instr->_base.tag = FBLE_UNION_SELECT_INSTR;
      select_instr->loc = select_expr->condition->loc;
      FbleVectorAppend(arena_, *instrs, &select_instr->_base);

      Type* return_type = NULL;
      FbleGotoInstr* enter_gotos[union_type->fields.size];
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        enter_gotos[i] = FbleAlloc(arena_, FbleGotoInstr);
        enter_gotos[i]->_base.tag = FBLE_GOTO_INSTR;
        FbleVectorAppend(arena_, *instrs, &enter_gotos[i]->_base);
      }

      size_t default_pc = instrs->size;
      FbleGotoInstr* exit_goto_default = NULL;
      if (select_expr->default_ != NULL) {
        FbleName name = {
          .name = ":",
          .loc = select_expr->default_->loc,  // unused.
          .space = FBLE_NORMAL_NAME_SPACE
        };
        EnterBlock(arena_, blocks, name, select_expr->default_->loc, instrs);
        return_type = CompileExpr(arena, blocks, exit, vars, select_expr->default_, instrs);
        ExitBlock(arena_, blocks, exit ? NULL : instrs);

        if (return_type == NULL) {
          FbleTypeRelease(arena, type);
          return NULL;
        }

        if (!exit) {
          exit_goto_default = FbleAlloc(arena_, FbleGotoInstr);
          exit_goto_default->_base.tag = FBLE_GOTO_INSTR;
          FbleVectorAppend(arena_, *instrs, &exit_goto_default->_base);
        }
      }

      FbleGotoInstr* exit_gotos[select_expr->choices.size];
      size_t choice = 0;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        if (choice < select_expr->choices.size && FbleNamesEqual(&select_expr->choices.xs[choice].name, &union_type->fields.xs[i].name)) {
          enter_gotos[i]->pc = instrs->size;

          EnterBlock(arena_, blocks,
              select_expr->choices.xs[choice].name,
              select_expr->choices.xs[choice].expr->loc,
              instrs);
          AddBlockTime(blocks, 1);
          Type* arg_type = CompileExpr(arena, blocks, exit, vars, select_expr->choices.xs[choice].expr, instrs);
          ExitBlock(arena_, blocks, exit ? NULL : instrs);

          if (!exit) {
            exit_gotos[choice] = FbleAlloc(arena_, FbleGotoInstr);
            exit_gotos[choice]->_base.tag = FBLE_GOTO_INSTR;
            FbleVectorAppend(arena_, *instrs, &exit_gotos[choice]->_base);
          }

          if (arg_type == NULL) {
            FbleTypeRelease(arena, return_type);
            FbleTypeRelease(arena, type);
            return NULL;
          }

          if (return_type == NULL) {
            return_type = arg_type;
          } else {
            if (!FbleTypesEqual(return_type, arg_type)) {
              ReportError(arena_, &select_expr->choices.xs[choice].expr->loc,
                  "expected type %t, but found %t\n",
                  return_type, arg_type);

              FbleTypeRelease(arena, type);
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
          FbleTypeRelease(arena, return_type);
          FbleTypeRelease(arena, type);
          return NULL;
        } else {
          enter_gotos[i]->pc = default_pc;
        }
      }
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
          exit_goto_default->pc = instrs->size;
        }
        for (size_t i = 0; i < select_expr->choices.size; ++i) {
          exit_gotos[i]->pc = instrs->size;
        }
      }
      return return_type;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;
      size_t argc = func_value_expr->args.size;

      bool error = false;
      Type* arg_types[argc];
      for (size_t i = 0; i < argc; ++i) {
        arg_types[i] = CompileType(arena, vars, func_value_expr->args.xs[i].type);
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
      instr->body = NewInstrBlock(arena_);
      instr->argc = argc;

      EnterBodyBlock(arena_, blocks, func_value_expr->body->loc, &instr->body->instrs);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      for (size_t i = 0; i < argc; ++i) {
        PushVar(arena_, &thunk_vars, func_value_expr->args.xs[i].name, arg_types[i]);
      }

      Type* type = CompileExpr(arena, blocks, true, &thunk_vars, func_value_expr->body, &instr->body->instrs);
      ExitBlock(arena_, blocks, NULL);
      if (type == NULL) {
        FreeVars(arena_, &thunk_vars);
        FreeInstr(arena_, &instr->_base);
        for (size_t i = 0; i < argc; ++i) {
          FbleTypeRelease(arena, arg_types[i]);
        }
        return NULL;
      }

      instr->scopec = ExitThunk(arena_, vars, &thunk_vars, instrs);

      // TODO: Is it right for time to be proportional to number of captured
      // variables?
      AddBlockTime(blocks, instr->scopec);
      vpush->count = instr->scopec + argc;
      FbleVectorAppend(arena_, *instrs, &instr->_base);

      CompileExit(arena_, exit, instrs);

      for (size_t i = 0; i < argc; ++i) {
        Type* arg_type = arg_types[argc - 1 - i];
        FuncType* ft = FbleAlloc(arena_, FuncType);
        FbleRefInit(arena, &ft->_base.ref);
        ft->_base.tag = FUNC_TYPE;
        ft->_base.loc = expr->loc;
        ft->_base.evaluating = false;
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

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_);

      EnterBodyBlock(arena_, blocks, expr->loc, &instr->body->instrs);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      Type* type = CompileExpr(arena, blocks, false, &thunk_vars, eval_expr->body, &instr->body->instrs);

      CompileExit(arena_, true, &instr->body->instrs);
      ExitBlock(arena_, blocks, NULL);

      instr->scopec = ExitThunk(arena_, vars, &thunk_vars, instrs);
      vpush->count = instr->scopec;
      FbleVectorAppend(arena_, *instrs, &instr->_base);
      CompileExit(arena_, exit, instrs);

      if (type == NULL) {
        return NULL;
      }

      ProcType* proc_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &proc_type->_base.ref);
      proc_type->_base.tag = PROC_TYPE;
      proc_type->_base.loc = expr->loc;
      proc_type->_base.evaluating = false;
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

      Type* port_type = CompileType(arena, vars, link_expr->type);
      if (port_type == NULL) {
        return NULL;
      }

      ProcType* get_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &get_type->_base.ref);
      get_type->_base.tag = PROC_TYPE;
      get_type->_base.loc = port_type->loc;
      get_type->_base.evaluating = false;
      get_type->type = port_type;
      FbleRefAdd(arena, &get_type->_base.ref, &get_type->type->ref);

      StructType* unit_type = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &unit_type->_base.ref);
      unit_type->_base.tag = STRUCT_TYPE;
      unit_type->_base.loc = expr->loc;
      unit_type->_base.evaluating = false;
      FbleVectorInit(arena_, unit_type->fields);

      ProcType* unit_proc_type = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &unit_proc_type->_base.ref);
      unit_proc_type->_base.tag = PROC_TYPE;
      unit_proc_type->_base.loc = expr->loc;
      unit_proc_type->_base.evaluating = false;
      unit_proc_type->type = &unit_type->_base;
      FbleRefAdd(arena, &unit_proc_type->_base.ref, &unit_proc_type->type->ref);
      FbleTypeRelease(arena, &unit_type->_base);

      FuncType* put_type = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &put_type->_base.ref);
      put_type->_base.tag = FUNC_TYPE;
      put_type->_base.loc = expr->loc;
      put_type->_base.evaluating = false;
      put_type->arg = port_type;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->arg->ref);
      put_type->rtype = &unit_proc_type->_base;
      FbleRefAdd(arena, &put_type->_base.ref, &put_type->rtype->ref);
      FbleTypeRelease(arena, &unit_proc_type->_base);

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_);

      EnterBodyBlock(arena_, blocks, link_expr->body->loc, &instr->body->instrs);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      FbleLinkInstr* link = FbleAlloc(arena_, FbleLinkInstr);
      link->_base.tag = FBLE_LINK_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &link->_base);

      FbleVPushInstr* vpush_ports = FbleAlloc(arena_, FbleVPushInstr);
      vpush_ports->_base.tag = FBLE_VPUSH_INSTR;
      vpush_ports->count = 2;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush_ports->_base);
      PushVar(arena_, &thunk_vars, link_expr->get, &get_type->_base);
      PushVar(arena_, &thunk_vars, link_expr->put, &put_type->_base);

      Type* type = CompileExpr(arena, blocks, false, &thunk_vars, link_expr->body, &instr->body->instrs);

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      proc->exit = true;
      FbleVectorAppend(arena_, instr->body->instrs, &proc->_base);
      ExitBlock(arena_, blocks, NULL);

      instr->scopec = ExitThunk(arena_, vars, &thunk_vars, instrs);
      vpush->count = instr->scopec;
      FbleVectorAppend(arena_, *instrs, &instr->_base);
      CompileExit(arena_, exit, instrs);

      FbleTypeRelease(arena, port_type);
      FbleTypeRelease(arena, &get_type->_base);
      FbleTypeRelease(arena, &put_type->_base);

      if (type == NULL) {
        FbleTypeRelease(arena, type);
        return NULL;
      }

      if (FbleNormalType(type)->tag != PROC_TYPE) {
        ReportError(arena_, &link_expr->body->loc,
            "expected a value of type proc, but found %t\n",
            type);
        FbleTypeRelease(arena, type);
        return NULL;
      }
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
        nvd[i].type = CompileType(arena, vars, exec_expr->bindings.xs[i].type);
        error = error || (nvd[i].type == NULL);
      }

      Vars thunk_vars;
      EnterThunk(arena_, vars, &thunk_vars);

      FbleProcValueInstr* instr = FbleAlloc(arena_, FbleProcValueInstr);
      instr->_base.tag = FBLE_PROC_VALUE_INSTR;
      instr->body = NewInstrBlock(arena_);

      EnterBodyBlock(arena_, blocks, exec_expr->body->loc, &instr->body->instrs);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &vpush->_base);

      FbleForkInstr* fork = FbleAlloc(arena_, FbleForkInstr);
      fork->_base.tag = FBLE_FORK_INSTR;
      fork->argc = exec_expr->bindings.size;
      FbleVectorAppend(arena_, instr->body->instrs, &fork->_base);

      FbleJoinInstr* join = FbleAlloc(arena_, FbleJoinInstr);
      join->_base.tag = FBLE_JOIN_INSTR;
      FbleVectorAppend(arena_, instr->body->instrs, &join->_base);

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {
        PushVar(arena_, &thunk_vars, nvd[i].name, nvd[i].type);
      }

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileExpr(arena, blocks, false, &thunk_vars, exec_expr->body, &instr->body->instrs);
        error = (rtype == NULL);
      }

      if (rtype != NULL && FbleNormalType(rtype)->tag != PROC_TYPE) {
        error = true;
        ReportError(arena_, &exec_expr->body->loc,
            "expected a value of type proc, but found %t\n",
            rtype);
      }

      FbleProcInstr* proc = FbleAlloc(arena_, FbleProcInstr);
      proc->_base.tag = FBLE_PROC_INSTR;
      proc->exit = true;
      FbleVectorAppend(arena_, instr->body->instrs, &proc->_base);
      ExitBlock(arena_, blocks, NULL);

      size_t captured = ExitThunk(arena_, vars, &thunk_vars, instrs);
      instr->scopec = captured + exec_expr->bindings.size;
      vpush->count = captured;

      for (size_t i = 0; i < exec_expr->bindings.size; ++i) {

        Vars bthunk_vars;
        EnterThunk(arena_, vars, &bthunk_vars);

        FbleProcValueInstr* binstr = FbleAlloc(arena_, FbleProcValueInstr);
        binstr->_base.tag = FBLE_PROC_VALUE_INSTR;
        binstr->body = NewInstrBlock(arena_);

        EnterBodyBlock(arena_, blocks, exec_expr->bindings.xs[i].expr->loc, &binstr->body->instrs);

        FbleVPushInstr* bvpush = FbleAlloc(arena_, FbleVPushInstr);
        bvpush->_base.tag = FBLE_VPUSH_INSTR;
        FbleVectorAppend(arena_, binstr->body->instrs, &bvpush->_base);

        Type* type = CompileExpr(arena, blocks, false, &bthunk_vars, exec_expr->bindings.xs[i].expr, &binstr->body->instrs);

        FbleProcInstr* bproc = FbleAlloc(arena_, FbleProcInstr);
        bproc->_base.tag = FBLE_PROC_INSTR;
        bproc->exit = true;
        FbleVectorAppend(arena_, binstr->body->instrs, &bproc->_base);
        ExitBlock(arena_, blocks, NULL);

        binstr->scopec = ExitThunk(arena_, vars, &bthunk_vars, instrs);
        bvpush->count = binstr->scopec;
        FbleVectorAppend(arena_, *instrs, &binstr->_base);

        error = error || (type == NULL);

        if (type != NULL) {
          ProcType* proc_type = (ProcType*)FbleNormalType(type);
          if (proc_type->_base.tag == PROC_TYPE) {
            if (nvd[i].type != NULL && !FbleTypesEqual(nvd[i].type, proc_type->type)) {
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
          FbleTypeRelease(arena, type);
        }
        FbleTypeRelease(arena, nvd[i].type);
      }

      FbleVectorAppend(arena_, *instrs, &instr->_base);
      CompileExit(arena_, exit, instrs);

      if (error) {
        FbleTypeRelease(arena, rtype);
        return NULL;
      }

      return rtype;
    }

    case FBLE_VAR_EXPR: {
      AddBlockTime(blocks, 1);
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;

      for (size_t i = 0; i < vars->nvars; ++i) {
        size_t j = vars->nvars - i - 1;
        Var* var = vars->vars.xs + j;
        if (FbleNamesEqual(&var_expr->var, &var->name)) {
          FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
          instr->_base.tag = FBLE_VAR_INSTR;
          instr->position = j;
          FbleVectorAppend(arena_, vars->vars.xs[j].instrs, instr);
          FbleVectorAppend(arena_, *instrs, &instr->_base);
          CompileExit(arena_, exit, instrs);
          return FbleTypeRetain(arena, var->type);
        }
      }

      ReportError(arena_, &var_expr->var.loc,
          "variable '%n' not defined\n",
          &var_expr->var);
      return NULL;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;
      bool error = false;
      AddBlockTime(blocks, 1 + let_expr->bindings.size);

      // Evaluate the types of the bindings and set up the new vars.
      Var nvd[let_expr->bindings.size];
      VarType* var_types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleBinding* binding = let_expr->bindings.xs + i;
        var_types[i] = NULL;

        nvd[i].name = let_expr->bindings.xs[i].name;
        if (binding->type == NULL) {
          assert(binding->kind != NULL);
          VarType* var = FbleAlloc(arena_, VarType);
          FbleRefInit(arena, &var->_base.ref);
          var->_base.tag = VAR_TYPE;
          var->_base.loc = binding->name.loc;
          var->_base.evaluating = false;
          var->name = let_expr->bindings.xs[i].name;
          var->kind = CompileKind(arena_, binding->kind);
          var->value = NULL;

          TypeType* type_type = FbleAlloc(arena_, TypeType);
          FbleRefInit(arena, &type_type->_base.ref);
          type_type->_base.tag = TYPE_TYPE;
          type_type->_base.loc = binding->name.loc;
          type_type->_base.evaluating = false;
          type_type->type = &var->_base;
          FbleRefAdd(arena, &type_type->_base.ref, &type_type->type->ref);
          FbleTypeRelease(arena, &var->_base);

          nvd[i].type = &type_type->_base;
          var_types[i] = var;
        } else {
          assert(binding->kind == NULL);
          nvd[i].type = CompileType(arena, vars, binding->type);
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

      FbleLetPrepInstr* let_prep_instr = FbleAlloc(arena_, FbleLetPrepInstr);
      let_prep_instr->_base.tag = FBLE_LET_PREP_INSTR;
      let_prep_instr->count = let_expr->bindings.size;
      FbleVectorAppend(arena_, *instrs, &let_prep_instr->_base);

      size_t vi = vars->nvars;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PushVar(arena_, vars, nvd[i].name, nvd[i].type);
      }

      // Compile the values of the variables.
      Type* var_type_values[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        var_type_values[i] = NULL;
        FbleBinding* binding = let_expr->bindings.xs + i;

        Type* type = NULL;
        if (!error) {
          EnterBlock(arena_, blocks, binding->name, binding->expr->loc, instrs);
          type = CompileExpr(arena, blocks, false, vars, binding->expr, instrs);
          ExitBlock(arena_, blocks, instrs);
        }
        error = error || (type == NULL);

        if (!error && binding->type != NULL && !FbleTypesEqual(nvd[i].type, type)) {
          error = true;
          ReportError(arena_, &binding->expr->loc,
              "expected type %t, but found %t\n",
              nvd[i].type, type);
        } else if (!error && binding->type == NULL) {
          VarType* var = var_types[i];
          var_type_values[i] = ValueOfType(arena, type);
          if (var_type_values[i] == NULL) {
            ReportError(arena_, &binding->expr->loc,
                "expected type, but found something of type %t\n",
                type);
            error = true;
          } else {
            Kind* expected_kind = var->kind;
            Kind* actual_kind = FbleGetKind(arena_, var_type_values[i]);
            if (!FbleKindsEqual(expected_kind, actual_kind)) {
              ReportError(arena_, &binding->expr->loc,
                  "expected kind %k, but found %k\n",
                  expected_kind, actual_kind);
              error = true;
            }
            FreeKind(arena_, actual_kind);
          }
        }

        FbleTypeRelease(arena, type);
      }

      // Check to see if this is a recursive let block.
      bool recursive = false;
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        recursive = recursive || (vars->vars.xs[vi + i].instrs.size > 0);
      }

      // Apply the newly computed type values for variables whose types were
      // previously unknown.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        if (var_type_values[i] != NULL) {
          VarType* var = var_types[i];
          assert(var != NULL);
          var->value = var_type_values[i];
          FbleRefAdd(arena, &var->_base.ref, &var->value->ref);
          FbleTypeRelease(arena, var->value);
        }
      }

      // Re-evaluate the variable types now that we have updated information
      // about the values of some of the types in question.
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        FbleEvalType(arena, nvd[i].type);
      }

      FbleLetDefInstr* let_def_instr = FbleAlloc(arena_, FbleLetDefInstr);
      let_def_instr->_base.tag = FBLE_LET_DEF_INSTR;
      let_def_instr->count = let_expr->bindings.size;
      let_def_instr->recursive = recursive;
      FbleVectorAppend(arena_, *instrs, &let_def_instr->_base);

      Type* rtype = NULL;
      if (!error) {
        rtype = CompileExpr(arena, blocks, exit, vars, let_expr->body, instrs);
        error = (rtype == NULL);
      }

      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        PopVar(arena_, vars);
        FbleTypeRelease(arena, nvd[i].type);
      }

      if (error) {
        FbleTypeRelease(arena, rtype);
        return NULL;
      }

      if (!exit) {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->count = let_expr->bindings.size;
        FbleVectorAppend(arena_, *instrs, &descope->_base);
      }
      return rtype;
    }

    case FBLE_MODULE_REF_EXPR: {
      AddBlockTime(blocks, 1);
      FbleModuleRefExpr* module_ref_expr = (FbleModuleRefExpr*)expr;

      for (size_t i = 0; i < vars->nvars; ++i) {
        size_t j = vars->nvars - i - 1;
        Var* var = vars->vars.xs + j;
        if (FbleNamesEqual(&module_ref_expr->ref.resolved, &var->name)) {
          FbleVarInstr* instr = FbleAlloc(arena_, FbleVarInstr);
          instr->_base.tag = FBLE_VAR_INSTR;
          instr->position = j;
          FbleVectorAppend(arena_, vars->vars.xs[j].instrs, instr);
          FbleVectorAppend(arena_, *instrs, &instr->_base);
          CompileExit(arena_, exit, instrs);
          return FbleTypeRetain(arena, var->type);
        }
      }

      // We should have resolved all modules at program load time, so we
      // should never get here.
      UNREACHABLE("module not in scope");
      return NULL;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* poly = (FblePolyExpr*)expr;

      VarType* arg = FbleAlloc(arena_, VarType);
      FbleRefInit(arena, &arg->_base.ref);
      arg->_base.tag = VAR_TYPE;
      arg->_base.loc = poly->arg.name.loc;
      arg->_base.evaluating = false;
      arg->name = poly->arg.name;
      arg->kind = CompileKind(arena_, poly->arg.kind);
      arg->value = NULL;

      TypeType* type_type = FbleAlloc(arena_, TypeType);
      FbleRefInit(arena, &type_type->_base.ref);
      type_type->_base.tag = TYPE_TYPE;
      type_type->_base.loc = poly->arg.name.loc;
      type_type->_base.evaluating = false;
      type_type->type = &arg->_base;
      FbleRefAdd(arena, &type_type->_base.ref, &arg->_base.ref);

      // TODO: It's a little silly that we are pushing an empty type value
      // here. Oh well. Maybe in the future we'll optimize those away or
      // add support for non-type poly args too.
      AddBlockTime(blocks, 1);
      FbleTypeInstr* type_instr = FbleAlloc(arena_, FbleTypeInstr);
      type_instr->_base.tag = FBLE_TYPE_INSTR;
      FbleVectorAppend(arena_, *instrs, &type_instr->_base);

      FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
      vpush->_base.tag = FBLE_VPUSH_INSTR;
      vpush->count = 1;
      FbleVectorAppend(arena_, *instrs, &vpush->_base);
      PushVar(arena_, vars, poly->arg.name, &type_type->_base);

      Type* body = CompileExpr(arena, blocks, exit, vars, poly->body, instrs);

      PopVar(arena_, vars);
      FbleTypeRelease(arena, &type_type->_base);

      if (!exit) {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->count = 1;
        FbleVectorAppend(arena_, *instrs, &descope->_base);
      }

      if (body == NULL) {
        FbleTypeRelease(arena, &arg->_base);
        return NULL;
      }

      Type* pt = FbleNewPolyType(arena, expr->loc, &arg->_base, body);
      FbleTypeRelease(arena, &arg->_base);
      FbleTypeRelease(arena, body);
      return pt;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;

      // Note: typeof(poly<arg>) = typeof(poly)<arg>
      // CompileExpr gives us typeof(poly)
      Type* poly = CompileExpr(arena, blocks, exit, vars, apply->poly, instrs);
      if (poly == NULL) {
        return NULL;
      }

      PolyKind* poly_kind = (PolyKind*)FbleGetKind(arena_, poly);
      if (poly_kind->_base.tag != POLY_KIND) {
        ReportError(arena_, &expr->loc,
            "cannot apply poly args to a basic kinded entity");
        FreeKind(arena_, &poly_kind->_base);
        FbleTypeRelease(arena, poly);
        return NULL;
      }

      // Note: CompileType gives us the value of arg
      Type* arg = CompileType(arena, vars, apply->arg);
      if (arg == NULL) {
        FreeKind(arena_, &poly_kind->_base);
        FbleTypeRelease(arena, poly);
        return NULL;
      }

      Kind* expected_kind = poly_kind->arg;
      Kind* actual_kind = FbleGetKind(arena_, arg);
      if (!FbleKindsEqual(expected_kind, actual_kind)) {
        ReportError(arena_, &apply->arg->loc,
            "expected kind %k, but found %k\n",
            expected_kind, actual_kind);
        FreeKind(arena_, &poly_kind->_base);
        FreeKind(arena_, actual_kind);
        FbleTypeRelease(arena, poly);
        FbleTypeRelease(arena, arg);
        return NULL;
      }
      FreeKind(arena_, actual_kind);
      FreeKind(arena_, &poly_kind->_base);

      Type* pat = FbleNewPolyApplyType(arena, expr->loc, poly, arg);
      FbleTypeRelease(arena, poly);
      FbleTypeRelease(arena, arg);
      FbleEvalType(arena, pat);
      return pat;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      assert(list_expr->args.size > 0);

      FbleTypeofExpr typeof_elem = {
        ._base = { .tag = FBLE_TYPEOF_EXPR, .loc = expr->loc },
        .expr = list_expr->args.xs[0],
      };
      return CompileList(arena, blocks, exit, vars, expr->loc, &typeof_elem._base, list_expr->args, instrs);
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal = (FbleLiteralExpr*)expr;

      // Check that the type compiles before desugaring the literal
      // expression. Otherwise we could end up giving a lot of duplicate error
      // messages.
      Type* type = CompileType(arena, vars, literal->type);
      if (type == NULL) {
        return NULL;
      }
      FbleTypeRelease(arena, type);

      FbleStructValueImplicitTypeExpr unit = {
        ._base = {
          .tag = FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,
          .loc = literal->word_loc,
        },
        .args = { .size = 0, .xs = NULL, },
      };

      size_t n = strlen(literal->word);

      FbleUnionValueExpr letters[n];
      FbleExpr* xs[n];
      char word_letters[2*n];
      FbleLoc loc = literal->word_loc;
      for (size_t i = 0; i < n; ++i) {
        letters[i]._base.tag = FBLE_UNION_VALUE_EXPR;
        letters[i]._base.loc = loc;
        letters[i].type = literal->type;
        word_letters[2*i] = literal->word[i];
        word_letters[2*i + 1] = '\0';
        letters[i].field.name = word_letters + 2*i;
        letters[i].field.space = FBLE_NORMAL_NAME_SPACE;
        letters[i].field.loc = loc;
        letters[i].arg = &unit._base;

        xs[i] = &letters[i]._base;

        if (literal->word[i] == '\n') {
          loc.line++;
          loc.col = 0;
        }
        loc.col++;
      }

      FbleExprV args = { .size = n, .xs = xs, };
      return CompileList(arena, blocks, exit, vars, literal->word_loc, literal->type, args, instrs);
    }

    case FBLE_STRUCT_IMPORT_EXPR: {
      AddBlockTime(blocks, 1);
      FbleStructImportExpr* struct_import_expr = (FbleStructImportExpr*)expr;

      Type* type = CompileExpr(arena, blocks, false, vars, struct_import_expr->nspace, instrs);
      if (type == NULL) {
        return NULL;
      }

      StructType* struct_type = (StructType*)FbleNormalType(type);
      if (struct_type->_base.tag != STRUCT_TYPE) {
        ReportError(arena_, &struct_import_expr->nspace->loc,
            "expected value of type struct, but found value of type %t\n",
            type);

        FbleTypeRelease(arena, type);
        return NULL;
      }

      AddBlockTime(blocks, 1 + struct_type->fields.size);

      FbleStructImportInstr* struct_import = FbleAlloc(arena_, FbleStructImportInstr);
      struct_import->_base.tag = FBLE_STRUCT_IMPORT_INSTR;
      struct_import->loc = struct_import_expr->nspace->loc;
      FbleVectorAppend(arena_, *instrs, &struct_import->_base);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        PushVar(arena_, vars, struct_type->fields.xs[i].name, struct_type->fields.xs[i].type);
      }

      Type* rtype = CompileExpr(arena, blocks, exit, vars, struct_import_expr->body, instrs);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        PopVar(arena_, vars);
      }

      if (!exit) {
        FbleDescopeInstr* descope = FbleAlloc(arena_, FbleDescopeInstr);
        descope->_base.tag = FBLE_DESCOPE_INSTR;
        descope->count = struct_type->fields.size;
        FbleVectorAppend(arena_, *instrs, &descope->_base);
      }

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
//   vars - the list of variables in scope.
//   loc - the location of the list expression.
//   type - the expected type of the list elements.
//   args - the elements of the list expression to compile.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Updates blocks with compiled block information.
//   Appends instructions to 'instrs' for executing the given expression.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the expression fails to compile.
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static Type* CompileList(FbleTypeArena* arena, Blocks* blocks, bool exit, Vars* vars, FbleLoc loc, FbleTypeExpr* type, FbleExprV args, FbleInstrV* instrs)
{
  // The goal is to desugar a list expression [a, b, c, d] into the
  // following expression:
  // <@ T@>(T@ x, T@ x1, T@ x2, T@ x3)<@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   cons(x, cons(x1, cons(x2, cons(x3, nil))));
  // }<t@>(a, b, c, d)
  //
  // But if there are zero args, we'll instead generate:
  // <@ T@><@ L@>((T@, L@){L@;} cons, L@ nil) {
  //   nil;
  // }<t@>
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleKind basic_kind = { .tag = FBLE_BASIC_KIND, .loc = loc, };

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
      .kind = &basic_kind,
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
      .kind = &basic_kind,
      .name = elem_type_name,
    },
    .body = (args.size == 0) ? &inner_poly._base : &outer_func._base,
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

  FbleExpr* expr = (args.size == 0) ? &apply_type._base : &apply_elems._base;

  Type* result = CompileExpr(arena, blocks, exit, vars, expr, instrs);

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
//   vars - the list of variables in scope.
//   expr - the expression to compile.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static Type* CompileExprNoInstrs(FbleTypeArena* arena, Vars* vars, FbleExpr* expr)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  // Make a copy of vars to ensure we don't save references to any
  // instructions that we may free.
  Vars nvars;
  FbleVectorInit(arena_, nvars.vars);
  nvars.nvars = 0;
  for (size_t i = 0; i < vars->nvars; ++i) {
    PushVar(arena_, &nvars, vars->vars.xs[i].name, vars->vars.xs[i].type);
  }

  FbleInstrV instrs;
  FbleVectorInit(arena_, instrs);

  Blocks blocks;
  FbleVectorInit(arena_, blocks.stack);
  FbleVectorInit(arena_, blocks.blocks);
  Type* type = CompileExpr(arena, &blocks, true, &nvars, expr, &instrs);
  FbleFree(arena_, blocks.stack.xs);
  FbleFreeBlockNames(arena_, &blocks.blocks);

  for (size_t i = 0; i < instrs.size; ++i) {
    FreeInstr(arena_, instrs.xs[i]);
  }
  FbleFree(arena_, instrs.xs);
  FreeVars(arena_, &nvars);
  return type;
}

// CompileType --
//   Compile a type, returning its value.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the value of variables in scope.
//   type - the type to compile.
//
// Results:
//   The compiled and evaluated type, or NULL in case of error.
//
// Side effects:
//   Prints a message to stderr if the type fails to compile or evalute.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static Type* CompileType(FbleTypeArena* arena, Vars* vars, FbleTypeExpr* type)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (type->tag) {
    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* struct_type = (FbleStructTypeExpr*)type;
      StructType* st = FbleAlloc(arena_, StructType);
      FbleRefInit(arena, &st->_base.ref);
      st->_base.tag = STRUCT_TYPE;
      st->_base.loc = type->loc;
      st->_base.evaluating = false;
      FbleVectorInit(arena_, st->fields);

      for (size_t i = 0; i < struct_type->fields.size; ++i) {
        FbleField* field = struct_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FbleTypeRelease(arena, &st->_base);
          return NULL;
        }

        if (!CheckNameSpace(arena, &field->name, compiled)) {
          FbleTypeRelease(arena, compiled);
          FbleTypeRelease(arena, &st->_base);
          return NULL;
        }

        Field* cfield = FbleVectorExtend(arena_, st->fields);
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
      UnionType* ut = FbleAlloc(arena_, UnionType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.tag = UNION_TYPE;
      ut->_base.loc = type->loc;
      ut->_base.evaluating = false;
      FbleVectorInit(arena_, ut->fields);

      FbleUnionTypeExpr* union_type = (FbleUnionTypeExpr*)type;
      for (size_t i = 0; i < union_type->fields.size; ++i) {
        FbleField* field = union_type->fields.xs + i;
        Type* compiled = CompileType(arena, vars, field->type);
        if (compiled == NULL) {
          FbleTypeRelease(arena, &ut->_base);
          return NULL;
        }
        Field* cfield = FbleVectorExtend(arena_, ut->fields);
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
      FuncType* ft = FbleAlloc(arena_, FuncType);
      FbleRefInit(arena, &ft->_base.ref);
      ft->_base.tag = FUNC_TYPE;
      ft->_base.loc = type->loc;
      ft->_base.evaluating = false;
      ft->arg = NULL;
      ft->rtype = NULL;

      FbleFuncTypeExpr* func_type = (FbleFuncTypeExpr*)type;
      ft->arg = CompileType(arena, vars, func_type->arg);
      if (ft->arg == NULL) {
        FbleTypeRelease(arena, &ft->_base);
        return NULL;
      }
      FbleRefAdd(arena, &ft->_base.ref, &ft->arg->ref);
      FbleTypeRelease(arena, ft->arg);

      Type* rtype = CompileType(arena, vars, func_type->rtype);
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
      ProcType* ut = FbleAlloc(arena_, ProcType);
      FbleRefInit(arena, &ut->_base.ref);
      ut->_base.loc = type->loc;
      ut->_base.evaluating = false;

      ut->_base.tag = PROC_TYPE;

      ut->type = NULL;

      FbleProcTypeExpr* unary_type = (FbleProcTypeExpr*)type;
      ut->type = CompileType(arena, vars, unary_type->type);
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
      return CompileExprNoInstrs(arena, vars, typeof->expr);
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
      Type* type = CompileExprNoInstrs(arena, vars, expr);
      if (type == NULL) {
        return NULL;
      }

      Type* type_value = ValueOfType(arena, type);
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

// CompileKind --
//   Compile a kind.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the kind to compile.
//
// Results:
//   The compiled kind.
//
// Side effects:
//   Allocates a reference-counted kind that must be freed using FreeKind
//   when it is no longer needed.
static Kind* CompileKind(FbleArena* arena, FbleKind* kind)
{
  switch (kind->tag) {
    case FBLE_BASIC_KIND: {
      FbleBasicKind* basic = (FbleBasicKind*)kind;
      BasicKind* k = FbleAlloc(arena, BasicKind);
      k->_base.tag = BASIC_KIND;
      k->_base.loc = basic->_base.loc;
      k->_base.refcount = 1;
      k->level = 1;
      return &k->_base;
    }

    case FBLE_POLY_KIND: {
      FblePolyKind* poly = (FblePolyKind*)kind;
      PolyKind* k = FbleAlloc(arena, PolyKind);
      k->_base.tag = POLY_KIND;
      k->_base.loc = poly->_base.loc;
      k->_base.refcount = 1;
      k->arg = CompileKind(arena, poly->arg);
      k->rkind = CompileKind(arena, poly->rkind);
      return &k->_base;
    }
  }

  UNREACHABLE("Should never get here");
}

// CompileProgram --
//   Type check and compile the given program. Returns the type of the
//   program and generates instructions to compute the value of that
//   program at runtime.
//
// Inputs:
//   arena - arena to use for allocations.
//   blocks - the blocks stack.
//   vars - the list of variables in scope.
//   prgm - the program to compile.
//   instrs - vector of instructions to append new instructions to.
//
// Results:
//   The type of the program, or NULL if the program is not well typed.
//
// Side effects:
//   Updates 'blocks' with compiled block information. Exits the current
//   block.
//   Appends instructions to 'instrs' for executing the given program.
//   There is no gaurentee about what instructions have been appended to
//   'instrs' if the program fails to compile.
//   Prints a message to stderr if the program fails to compile.
//   Allocates a reference-counted type that must be freed using
//   FbleTypeRelease when it is no longer needed.
static Type* CompileProgram(FbleTypeArena* arena, Blocks* blocks, Vars* vars, FbleProgram* prgm, FbleInstrV* instrs)
{
  AddBlockTime(blocks, 1 + prgm->modules.size);

  FbleArena* arena_ = FbleRefArenaArena(arena);
  Type* types[prgm->modules.size];
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    EnterBlock(arena_, blocks, prgm->modules.xs[i].name, prgm->modules.xs[i].value->loc, instrs);
    types[i] = CompileExpr(arena, blocks, false, vars, prgm->modules.xs[i].value, instrs);
    ExitBlock(arena_, blocks, instrs);

    if (types[i] == NULL) {
      for (size_t j = 0; j < i; ++j) {
        FbleTypeRelease(arena, types[j]);
      }
      return NULL;
    }

    FbleVPushInstr* vpush = FbleAlloc(arena_, FbleVPushInstr);
    vpush->_base.tag = FBLE_VPUSH_INSTR;
    vpush->count = 1;
    FbleVectorAppend(arena_, *instrs, &vpush->_base);
    PushVar(arena_, vars, prgm->modules.xs[i].name, types[i]);
  }

  Type* rtype = CompileExpr(arena, blocks, true, vars, prgm->main, instrs);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    PopVar(arena_, vars);
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

  FbleInstrBlock* block = NewInstrBlock(arena);

  FbleName entry_name = {
    .name = "",
    .loc = program->main->loc,
    .space = FBLE_NORMAL_NAME_SPACE
  };

  Vars vars;
  FbleVectorInit(arena, vars.vars);
  vars.nvars = 0;

  FbleTypeArena* type_arena = FbleNewTypeArena(arena);
  EnterBlock(arena, &block_stack, entry_name, program->main->loc, &block->instrs);
  Type* type = CompileProgram(type_arena, &block_stack, &vars, program, &block->instrs);
  ExitBlock(arena, &block_stack, NULL);
  FbleTypeRelease(type_arena, type);
  FbleFreeTypeArena(type_arena);

  assert(block_stack.stack.size == 0);
  FbleFree(arena, block_stack.stack.xs);
  *blocks = block_stack.blocks;

  FreeVars(arena, &vars);
  if (type == NULL) {
    FbleFreeInstrBlock(arena, block);
    return NULL;
  }
  return block;
}
