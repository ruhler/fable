// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdbool.h>  // for false
#include <stdlib.h>   // for NULL

#include "fble.h"

#define UNREACHABLE(x) assert(false && x)

// InstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  TYPE_TYPE_INSTR,
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

// Result --
//   A single frame of the result stack.
typedef struct {
  FbleValue** result;
  Instr* next;
} Result;

// ResultV --
//   A vector of Result.
typedef struct {
  size_t size;
  Result* xs;
} ResultV;

static FbleValue* Compile(FbleArena* arena, FbleExpr* expr, Instr** instrs);
static FbleValue* Eval(FbleArena* arena, Instr* instrs);
static void FreeInstrs(FbleArena* arena, Instr* instrs);


// Compile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   expr - the expression to compile.
//   instrs - output pointer to store generated in structions
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile.
static FbleValue* Compile(FbleArena* arena, FbleExpr* expr, Instr** instrs)
{
  switch (expr->tag) {
    case FBLE_VAR_EXPR: assert(false && "TODO: FBLE_VAR_EXPR"); return NULL;
    case FBLE_LET_EXPR: assert(false && "TODO: FBLE_LET_EXPR"); return NULL;

    case FBLE_TYPE_TYPE_EXPR: {
      TypeTypeInstr* instr = FbleAlloc(arena, TypeTypeInstr);
      instr->_base.tag = TYPE_TYPE_INSTR;
      *instrs = &instr->_base;

      FbleTypeTypeValue* type = FbleAlloc(arena, FbleTypeTypeValue);
      type->_base.tag = FBLE_TYPE_TYPE_VALUE;
      type->_base.refcount = 1;
      type->_base.type = &type->_base;
      return &type->_base;
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
static FbleValue* Eval(FbleArena* arena, Instr* instrs)
{
//  FbleValue* result;
//
//  ResultV results;
//  FbleVectorInit(arena, results);
//  Result* r = FbleVectorExtend(arena, results);
//  r->result = &result;
//  r->next = NULL;
//
//  while (true) {
//    if (instrs == NULL) {
//      if (results
//    }
//  }
  UNREACHABLE("should never get here");
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
  assert(false && "TODO");
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleArena* arena, FbleExpr* expr)
{
  Instr* instrs = NULL;
  FbleValue* type = Compile(arena, expr, &instrs);
  if (type == NULL) {
    return NULL;
  }
  FbleRelease(arena, type);

  FbleValue* result = Eval(arena, instrs);
  FreeInstrs(arena, instrs);
  return result;
}

