// fble-value.h --
//   Header file for the fble value APIs.

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

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
//   Allocates a heap that should be freed using FbleDeleteValueHeap.
FbleValueHeap* FbleNewValueHeap(FbleArena* arena);

// FbleDeleteValueHeap --
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
void FbleDeleteValueHeap(FbleValueHeap* heap);

// FbleNewValue --
//   Allocate a new value of the given type.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//
// Results:
//   The newly allocated value.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define FbleNewValue(heap, T) ((T*) heap->new(heap, sizeof(T)))

// FbleValueRetain --
//   Keep the given value alive until a corresponding FbleValueRelease is
//   called.
//
// Inputs:
//   arena - The heap used to allocate the value.
//   value - The value to retain. The value may be NULL, in which case nothing
//           is done.
//
// Results:
//   The given value, for the caller's convenience when retaining the
//   value and assigning it at the same time.
//
// Side effects:
//   Causes the value to be retained until a corresponding FbleValueRelease
//   calls is made on the value. FbleValueRelease must be called when the
//   value is no longer needed.
FbleValue* FbleValueRetain(FbleValueHeap* heap, FbleValue* src);

// FbleValueRelease --
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
void FbleValueRelease(FbleValueHeap* heap, FbleValue* value);

// FbleValueAddRef --
//   Notify the value heap of a new reference from src to dst.
//
// Inputs:
//   heap - the heap the values are allocated on.
//   src - the source of the reference.
//   dst - the destination of the reference.
//
// Side effects:
//   Causes the dst value to be retained for at least as long as the src value.
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);

// FbleValueDelRef --
//   Notify the value heap of a deleted reference from src to dst.
//
// Inputs:
//   heap - the heap the values are allocated on.
//   src - the source of the reference.
//   dst - the destination of the reference.
//
// Side effects:
//   Causes the dst value to no longer be retained by src.
void FbleValueDelRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);

// FbleNewStructValue --
//   Create a new struct value with given arguments.
//
// Inputs:
//   heap - The heap to allocate the value on.
//   args - The arguments to the struct value.
//
// Results:
//   A newly allocated struct value with given args.
//
// Side effects:
//   The returned struct value must be freed using FbleValueRelease when no
//   longer in use. The args are freed using FbleValueRelease as part of
//   calling this function. The function does not take ownership of the args
//   array.
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
//   arg - The argument of the union value.
//
// Results:
//   A newly allocated union value with given tag and arg.
//
// Side effects:
//   The returned union value must be freed using FbleValueRelease when no
//   longer in use. The arg is freed using FbleValueRelease as part of calling
//   this function.
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg);

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
//   id - the id of the port for use with FbleIO.
//
// Results:
//   A newly allocated port value.
//
// Side effects:
//   The returned port value must be freed using FbleValueRelease when no
//   longer in use.
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, size_t id);

// FbleNewOutputPortValue --
//   Create a new output port value with given id.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   id - the id of the port for use with FbleIO.
//
// Results:
//   A newly allocated port value.
//
// Side effects:
//   The returned port value must be freed using FbleValueRelease when no
//   longer in use.
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, size_t id);

#endif // FBLE_VALUE_H_
