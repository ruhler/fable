// fble-value.h --
//   Header file for the fble value APIs.

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include <stdbool.h>    // for bool

#include "fble-alloc.h"
#include "fble-vector.h"

typedef struct FbleHeap FbleValueHeap;

// FbleValue --
//   An fble value.
typedef struct FbleValue FbleValue;

// FbleValueV --
//   A vector of FbleValue*
typedef struct {
  size_t size;
  FbleValue** xs;
} FbleValueV;

// FbleNewValueHeap --
//   Create a new heap for allocation of values.
// 
// Inputs: 
//   arena - the arena to use for underlying allocations.
//
// Results:
//   A heap that can be used to allocate values.
//
// Side effects:
//   Allocates a heap that should be freed using FbleFreeValueHeap.
FbleValueHeap* FbleNewValueHeap(FbleArena* arena);

// FbleFreeValueHeap --
//   Reclaim resources associated with a value heap.
//
// Inputs:
//   heap - the heap to free.
//
// Results:
//   None.
//
// Side effects:
//   The resources associated with the given heap are freed. The heap should
//   not be used after this call.
void FbleFreeValueHeap(FbleValueHeap* heap);

// FbleRetainValue --
//   Keep the given value alive until a corresponding FbleReleaseValue is
//   called.
//
// Inputs:
//   arena - The heap used to allocate the value.
//   value - The value to retain.
//
// Side effects:
//   Causes the value to be retained until a corresponding FbleReleaseValue
//   calls is made on the value. FbleReleaseValue must be called when the
//   value is no longer needed.
void FbleRetainValue(FbleValueHeap* heap, FbleValue* src);

// FbleReleaseValue --
//
//   Decrement the strong reference count of a value and free the resources
//   associated with that value if it has no more references.
//
// Inputs:
//   heap - The heap the value was allocated with.
//   value - The value to decrement the strong reference count of. The value
//           may be NULL, in which case no action is performed.
//
// Results:
//   None.
//
// Side effect:
//   Decrements the strong reference count of the value and frees resources
//   associated with the value if there are no more references to it.
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value);

// FbleValueAddRef --
//   Notify the value heap of a new reference from src to dst.
//
// Inputs:
//   heap - the heap the values are allocated on.
//   src - the source of the reference.
//   dst - the destination of the reference. May be NULL.
//
// Side effects:
//   Causes the dst value to be retained for at least as long as the src value.
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);

// FbleValueFullGc --
//   Perform a full garbage collection on the value heap. Frees any
//   unreachable objects currently on the heap.
//
// This is an expensive operation intended for test and debug purposes.
//
// Inputs:
//   heap - the heap to perform gc on.
//
// Side effects:
//   Frees any unreachable objects currently on the heap.
void FbleValueFullGc(FbleValueHeap* heap);

// FbleNewStructValue --
//   Create a new struct value with given arguments.
//
// Inputs:
//   heap - The heap to allocate the value on.
//   args - The arguments to the struct value. Borrowed.
//
// Results:
//   A newly allocated struct value with given args.
//
// Side effects:
//   The returned struct value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewStructValue(FbleValueHeap* heap, FbleValueV args);

// FbleStructValueAccess --
//   Gets the given field value of a struct value.
//
// Inputs:
//   object - the struct value object to get the field value of.
//   field - the field to access.
//
// Results:
//   The value of the given field of the struct value object.
//
// Side effects:
//   Behavior is undefined if the object is not a struct value or the field
//   is invalid.
//   
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field);

// FbleNewUnionValue --
//   Create a new union value with given tag and argument.
//
// Inputs:
//   heap - The heap to allocate the value on.
//   tag - The tag of the union value.
//   arg - The argument of the union value. Borrowed.
//
// Results:
//   A newly allocated union value with given tag and arg.
//
// Side effects:
//   The returned union value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg);

// FbleNewEnumValue --
//   Create a new union value with given tag. Convenience function for
//   creating unions with value of type *().
//
// Inputs:
//   heap - The heap to allocate the value on.
//   tag - The tag of the union value.
//
// Results:
//   A newly allocated union value with given tag and arg.
//
// Side effects:
//   The returned union value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag);

// FbleUnionValueTag --
//   Gets the tag of a union value.
//
// Inputs:
//   object - the union value object to get the tag of.
//
// Results:
//   The tag of the union value object.
//
// Side effects:
//   Behavior is undefined if the object is not a union value.
//   
size_t FbleUnionValueTag(FbleValue* object);

// FbleUnionValueAccess --
//   Gets the argument of a union value.
//
// Inputs:
//   object - the union value object to get the argument of.
//
// Results:
//   The argument of the union value object.
//
// Side effects:
//   Behavior is undefined if the object is not a union value.
//   
FbleValue* FbleUnionValueAccess(FbleValue* object);

// FbleIsProcValue --
//   Returns true if the value represents a process value.
//
// Inputs:
//   value - the value to check.
//
// Results:
//   true if the value is a proc value, false otherwise.
//
// Side effects:
//   none.
bool FbleIsProcValue(FbleValue* value);

// FbleNewInputPortValue --
//   Create a new input port value with given id.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   data - a pointer to where the input data will be communicated.
//
// Results:
//   A newly allocated port value.
//
// Side effects:
//   The returned port value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, FbleValue** data);

// FbleNewOutputPortValue --
//   Create a new output port value with given id.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   data - a pointer to where the output data will be communicated.
//
// Results:
//   A newly allocated port value.
//
// Side effects:
//   The returned port value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, FbleValue** data);

#endif // FBLE_VALUE_H_
