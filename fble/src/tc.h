// tc.h --
//   Header file defining the FbleTc type, which is used as the implementation
//   for FbleValue.
//   This is an internal library interface.

#ifndef FBLE_INTERNAL_TC_H_
#define FBLE_INTERNAL_TC_H_

#include "expr.h"       // for FbleDataTypeTag
#include "fble-alloc.h"
#include "fble-loc.h"
#include "fble-name.h"

// FbleTc --
//   An already type-checked representation of an fble syntactic expression.
//
// FbleTc is like FbleExpr, except that:
// * Field and variable names are replaced with integer indicies.
// * Types are eliminated.
// * Processes are treated as zero argument functions.
// * There is no difference between a function context and a process context.
//   In particular, LINK_TC and EXEC_TC represent the computation that returns
//   the result of running the link and exec processes, rather than a
//   computation that creates link and exec process values.
//
typedef struct FbleTc FbleTc;

// FbleTcV --
//   A vector of FbleTc.
typedef struct {
  size_t size;
  FbleTc** xs;
} FbleTcV;

// FbleTcTag --
//   A tag used to distinguish among different kinds of FbleTc.
typedef enum {
  FBLE_TYPE_VALUE_TC,
  FBLE_VAR_TC,
  FBLE_LET_TC,
  FBLE_STRUCT_VALUE_TC,
  FBLE_UNION_VALUE_TC,
  FBLE_UNION_SELECT_TC,
  FBLE_DATA_ACCESS_TC,
  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,
  FBLE_LINK_TC,
  FBLE_EXEC_TC,
  FBLE_PROFILE_TC,
} FbleTcTag;

// FbleTc --
//   A tagged union of tc types. All tcs have the same initial
//   layout as FbleTc. The tag can be used to determine what kind of
//   tc this is to get access to additional fields of the value
//   by first casting to that specific type of tc.
struct FbleTc {
  FbleTcTag tag;
};

// FbleTypeValueTc --
//   FBLE_TYPE_VALUE_TC
//
// An expression to compute the type value.
typedef struct {
  FbleTc _base;
} FbleTypeValueTc;

// FbleVarSource
//   Where to find a variable.
//
// FBLE_LOCAL_VAR is a local variable.
// FBLE_STATIC_VAR is a variable captured from the parent scope.
typedef enum {
  FBLE_LOCAL_VAR,
  FBLE_STATIC_VAR,
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

// FbleVarTc --
//   FBLE_VAR_TC
//
// A variable expression.
// * Used to represent variables refering to function arguments or local
//   variables. 
typedef struct {
  FbleTc _base;
  FbleVarIndex index;
} FbleVarTc;

// FbleLocTc --
//   A value bundled together with a location.
//
// Note: this is not an FbleTc subtype.
typedef struct {
  FbleLoc loc;
  FbleTc* tc;
} FbleLocTc;

// FbleLocTcV --
//   A vector of FbleLocTc.
typedef struct {
  size_t size;
  FbleLocTc* xs;
} FbleLocTcV;

// FbleLetTc --
//   FBLE_LET_TC
//
// Represents a let expression.
//
// The bindings are bound to variables implicitly based on the position of the
// binding in the let expression and the position of the let expression in its
// parent expression as specified for FbleVarIndex.
//
// Fields:
//   recursive - false if the let is a non-recursive let expression.
//   bindings - the values of the variable being defined, with the location of
//              the variable being defined.
typedef struct {
  FbleTc _base;
  bool recursive;
  FbleLocTcV bindings;
  FbleTc* body;
} FbleLetTc;

// FbleStructValueTc --
//   FBLE_STRUCT_VALUE_TC
//
// Represents a struct value expression.
typedef struct {
  FbleTc _base;
  size_t fieldc;
  FbleTc* fields[];
} FbleStructValueTc;

// FbleUnionValueTc --
//   FBLE_UNION_VALUE_TC
//
// Represents a union value expression.
typedef struct {
  FbleTc _base;
  size_t tag;
  FbleTc* arg;
} FbleUnionValueTc;

// FbleUnionSelectTc --
//   FBLE_UNION_SELECT_TC
//
// Represents a union select expression.
//
// Because of default branches in union select, it is possible that multiple
// choices point to the same value. Code generation is expected to check for
// that and avoid generating duplicate code.
typedef struct {
  FbleTc _base;
  FbleLoc loc;
  FbleTc* condition;
  size_t choicec;
  FbleTc* choices[];
} FbleUnionSelectTc;

// FbleDataAccessTc --
//   FBLE_DATA_ACCESS_TC
//
// Used to represent struct and union access expressions.
typedef struct {
  FbleTc _base;
  FbleDataTypeTag datatype;
  FbleTc* obj;
  size_t tag;
  FbleLoc loc;
} FbleDataAccessTc;

// FbleFuncValueTc --
//   FBLE_FUNC_VALUE_TC
//
// Represents a function or process value.
typedef struct {
  FbleTc _base;
  FbleLoc body_loc;
  FbleVarIndexV scope;
  size_t argc;
  FbleTc* body;
} FbleFuncValueTc;

// FbleFuncApplyTc --
//   FBLE_FUNC_APPLY_TC
//
// Represents a function application expression.
typedef struct {
  FbleTc _base;
  FbleLoc loc;
  FbleTc* func;
  FbleTcV args;
} FbleFuncApplyTc;

// FbleLinkTc --
//   FBLE_LINK_TC
//
// Represents a process link expression. Unlike FBLE_LINK_EXPR, which
// evaluates to a proc value, FBLE_LINK_TC evaluates to the result of
// computing the proc value.
typedef struct {
  FbleTc _base;
  FbleTc* body;
} FbleLinkTc;

// FbleExecTc --
//   FBLE_EXEC_TC
//
// Represents a process exec expression. Unlike FBLE_EXEC_EXPR, which
// evaluates to a proc value, FBLE_EXEC_TC evaluates to the result of
// computing the proc value.
typedef struct {
  FbleTc _base;
  FbleTcV bindings;
  FbleTc* body;
} FbleExecTc;

// FbleProfileTc --
//   FBLE_PROFILE_TC
//
// FbleProfileTc is used to denote a profiling block.
//
// Fields:
//   name - the name of the block for profiling purposes.
//   loc - the location of the profile block.
//   body - the body of the profile block.
//
// The location of the profiling block is passed through loc, not name.loc.
typedef struct {
  FbleTc _base;
  FbleName name;
  FbleLoc loc;
  FbleTc* body;
} FbleProfileTc;

// FbleFreeTc --
//   Free resources associated with an FbleTc.
//
// Inputs:
//   tc - the tc to free. May be NULL.
//
// Side effects:
//   Frees all resources associated with the given tc.
void FbleFreeTc(FbleTc* tc);

#endif // FBLE_INTERNAL_TC_H_
