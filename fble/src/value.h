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

// FbleExecutable --
//   A reference counted, partially abstract data type describing how to
//   execute a function.
//
// The 'on_free' function is called passing this as an argument just before
// the FbleExecutable object is freed. Subclasses should use this to free any
// custom state.
#define FBLE_EXECUTABLE_MAGIC 0xB10CE
struct FbleExecutable {
  size_t refcount;        // reference count.
  size_t magic;           // FBLE_EXECUTABLE_MAGIC.
  size_t args;            // The number of arguments expected by the function.
  size_t statics;         // The number of statics used by the function.
  size_t locals;          // The number of locals used by the function.
  FbleRunFunction* run;   // How to run the function.
  void (*on_free)(struct FbleExecutable* this);
};

// FbleFreeExecutable --
//   Decrement the refcount and, if necessary, free resources associated with
//   the given executable.
//
// Inputs:
//   executable - the executable to free. May be NULL.
//
// Side effects:
//   Decrements the refcount and, if necessary, calls executable->on_free and
//   free resources associated with the given executable.
void FbleFreeExecutable(FbleExecutable* executable);

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   executable - The code for the function.
//   statics - static variables captured by the function.
//             Size is executable->statics
//
// Note: Function values are used for both pure functions and processes. We
// don't distinguish between the two at runtime, except that
// executable->args == 0 suggests this is for a process instead of a function.
struct FbleFuncValue {
  FbleValue _base;
  FbleExecutable* executable;
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
//
// Results:
//   A newly allocated FbleFuncValue that is fully initialized except for
//   statics.
//
// Side effects:
//   Allocates a new FbleFuncValue that should be freed using FbleReleaseValue
//   when it is no longer needed.
FbleFuncValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable);

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

#endif // FBLE_INTERNAL_VALUE_H_
