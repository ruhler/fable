// value.h --
//   Header file describing the internal interface for working with fble
//   values.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_VALUE_H_
#define FBLE_INTERNAL_VALUE_H_

#include "fble-link.h"    // for typedef of FbleExecutable
#include "fble-value.h"
#include "execute.h"      // for FbleRunFunction
#include "heap.h"

// FbleValueTag --
//   A tag used to distinguish among different kinds of FbleValue.
typedef enum {
  FBLE_TYPE_VALUE,
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_LINK_VALUE,
  FBLE_PORT_VALUE,
  FBLE_THUNK_VALUE,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
struct FbleValue {
  FbleValueTag tag;
};

// FbleTypeValue --
//   FBLE_TYPE_VALUE
//
// Represents the type value. Because types are compile-time concepts, not
// runtime concepts, the type value contains no information.
typedef struct {
  FbleValue _base;
} FbleTypeValue;

// FbleStructValue --
//   FBLE_STRUCT_VALUE
//
// Represents a struct value.
typedef struct {
  FbleValue _base;
  size_t fieldc;
  FbleValue* fields[];
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
//
// Represents a union value.
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

// FbleExecutable --
//
// TODO: Turn this into an abstract data type that allows multiple
// implementations.
//
// Fields:
//   code - The code for the function.
//   run - A native function to use to evaluate this fble function.
struct FbleExecutable {
  FbleCode* code;
  FbleRunFunction* run;
};

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   argc - The number of arguments expected by the function.
//   executable - The code for the function.
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
  FbleExecutable* executable;
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
// Fields:
//   data - a pointer to a value owned externally where data should be put to
//          and got from.
typedef struct {
  FbleValue _base;
  FbleValue** data;
} FblePortValue;

// FbleThunkValue --
//   FBLE_THUNK_VALUE
//
// A implementation-specific value introduced to support recursive values and
// partially evaluated expressions.
//
// A thunk value holds a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are thunk
// values.
//
// For recursive values, 'tail' and 'func' will be NULL, 'joins' and 'pc' will
// be 0 and 'locals' will be empty.
//
// For partially evaluated expressions, func is the currently executing
// function, pc the location in that function, locals the list of current
// local variables, and tail is a thunk to compute after this thunk is
// finished computing.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
//   tail - the next thunk to compute after this one.
//   joins - the number of threads to join before continuing with this thunk.
//   func - the function being executed.
//   pc - offset into func->code.
//   locals - vector of local variables.
typedef struct FbleThunkValue {
  FbleValue _base;
  FbleValue* value;

  struct FbleThunkValue* tail;
  size_t joins;
  FbleFuncValue* func;
  size_t pc;
  FbleValueV locals;
} FbleThunkValue;

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

// FbleStrictValue --
//   Get the strict value associated with the given value, which will either
//   be the value itself, or the computed result if the value is a thunk.
//
// Inputs:
//   value - the value to get the strict version of.
//
// Results:
//   The value with all layers of thunks removed. NULL if the value is a thunk
//   that has not been fully computed.
//
// Side effects:
//   None.
FbleValue* FbleStrictValue(FbleValue* value);

#endif // FBLE_INTERNAL_VALUE_H_
