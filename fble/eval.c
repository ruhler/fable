// eval.c --
//   This file implements the fble evaluation routines.

#include <stdio.h>    // for fprintf, stderr

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
  LET_INSTR,
} InstrTag;

// Instr --
//   Common base type for all instructions.
typedef struct {
  InstrTag tag;
} Instr;

// InstrV --
//   A vector of Instr.
typedef struct {
  size_t size;
  Instr** xs;
} InstrV;

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

// LetInstr -- LET_INSTR
//   Evaluate each of the bindings, add the results to the scope, then execute
//   the body.
typedef struct {
  Instr _base;
  InstrV bindings;
  Instr* body;
} LetInstr;

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

static bool TypesEqual(FbleValue* a, FbleValue* b);
static void PrintType(FbleValue* type);
static FbleValue* Compile(FbleArena* arena, Vars* vars, FbleExpr* expr, Instr** instrs);
static FbleValue* Eval(FbleArena* arena, Instr* instrs);
static void FreeInstrs(FbleArena* arena, Instr* instrs);


// TypesEqual --
//   Test whether the two given types are equal.
//
// Inputs:
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(FbleValue* a, FbleValue* b)
{
  assert(false && "TODO: TypesEqual");
  return false;
}

// PrintType --
//   Print the given type in human readable form to stderr.
//
// Inputs:
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
static void PrintType(FbleValue* type)
{
  switch (type->tag) {
    case FBLE_TYPE_TYPE_VALUE: fprintf(stderr, "@"); return;
    case FBLE_FUNC_TYPE_VALUE: assert(false && "TODO FUNC_TYPE"); return;
    case FBLE_FUNC_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_STRUCT_TYPE_VALUE: assert(false && "TODO STRUCT_TYPE"); return;
    case FBLE_STRUCT_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_UNION_TYPE_VALUE: assert(false && "TODO UNION_TYPE"); return;
    case FBLE_UNION_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_PROC_TYPE_VALUE: assert(false && "TODO PROC_TYPE"); return;
    case FBLE_INPUT_TYPE_VALUE: assert(false && "TODO INPUT_TYPE"); return;
    case FBLE_OUTPUT_TYPE_VALUE: assert(false && "TODO OUTPUT_TYPE"); return;
    case FBLE_PROC_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_INPUT_VALUE: UNREACHABLE("not a type"); return;
    case FBLE_OUTPUT_VALUE: UNREACHABLE("not a type"); return;
  }
}

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

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;

      // Step 1: Compile, check, and evaluate the types of all the variables.
      FbleValue* types[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Instr* prgm;
        FbleType* type_expr = let_expr->bindings.xs[i].type;
        FbleValue* type_type = Compile(arena, vars, type_expr, &prgm);
        if (type_type == NULL) {
          return NULL;
        }
        
        if (type_type->tag != FBLE_TYPE_TYPE_VALUE) {
          FbleReportError("expected a type, found something else\n", &type_expr->loc);
          return NULL;
        }

        // TODO: Pass in the current scope, otherwise this is sure to fail.
        types[i] = Eval(arena, prgm);
        if (types[i] == NULL) {
          FbleReportError("failed to evaluate type\n", &type_expr->loc);
          return NULL;
        }
      }

      // Step 2: Add the new variables to the scope.
      assert(let_expr->bindings.size > 0);
      Vars nvars[let_expr->bindings.size];
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        nvars[i].name = let_expr->bindings.xs[i].name;
        nvars[i].type = types[i];
        nvars[i].next = vars;
        vars = nvars + i;
      }

      // Step 3: Compile, check, and if appropriate, evaluate the values of
      // the variables.
      LetInstr* instr = FbleAlloc(arena, LetInstr);
      instr->_base.tag = LET_INSTR;
      FbleVectorInit(arena, instr->bindings);
      for (size_t i = 0; i < let_expr->bindings.size; ++i) {
        Instr** prgm = FbleVectorExtend(arena, instr->bindings);
        FbleValue* type = Compile(arena, vars, let_expr->bindings.xs[i].expr, prgm);
        if (type == NULL) {
          return NULL;
        }

        if (!TypesEqual(types[i], type)) {
          FbleReportError("expected type ", &let_expr->bindings.xs[i].expr->loc);
          PrintType(types[i]);
          fprintf(stderr, ", but found ");
          PrintType(type);
          fprintf(stderr, "\n");
          return NULL;
        }

        // TODO: Evaluate the variable if it has a kinded type.
      }

      *instrs = &instr->_base;
      return Compile(arena, vars, let_expr->body, &(instr->body));
    }

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
    case VAR_INSTR: {
      FbleFree(arena, instrs);
      return;
    }

    case LET_INSTR: {
      LetInstr* let_instr = (LetInstr*)instrs;
      for (size_t i = 0; i < let_instr->bindings.size; ++i) {
        FreeInstrs(arena, let_instr->bindings.xs[i]);
      }
      FbleFree(arena, let_instr->bindings.xs);
      FreeInstrs(arena, let_instr->body);
      FbleFree(arena, let_instr);
      return;
    }
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

