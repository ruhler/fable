// fble-internal.h --
//   This header file describes the internal interface used in the
//   implementation of the fble library.

#ifndef FBLE_INTERNAL_H_
#define FBLE_INTERNAL_H_

#include "fble.h"

// FbleInstrTag --
//   Enum used to distinguish among different kinds of instructions.
typedef enum {
  FBLE_STRUCT_VALUE_INSTR,
  FBLE_STRUCT_ACCESS_INSTR,
  FBLE_UNION_VALUE_INSTR,
  FBLE_UNION_ACCESS_INSTR,
  FBLE_COND_INSTR,
  FBLE_FUNC_VALUE_INSTR,
  FBLE_FUNC_APPLY_INSTR,
  FBLE_VAR_INSTR,
  FBLE_LET_INSTR,
  FBLE_PUSH_INSTR,
  FBLE_POP_INSTR,
} FbleInstrTag;

// FbleInstr --
//   Common base type for all instructions.
typedef struct {
  FbleInstrTag tag;
} FbleInstr;

// FbleInstrV --
//   A vector of FbleInstr.
typedef struct {
  size_t size;
  FbleInstr** xs;
} FbleInstrV;

// FbleStructValueInstr -- FBLE_STRUCT_VALUE_INSTR
//   Allocate a struct value, then execute each of its arguments.
typedef struct {
  FbleInstr _base;
  FbleInstrV fields;
} FbleStructValueInstr;

// FbleUnionValueInstr -- FBLE_UNION_VALUE_INSTR
//   Allocate a union value, then execute its argument.
typedef struct {
  FbleInstr _base;
  size_t tag;
  FbleInstr* mkarg;
} FbleUnionValueInstr;

// FbleAccessInstr -- FBLE_STRUCT_ACCESS_INSTR or FBLE_UNION_ACCESS_INSTR
//   Access the tagged field from the object on top of the vstack.
typedef struct {
  FbleInstr _base;
  FbleLoc loc;
  size_t tag;
} FbleAccessInstr;

// FbleCondInstr -- FBLE_COND_INSTR
//   Select the next thing to execute based on the tag of the value on top of
//   the value stack.
typedef struct {
  FbleInstr _base;
  FbleInstrV choices;
} FbleCondInstr;

// FbleFuncValueInstr -- FBLE_FUNC_VALUE_INSTR
//   Allocate a function, capturing the current variable context in the
//   process.
typedef struct {
  FbleInstr _base;
  size_t argc;
  FbleInstr* body;
} FbleFuncValueInstr;

// FbleFuncApplyInstr -- FBLE_FUNC_APPLY_INSTR
//   Given f, x1, x2, ... on the top of the value stack,
//   apply f(x1, x2, ...).
// From the top of the stack down, we should find the arguments in reverse
// order and then the function value.
typedef struct {
  FbleInstr _base;
  size_t argc;
} FbleFuncApplyInstr;

// FbleVarInstr -- FBLE_VAR_INSTR
//   Reads the variable at the given position in the stack. Position 0 is the
//   top of the stack.
typedef struct {
  FbleInstr _base;
  size_t position;
} FbleVarInstr;

// FblePopInstr -- FBLE_POP_INSTR
//   Pop count values from the value stack.
typedef struct {
  FbleInstr _base;
  size_t count;
} FblePopInstr;

// FbleLetInstr -- FBLE_LET_INSTR
//   Evaluate each of the bindings, add the results to the scope, then execute
//   the body.
typedef struct {
  FbleInstr _base;
  FbleInstrV bindings;
  FbleInstr* body;
  FblePopInstr pop;
} FbleLetInstr;

// FblePushInstr -- FBLE_PUSH_INSTR
//   Evaluate and push the given values on top of the value stack and execute
//   the following instruction.
typedef struct {
  FbleInstr _base;
  FbleInstrV values;
  FbleInstr* next;
} FblePushInstr;

// FbleFreeInstrs --
//   Free the given sequence of instructions.
//
// Inputs:
//   arena - the arena used to allocation the instructions.
//   instrs - the instructions to free. May be NULL.
//
// Result:
//   none.
//
// Side effect:
//   Frees memory allocated for the instrs.
void FbleFreeInstrs(FbleArena* arena, FbleInstr* instrs);

// FbleCompile --
//   Type check and compile the given expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   expr - the expression to compile.
//
// Results:
//   The compiled expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression fails to compile. Allocates
//   memory for the instructions which must be freed with FbleFreeInstrs when
//   it is no longer needed.
FbleInstr* FbleCompile(FbleArena* arena, FbleExpr* expr);

// FbleFreeFuncValue --
//   Free memory associated with the given function value.
//
// Inputs: 
//   arena - the arena used for allocation.
//   value - the value to free.
//
// Results:
//   none.
//
// Side effects:
//   Frees all memory associated with the function value, including value
//   itself.
void FbleFreeFuncValue(FbleArena* arena, FbleFuncValue* value);


#endif // FBLE_INTERNAL_H_
