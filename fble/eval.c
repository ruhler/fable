// eval.c --
//   This file implements the fble evaluation routines.

#include "fble.h"

#define UNREACHABLE(x) assert(false && x)

static FbleValue gTypeTypeValue = {
  .tag = FBLE_TYPE_TYPE_VALUE,
  .refcount = 1,
  .type = &gTypeTypeValue
};

// Vars --
//   Scope of variables visible during type checking.
typedef struct Vars {
  FbleName name;
  FbleValue* type;
  struct Vars* next;
} Vars;

// InstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  TYPE_TYPE_INSTR,
  VAR_INSTR,
} InstrTag;

// Instr --
//   Common base type for all instructions.
typedef struct {
  InstrTag tag;
} Instr;

// TypeTypeInstr -- TYPE_TYPE_INSTR
//   Outputs a TYPE_TYPE_VALUE.
typedef struct {
  Instr _base;
} TypeTypeInstr;

// VarInstr -- VAR_INSTR
//   Reads the variable at the given position in the stack. Position 0 is the
//   top of the stack.
typedef struct {
  Instr _base;
  int position;
} VarInstr;

// ResultStack --
// 
// Fields:
//   result - Pointer to where the result of evaluating an expression should
//            be stored.
//   next_pc - The next instruction to execute after returning a result.
typedef struct ResultStack {
  FbleValue** result;
  Instr* next_pc;
  struct ResultStack* tail;
} ResultStack;

static FbleValue* Compile(FbleArena* arena, Vars* vars, FbleExpr* expr, Instr** instrs);
static FbleValue* Eval(FbleArena* arena, Instr* instrs);
static void FreeInstrs(FbleArena* arena, Instr* instrs);


// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   vars - the list of variables in scope.
//   expr - the expression to compile.
//   instrs - output pointer to store generated in structions
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile.
static FbleValue* Compile(FbleArena* arena, Vars* vars, FbleExpr* expr, Instr** instrs)
{
  switch (expr->tag) {
    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;

      int position = 0;
      while (vars != NULL && !FbleNamesEqual(var_expr->var.name, vars->name.name)) {
        vars = vars->next;
        position++;
      }

      if (vars == NULL) {
        FbleReportError("variable '%s' not defined\n", &var_expr->var.loc, var_expr->var.name);
        return NULL;
      }

      VarInstr* instr = FbleAlloc(arena, VarInstr);
      instr->_base.tag = VAR_INSTR;
      instr->position = position;
      *instrs = &instr->_base;
      return vars->type;
    }

    case FBLE_LET_EXPR: assert(false && "TODO: FBLE_LET_EXPR"); return NULL;

    case FBLE_TYPE_TYPE_EXPR: {
      TypeTypeInstr* instr = FbleAlloc(arena, TypeTypeInstr);
      instr->_base.tag = TYPE_TYPE_INSTR;
      *instrs = &instr->_base;

      return FbleCopy(arena, &gTypeTypeValue);
    }

    case FBLE_FUNC_TYPE_EXPR: assert(false && "TODO: FBLE_FUNC_TYPE_EXPR"); return NULL;
    case FBLE_FUNC_VALUE_EXPR: assert(false && "TODO: FBLE_FUNC_VALUE_EXPR"); return NULL;
    case FBLE_FUNC_APPLY_EXPR: assert(false && "TODO: FBLE_FUNC_APPLY_EXPR"); return NULL;

    case FBLE_STRUCT_TYPE_EXPR: assert(false && "TODO: FBLE_STRUCT_TYPE_EXPR"); return NULL;
    case FBLE_STRUCT_VALUE_EXPR: assert(false && "TODO: FBLE_STRUCT_VALUE_EXPR"); return NULL;
    case FBLE_STRUCT_ACCESS_EXPR: assert(false && "TODO: FBLE_STRUCT_ACCESS_EXPR"); return NULL;

    case FBLE_UNION_TYPE_EXPR: assert(false && "TODO: FBLE_UNION_TYPE_EXPR"); return NULL;
    case FBLE_UNION_VALUE_EXPR: assert(false && "TODO: FBLE_UNION_VALUE_EXPR"); return NULL;
    case FBLE_UNION_ACCESS_EXPR: assert(false && "TODO: FBLE_UNION_ACCESS_EXPR"); return NULL;
    case FBLE_COND_EXPR: assert(false && "TODO: FBLE_COND_EXPR"); return NULL;

    case FBLE_PROC_TYPE_EXPR: assert(false && "TODO: FBLE_PROC_TYPE_EXPR"); return NULL;
    case FBLE_INPUT_TYPE_EXPR: assert(false && "TODO: FBLE_INPUT_TYPE_EXPR"); return NULL;
    case FBLE_OUTPUT_TYPE_EXPR: assert(false && "TODO: FBLE_OUTPUT_TYPE_EXPR"); return NULL;
    case FBLE_EVAL_EXPR: assert(false && "TODO: FBLE_EVAL_EXPR"); return NULL;
    case FBLE_GET_EXPR: assert(false && "TODO: FBLE_GET_EXPR"); return NULL;
    case FBLE_PUT_EXPR: assert(false && "TODO: FBLE_PUT_EXPR"); return NULL;
    case FBLE_LINK_EXPR: assert(false && "TODO: FBLE_LINK_EXPR"); return NULL;
    case FBLE_EXEC_EXPR: assert(false && "TODO: FBLE_EXEC_EXPR"); return NULL;

    case FBLE_ACCESS_EXPR: assert(false && "TODO: FBLE_ACCESS_EXPR"); return NULL;
    case FBLE_APPLY_EXPR: assert(false && "TODO: FBLE_APPLY_EXPR"); return NULL;

    default:
      UNREACHABLE("invalid expression tag");
      return NULL;
  }

  UNREACHABLE("should already have returned");
  return NULL;
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   arena - the arena to use for allocations.
//   instrs - the instructions to evaluate.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   Prints a message to stderr in case of error.
static FbleValue* Eval(FbleArena* arena, Instr* prgm)
{
  Instr* pc = prgm;
  FbleValue* result = NULL;
  ResultStack* rstack = NULL;

  while (result == NULL || rstack != NULL) {
    if (result != NULL) {
      *(rstack->result) = result;
      pc = rstack->next_pc;
      rstack = rstack->tail;
    }

    switch (pc->tag) {
      case TYPE_TYPE_INSTR: {
        result = FbleCopy(arena, &gTypeTypeValue);
        break;
      }

      default:
        UNREACHABLE("invalid instruction");
        return NULL;
    }
  }
  return result;
}

// FreeInstrs --
//   Free the given sequence of instructions.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instrs - the instructions to free.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the instrs.
static void FreeInstrs(FbleArena* arena, Instr* instrs)
{
  switch (instrs->tag) {
    case TYPE_TYPE_INSTR:
    case VAR_INSTR:
      FbleFree(arena, instrs);
      return;
  }
  UNREACHABLE("invalid instruction");
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  Instr* instrs = NULL;
  FbleValue* type = Compile(arena, NULL, expr, &instrs);
  if (type == NULL) {
    return NULL;
  }
  FbleRelease(arena, type);

  FbleValue* result = Eval(arena, instrs);
  FreeInstrs(arena, instrs);
  return result;
}

