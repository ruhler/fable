// tc.h --
//   Header file defining the FbleTc type.
//   This is an internal library interface.

#ifndef FBLE_INTERNAL_TC_H_
#define FBLE_INTERNAL_TC_H_

#include "kind.h"       // for FbleDataTypeTag
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
  FBLE_LIST_TC,
  FBLE_LITERAL_TC,
} FbleTcTag;

// FbleTc --
//   A tagged union of tc types. All tcs have the same initial
//   layout as FbleTc. The tag can be used to determine what kind of
//   tc this is to get access to additional fields of the value
//   by first casting to that specific type of tc.
//
// loc is the location of the start of the expression in source code, used for
// general purpose debug information.
struct FbleTc {
  FbleTcTag tag;
  FbleLoc loc;
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

// FbleLetTcBinding --
//   Information for a binding used in FbleLetTc.
//
// var - The name of the variable, for profile and debugging purposes.
// profile_loc - The location of the profile block to use for the binding.
// tc - The value of the binding.
typedef struct {
  FbleName var;
  FbleLoc profile_loc;
  FbleTc* tc;
} FbleLetTcBinding;

// FbleLetTcBindingV --
//   A vector of FbleLetTcBinding.
typedef struct {
  size_t size;
  FbleLetTcBinding* xs;
} FbleLetTcBindingV;

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
//   bindings - the variables being defined.
typedef struct {
  FbleTc _base;
  bool recursive;
  FbleLetTcBindingV bindings;
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

// FbleTcProfiled --
//   An FbleTc along with information needed to wrap it in a profiling block.
typedef struct {
  FbleName profile_name;
  FbleLoc profile_loc;
  FbleTc* tc;
} FbleTcProfiled;

// FbleTcProfiledV --
//   Vector of FbleTcProfiled.
typedef struct {
  size_t size;
  FbleTcProfiled* xs;
} FbleTcProfiledV;

// FbleUnionSelectTc --
//   FBLE_UNION_SELECT_TC
//
// Represents a union select expression. There will be one element in the
// choices vector for each possible tag of the condition.
//
// Because of default branches in union select, it is possible that multiple
// choices point to the same tc. Code generation is expected to check for
// that and avoid generating duplicate code. FbleFreeTc, FbleFreeLoc and
// FbleFreeName should be called only once on the fields of the
// FbleTcProfiled for each unique tc referred to by choices.
typedef struct {
  FbleTc _base;
  FbleTc* condition;
  FbleTcProfiledV choices;
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
  FbleTc* func;
  FbleTcV args;
} FbleFuncApplyTc;

// FbleLinkTc --
//   FBLE_LINK_TC
//
// Represents a process link expression. Unlike FBLE_LINK_EXPR, which
// evaluates to a proc value, FBLE_LINK_TC evaluates to the result of
// computing the proc value.
//
// 'get' and 'put' fields are the names of the get and put variables for use
// in profiling.
typedef struct {
  FbleTc _base;
  FbleName get;
  FbleName put;
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
  FbleTcProfiledV bindings;
  FbleTc* body;
} FbleExecTc;

// FbleListTc --
//   FBLE_LIST_TC
//
// An expression to construct the list value that will be passed to the
// function as part of a list expression.
typedef struct {
  FbleTc _base;
  size_t fieldc;
  FbleTc* fields[];
} FbleListTc;

// FbleLiteralTc --
//   FBLE_LITERAL_TC
//
// An expression to construct the list value that will be passed to the
// function as part of a literal expression.
//
// letters[i] is the tag value to use for the ith letter in the literal.
typedef struct {
  FbleTc _base;
  size_t letterc;
  size_t letters[];
} FbleLiteralTc;

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
