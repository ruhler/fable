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
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_LINK_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
  FBLE_TYPE_VALUE,

  FBLE_SYMBOLIC_VALUE,
  FBLE_STRUCT_ACCESS_VALUE,
  FBLE_UNION_ACCESS_VALUE,
  FBLE_UNION_SELECT_VALUE,
  // FBLE_FUNC_APPLY_VALUE,
  // FBLE_PROC_EXEC_VALUE,
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

// FbleSymbolicValue --
//   FBLE_SYMBOLIC_VALUE
//
// A value representing a symbol. A symbolic value contains no information,
// except that two symbolic values can be compared for equality or not based
// on their pointers.
typedef struct {
  FbleValue _base;
} FbleSymbolicValue;

// FbleAccessValue --
//   FBLE_STRUCT_ACCESS_VALUE
//   FBLE_UNION_ACCESS_VALUE
typedef struct {
  FbleValue _base;
  FbleValue* obj;
  size_t tag;
  FbleLoc loc;
} FbleAccessValue;

// FbleUnionSelectValue --
//   FBLE_UNION_SELECT_VALUE
typedef struct {
  FbleValue _base;
  FbleValue* condition;
  size_t choicec;
  FbleValue* choices[];
} FbleUnionSelectValue;

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
