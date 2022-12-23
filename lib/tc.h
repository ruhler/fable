/**
 * @file tc.h
 * Header for FbleTc, typed abstract syntax for fble.
 */

#ifndef FBLE_INTERNAL_TC_H_
#define FBLE_INTERNAL_TC_H_

#include <fble/fble-loc.h>
#include <fble/fble-name.h>

#include "kind.h"       // for FbleDataTypeTag
#include "var.h"        // for FbleVar

// FbleTc --
//   An already type-checked representation of an fble syntactic expression.
//
// FbleTc is like FbleExpr, except that:
// * Field and variable names are replaced with integer indicies.
// * Types are eliminated.
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
//
// FbleTc is reference counted. Pass by pointer. Explicit copy and free
// required. The magic field is set to FBLE_TC_MAGIC and is used to detect
// double frees of FbleTc.
#define FBLE_TC_MAGIC 0x5443
struct FbleTc {
  size_t refcount;
  size_t magic;
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

// FbleVarTc --
//   FBLE_VAR_TC
//
// A variable expression.
// * Used to represent variables refering to function arguments or local
//   variables. 
//
// For args, index starts at 0 and increases by one for each argument going
// from left to right.
//
// For local variables, index starts at 0 and increases by one for each new
// variable introduced, going from left to right, outer most to inner most
// binding.
typedef struct {
  FbleTc _base;
  FbleVar var;
} FbleVarTc;

// FbleTcBinding --
//   Information for a binding. Used for let bindings, exec bindings, and case
//   branches.
//
// name - The name of the variable or branch.
// loc - The location of the value.
// tc - The value of the binding.
typedef struct {
  FbleName name;
  FbleLoc loc;
  FbleTc* tc;
} FbleTcBinding;

// FbleTcBindingV --
//   A vector of FbleTcBinding.
typedef struct {
  size_t size;
  FbleTcBinding* xs;
} FbleTcBindingV;

// FbleLetTc --
//   FBLE_LET_TC
//
// Represents a let expression.
//
// The bindings are bound to variables implicitly based on the position of the
// binding in the let expression and the position of the let expression in its
// parent expression as specified for FbleVar.
//
// Fields:
//   recursive - false if the let is a non-recursive let expression.
//   bindings - the variables being defined.
typedef struct {
  FbleTc _base;
  bool recursive;
  FbleTcBindingV bindings;
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
  FbleTcBindingV choices;
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
  FbleVarV scope;       /**< Sources of static variables. */
  FbleNameV statics;    /**< Names of static variables. */
  FbleNameV args;       /**< Names of arguments. */
  FbleTc* body;         /**< The body of the function. */
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

// FbleNewTc --
//   Allocate a new tc. This function is not type safe.
//
// Inputs:
//   T - the type of the tc to allocate.
//   tag - the tag of the tc.
//   loc - the source loc of the tc. Borrowed.
//
// Results:
//   The newly allocated tc.
//
// Side effects:
//   Allocates a new tc that should be freed using FbleFreeTc when no longer
//   needed.
#define FbleNewTc(T, tag, loc) ((T*) FbleNewTcRaw(sizeof(T), tag, loc))
FbleTc* FbleNewTcRaw(size_t size, FbleTcTag tag, FbleLoc loc);

// FbleNewTcExtra --
//   Allocate a new tc with additional extra space.
//
// Inputs:
//   T - the type of the tc to allocate.
//   size - the size in bytes of additional extra space to include.
//   tag - the tag of the tc.
//   loc - the source loc of the tc. Borrowed.
//
// Results:
//   The newly allocated tc.
//
// Side effects:
//   Allocates a new tc that should be freed using FbleFreeTc when no longer
//   needed.
#define FbleNewTcExtra(T, tag, size, loc) ((T*) FbleNewTcRaw(sizeof(T) + size, tag, loc))

// FbleCopyTc --
//   Make a reference counted copy of the given tc.
//
// Inputs:
//   tc - the tc to copy.
//
// Result:
//   The copy of the tc.
//
// Side effects:
//   The user should arrange for FbleFreeTc to be called on this tc when it is
//   no longer needed.
FbleTc* FbleCopyTc(FbleTc* tc);

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
