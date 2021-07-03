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
  FBLE_REF_VALUE,
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

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   executable - The code for the function.
//   profile_base_id - An offset to use for profile blocks referenced from this
//                     function.
//   statics - static variables captured by the function.
//             Size is executable->statics
//
// Note: Function values are used for both pure functions and processes. We
// don't distinguish between the two at runtime, except that
// executable->args == 0 suggests this is for a process instead of a function.
struct FbleFuncValue {
  FbleValue _base;
  FbleExecutable* executable;
  size_t profile_base_id;
  FbleValue* statics[];
};

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

// FbleRefValue --
//   FBLE_REF_VALUE
//
// An implementation-specific value introduced to support recursive values and
// not yet computed values.
//
// A ref value holds a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are ref
// values.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
typedef struct {
  FbleValue _base;
  FbleValue* value;
} FbleRefValue;

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
#define FbleNewValue(heap, T) ((T*) FbleNewHeapObject(heap, sizeof(T)))

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
#define FbleNewValueExtra(heap, T, size) ((T*) FbleNewHeapObject(heap, sizeof(T) + size))

// FbleNewFuncValue --
//   Allocates a new FbleFuncValue that runs the given executable.
//
// Inputs:
//   heap - heap to use for allocations.
//   executable - the executable to run. Borrowed.
//   profile_base_id - the profile_base_id to use for the function.
//
// Results:
//   A newly allocated FbleFuncValue that is fully initialized except for
//   statics.
//
// Side effects:
//   Allocates a new FbleFuncValue that should be freed using FbleReleaseValue
//   when it is no longer needed.
FbleFuncValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id);

// FbleNewGetValue --
//   Create a new get proc value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   port - the port value to get from.
//   profile - the id of a profile block to use for when the get is executed.
//
// Results:
//   A newly allocated get value.
//
// Side effects:
//   The returned get value must be freed using FbleReleaseValue when no
//   longer in use. This function does not take ownership of the port value.
//   argument.
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port, FbleBlockId profile);

// FbleNewPutValue --
//   Create a new put value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   link - the link to put to. Borrowed.
//   profile - the first of two consecutive ids of profile blocks to use for
//             when the first argument is applied to the put and when the put
//             is executed.
//
// Results:
//   A newly allocated put value.
//
// Side effects:
//   The returned put value must be freed using FbleReleaseValue when no
//   longer in use. This function does not take ownership of the link value.
FbleValue* FbleNewPutValue(FbleValueHeap* heap, FbleValue* link, FbleBlockId profile);

// FbleNewListValue --
//   Create a new list value from the given list of arguments.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   argc - the number of elements on the list.
//   args - the elements to put on the list. Borrowed.
//
// Results:
//   A newly allocated list value.
//
// Side effects:
//   The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

// FbleNewLiteralValue --
//   Create a new literal value from the given list of letters.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   argc - the number of letters in the literal.
//   args - the tags of the letters in the literal.
//
// Results:
//   A newly allocated literal value.
//
// Side effects:
//   The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args);

// FbleStrictValue --
//   Get the strict value associated with the given value, which will either
//   be the value itself, or the dereferenced value if the value is a
//   reference.
//
// Inputs:
//   value - the value to get the strict version of.
//
// Results:
//   The value with all layers of reference indirection removed. NULL if the
//   value is a reference that has no value yet.
//
// Side effects:
//   None.
FbleValue* FbleStrictValue(FbleValue* value);

// FbleStrictRefValue --
//   Like FbleStrictValue, except returns the inner most RefValue if the value
//   is a reference that has no value yet.
//
// Inputs:
//   value - the value to get the strict version of.
//
// Results:
//   The value with all layers of reference indirection removed except the
//   last is the value is a reference that has no value yet.
//
// Side effects:
//   None.
FbleValue* FbleStrictRefValue(FbleValue* value);

#endif // FBLE_INTERNAL_VALUE_H_
