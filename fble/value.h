// value.h --
//   Header file describing the internal interface for working with fble
//   values.
//   This is an internal library interface.

#ifndef FBLE_INTERNAL_VALUE_H_
#define FBLE_INTERNAL_VALUE_H_

#include "fble.h"
#include "heap.h"
#include "instr.h"

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
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
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_LINK_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
  FBLE_TYPE_VALUE,

  FBLE_VAR_VALUE,
  FBLE_DATA_ACCESS_VALUE,
  FBLE_UNION_SELECT_VALUE,
  FBLE_PROFILE_TC,

  FBLE_LINK_TC,
  FBLE_EXEC_TC,
  FBLE_SYMBOLIC_VALUE_TC,
  FBLE_SYMBOLIC_COMPILE_TC,

  FBLE_LET_TC,
  FBLE_FUNC_VALUE_TC,
  FBLE_FUNC_APPLY_TC,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
struct FbleValue {
  FbleValueTag tag;
};

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleValue _base;
  size_t fieldc;
  FbleValue* fields[];
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   argc - The number of arguments expected by the function.
//   code - The code for the function.
//   scope - The scope at the time the function was created, representing the
//           lexical context available to the function. The length of this
//           array is code->statics.
//
// Note: Function values are used for both pure functions and processes. We
// don't distinguish between the two at runtime, except that argc == 0
// suggests this is for a process instead of a function.
typedef struct {
  FbleValue _base;
  size_t argc;
  FbleInstrBlock* code;
  FbleValue* scope[];
} FbleFuncValue;

// FbleProcValue -- FBLE_PROC_VALUE
//   A proc value is represented as a function that takes no arguments.
#define FBLE_PROC_VALUE FBLE_FUNC_VALUE
typedef FbleFuncValue FbleProcValue;

// FbleValues --
//   A non-circular singly linked list of values.
typedef struct FbleValues {
  FbleValue* value;
  struct FbleValues* next;
} FbleValues;

// FbleLinkValue -- FBLE_LINK_VALUE
//   Holds the list of values on a link. Values are added to the tail and taken
//   from the head. If there are no values on the list, both head and tail are
//   set to NULL.
typedef struct {
  FbleValue _base;
  FbleValues* head;
  FbleValues* tail;
} FbleLinkValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
//
// data is a pointer to a value owned externally where data should be put to
// and got from.
typedef struct {
  FbleValue _base;
  FbleValue** data;
} FblePortValue;

// FbleRefValue --
//   FBLE_REF_VALUE
//
// A implementation-specific value introduced to support recursive values. A
// ref value is simply a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are reference
// values.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
typedef struct FbleRefValue {
  FbleValue _base;
  FbleValue* value;
} FbleRefValue;

// FbleTypeValue --
//   FBLE_TYPE_VALUE
//
// A value representing a type. Because types are compile-time concepts, not
// runtime concepts, the type value contains no information.
typedef struct {
  FbleValue _base;
} FbleTypeValue;

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

// FbleVarValue --
//   FBLE_VAR_VALUE
//
// A value representing a specific variable.
typedef struct {
  FbleValue _base;
  FbleVarIndex index;
} FbleVarValue;

// FbleDataAccessValue --
//   FBLE_DATA_ACCESS_VALUE
typedef struct {
  FbleValue _base;
  FbleDataTypeTag datatype;
  FbleValue* obj;
  size_t tag;
  FbleLoc loc;
} FbleDataAccessValue;

// FbleUnionSelectValue --
//   FBLE_UNION_SELECT_VALUE
//
// Because of default branches in union select, it is possible that multiple
// choices point to the same value. Code generation is expected to check for
// that and avoid generating duplicate code.
typedef struct {
  FbleValue _base;
  FbleLoc loc;
  FbleValue* condition;
  size_t choicec;
  FbleValue* choices[];
} FbleUnionSelectValue;

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
  FbleValue _base;
  FbleName name;
  FbleLoc loc;
  FbleValue* body;
} FbleProfileTc;

// FbleLinkTc --
//   FBLE_LINK_TC
//
// Unlike FBLE_LINK_EXPR, which evaluates to a proc value, FBLE_LINK_TC
// evaluates to the result of the computing the proc value.
typedef struct {
  FbleValue _base;
  FbleValue* body;
} FbleLinkTc;

// FbleExecTc --
//   FBLE_EXEC_TC
//
// Unlike FBLE_EXEC_EXPR, which evaluates to a proc value, FBLE_EXEC_TC
// evaluates to the result of the computing the proc value.
typedef struct {
  FbleValue _base;
  FbleValueV bindings;
  FbleValue* body;
} FbleExecTc;

// FbleSymbolicValueTc --
//   FBLE_SYMBOLIC_VALUE_TC
//
// An expression to allocate a new symbolic value.
typedef struct {
  FbleValue _base;
} FbleSymbolicValueTc;

// FbleSymbolicCompileTc --
//   FBLE_SYMBOLIC_COMPILE_TC 
//
// An expression to compile a symbolic value into a function.
typedef struct {
  FbleValue _base;
  FbleLoc loc;
  FbleVarIndexV args;
  FbleValue* body;
} FbleSymbolicCompileTc;

// FbleLetTc --
//   FBLE_LET_TC
//
// recursive is false if the let is a non-recursive let expression.
typedef struct {
  FbleValue _base;
  bool recursive;
  FbleValueV bindings;
  FbleValue* body;
} FbleLetTc;

// FbleFuncValueTc --
//   FBLE_FUNC_VALUE_TC
//
// Note: FuncValueTc is used for process values as well as function values.
typedef struct {
  FbleValue _base;
  FbleLoc body_loc;
  FbleVarIndexV scope;
  size_t argc;
  FbleValue* body;
} FbleFuncValueTc;

// FbleFuncApplyTc --
//   FBLE_FUNC_APPLY_TC
typedef struct {
  FbleValue _base;
  FbleLoc loc;
  FbleValue* func;
  FbleValueV args;
} FbleFuncApplyTc;

// FbleNewValue --
//   Allocate a new value of the given type.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//
// Results:
//   The newly allocated value. The value does not have its tag initialized.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define FbleNewValue(heap, T) ((T*) heap->new(heap, sizeof(T)))

// FbleNewValueExtra --
//   Allocate a new value of the given type with some extra space.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//   size - the number of bytes of extra space to include in the allocated
//   object.
//
// Results:
//   The newly allocated value with extra space.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define FbleNewValueExtra(heap, T, size) ((T*) heap->new(heap, sizeof(T) + size))

// FbleNewGetValue --
//   Create a new get proc value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   port - the port value to get from.
//
// Results:
//   A newly allocated get value.
//
// Side effects:
//   The returned get value must be freed using FbleReleaseValue when no
//   longer in use. This function does not take ownership of the port value.
//   argument.
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port);

// FbleNewPutValue --
//   Create a new put value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   link - the link to put to.
//
// Results:
//   A newly allocated put value.
//
// Side effects:
//   The returned put value must be freed using FbleReleaseValue when no
//   longer in use. This function does not take ownership of the link value.
FbleValue* FbleNewPutValue(FbleValueHeap* heap, FbleValue* link);

#endif // FBLE_INTERNAL_VALUE_H_
