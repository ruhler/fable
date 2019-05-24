// fble-value.h --
//   Header file for the fble value APIs.

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include "fble-alloc.h"
#include "fble-vector.h"

typedef struct FbleRefArena FbleValueArena;

// FbleValue --
//   An fble value.
typedef struct FbleValue FbleValue;

// FbleValueV --
//   A vector of FbleValue*
typedef struct {
  size_t size;
  FbleValue** xs;
} FbleValueV;

// FbleNewValueArena --
//   Create a new arena for allocation of values.
// 
// Inputs: 
//   arena - the arena to use for underlying allocations.
//
// Results:
//   A value arena that can be used to allocate values.
//
// Side effects:
//   Allocates a value arena that should be freed using FbleDeleteValueArena.
FbleValueArena* FbleNewValueArena(FbleArena* arena);

// FbleDeleteValueArena --
//   Reclaim resources associated with a value arena.
//
// Inputs:
//   arena - the arena to free.
//
// Results:
//   None.
//
// Side effects:
//   The resources associated with the given arena are freed. The arena should
//   not be used after this call.
void FbleDeleteValueArena(FbleValueArena* arena);

// FbleValueRetain --
//   Keep the given value alive until a corresponding FbleValueRelease is
//   called.
//
// Inputs:
//   arena - The arena used to allocate the value.
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
FbleValue* FbleValueRetain(FbleValueArena* arena, FbleValue* src);

// FbleValueRelease --
//
//   Decrement the strong reference count of a value and free the resources
//   associated with that value if it has no more references.
//
// Inputs:
//   arena - The value arena the value was allocated with.
//   value - The value to decrement the strong reference count of. The value
//           may be NULL, in which case no action is performed.
//
// Results:
//   None.
//
// Side effect:
//   Decrements the strong reference count of the value and frees resources
//   associated with the value if there are no more references to it.
void FbleValueRelease(FbleValueArena* arena, FbleValue* value);

// FbleNewStructValue --
//   Create a new struct value with given arguments.
//
// Inputs:
//   arena - The arena to use for allocations.
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
FbleValue* FbleNewStructValue(FbleValueArena* arena, FbleValueV args);

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
//   arena - The arena to use for allocations.
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
FbleValue* FbleNewUnionValue(FbleValueArena* arena, size_t tag, FbleValue* arg);

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

// FbleNewPortValue --
//   Create a new io port value with given id.
//
// Inputs:
//   arena - the arena to use for allocations.
//   id - the id of the port for use with FbleIO.
//
// Results:
//   A newly allocated port value.
//
// Side effects:
//   The returned port value must be freed using FbleValueRelease when no
//   longer in use.
FbleValue* FbleNewPortValue(FbleValueArena* arena, size_t id);

#endif // FBLE_VALUE_H_
