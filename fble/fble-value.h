// fble-value.h --
//   Header file for the fble value APIs.

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include "fble-alloc.h"
#include "fble-vector.h"
#include "fble-ref.h"

typedef FbleRefArena FbleValueArena;

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_PROC_VALUE,
  FBLE_INPUT_VALUE,
  FBLE_OUTPUT_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
  FBLE_TYPE_VALUE,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
typedef struct FbleValue {
  FbleRef ref;
  FbleValueTag tag;
} FbleValue;

// FbleValueV --
//   A vector of FbleValue*
typedef struct {
  size_t size;
  FbleValue** xs;
} FbleValueV;

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleValue _base;
  FbleValueV fields;
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

// FbleFuncValue --
//   FBLE_FUNC_VALUE
//
// Defined internally, because representing the body of the function depends
// on the internals of the evaluator.
typedef struct FbleFuncValue FbleFuncValue;

// FbleProcValue --
//   FBLE_PROC_VALUE
//
// Defined internally, because representing a process depends on the internals
// of the evaluator.
typedef struct FbleProcValue FbleProcValue;

// FbleInputValue --
//   FBLE_INPUT_VALUE
//
// Defined internally.
typedef struct FbleInputValue FbleInputValue;

// FbleOutputValue --
//   FBLE_OUTPUT_VALUE
//
// Defined internally.
typedef struct FbleOutputValue FbleOutputValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
//
// Defined internally.
typedef struct FblePortValue FblePortValue;

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
typedef struct FbleTypeValue {
  FbleValue _base;
} FbleTypeValue;

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
