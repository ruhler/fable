// typecheck.h --
//   Header file for fble type checking.

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include "fble-alloc.h"
#include "fble-value.h"
#include "syntax.h"
#include "type.h"

// FbleTcTag --
//   A tag used to distinguish among different kinds of type checked
//   expressions.
//
// FbleTc is like FbleExpr, except that:
// * Field and variable names are replaced with integer indicies.
// * Types are eliminated.
// * Processes are treated as zero argument functions.
// * There is no difference between a function context and a process context.
//   In particular, LINK_TC and EXEC_TC represent the computation that returns
//   the result of running the link and exec processes, rather than a
//   computation that creates link and exec process values.
typedef enum {
  FBLE_LET_TC,
  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,
} FbleTcTag;

// FbleTc --
//   A tagged union of type checked expression types. All tcs have the same
//   initial layout as FbleTc. The tag can be used to determine what kind of
//   tc this is to get access to additional fields of the tc by first
//   casting to that specific type of tc.
typedef struct {
  FbleTcTag tag;
} FbleTc;

// FbleVarSource
//   Where to find a variable.
//
// FBLE_LOCAL_VAR is a local variable.
// FBLE_STATIC_VAR is a variable captured from the parent scope.
// FBLE_FREE_VAR is a unbound var introduced for symbolic elaboration.
typedef enum {
  FBLE_LOCAL_VAR,
  FBLE_STATIC_VAR,
  FBLE_FREE_VAR,
} FbleVarSource;

// FbleVarIndex --
//   Identifies a variable in scope.
//
// For local variables, index starts at 0 for the first argument to a
// function. The index increases by one for each new variable introduced,
// going from left to right, outer most to inner most binding.
typedef struct {
  FbleVarSource source;
  size_t index;
} FbleVarIndex;

// FbleVarIndexV --
//   A vector of FbleVarIndex.
typedef struct {
  size_t size;
  FbleVarIndex* xs;
} FbleVarIndexV;

// FbleLetTc --
//   FBLE_LET_TC
//
// recursive is false if the let is a non-recursive let expression.
typedef struct {
  FbleTc _base;
  bool recursive;
  FbleValueV bindings;
  FbleValue* body;
} FbleLetTc;

// FbleFuncValueTc --
//   FBLE_FUNC_VALUE_TC
//
// Note: FuncValueTc is used for process values as well as function values.
typedef struct {
  FbleTc _base;
  FbleLoc body_loc;
  FbleVarIndexV scope;
  size_t argc;
  FbleValue* body;
} FbleFuncValueTc;

// FbleFuncApplyTc --
//   FBLE_FUNC_APPLY_TC
typedef struct {
  FbleTc _base;
  FbleLoc loc;
  FbleValue* func;
  FbleValueV args;
} FbleFuncApplyTc;

// FbleNewTcValue --
//   Create a new FbleTcValue.
//
// Inputs:
//   heap - to use for allocations.
//   tc - the tc to turn into a value. Consumed.
//
// Results:
//   A new FbleValue wrapping the tc.
//
// Side effects:
//   Caller should call FbleReleaseValue when the value is no longer needed.
//
// TODO: Remove this once we fully replace FbleTc with FbleValue.
FbleValue* FbleNewTcValue(FbleValueHeap* heap, FbleTc* tc);

// FbleTypeCheck -- 
//   Run typecheck on the given program.
//
// Inputs:
//   arena - arena to use for allocations
//   program - the program to typecheck
//
// Result:
//   The type checked program, or NULL if the program failed to type check.
//
// Side effects:
// * Prints messages to stderr in case of failure to type check.
FbleValue* FbleTypeCheck(FbleValueHeap* heap, struct FbleProgram* program);

// FbleFreeTc --
//   Free resources assocatied with an FbleTc.
// 
// Inputs:
//   arena - arena to use for allocations
//   tc - the type checked expression to free
//
// Side effects:
//   Frees resources associated with the given expression.
void FbleFreeTc(FbleValueHeap* heap, FbleTc* tc);

#endif // FBLE_INTERNAL_TYPECHECK_H_
