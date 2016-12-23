
#include <stdlib.h>

#include "fblc.h"


// NewStructValue --
//
//  Allocate a new struct value.
//
// Inputs:
//   arena - The arena to use for allocations.
//   fieldc - The number of fields of the struct.
//
// Results:
//   A newly allocated struct value. The fields of the struct are left
//   uninitialized.
//
// Side effects:
//   Allocates a new struct value. Use Copy to make a (shared) copy of the
//   struct value and Release to release the resources associated with
//   the value.

StructValue* NewStructValue(FblcArena* arena, size_t fieldc)
{
  StructValue* value = arena->alloc(arena, sizeof(StructValue) + fieldc * sizeof(Value*));
  value->refcount = 1;
  value->kind = fieldc;
  return value;
}

// NewUnionValue --
//
//  Allocate a new union value.
//
// Inputs:
//   arena - The arena to use for allocations.
//
// Results:
//   A newly allocated union value. The field and tag of the union are left
//   uninitialized.
//
// Side effects:
//   Allocates a new union value. Use Copy to make a (shared) copy of the
//   union value, and Release to release the resources associated with
//   the value.

UnionValue* NewUnionValue(FblcArena* arena, size_t fieldc)
{
  UnionValue* value = arena->alloc(arena, sizeof(UnionValue));
  value->refcount = 1;
  value->kind = -fieldc;
  return value;
}

// Copy --
//
//   Make a (likely shared) copy of the given value.
//
// Inputs:
//   arena - The arena to use for allocations.
//   src - The value to make a copy of.
//
// Results:
//   A copy of the value, which may be the same as the original value.
//
// Side effects:
//   Performs arena allocations.

Value* Copy(FblcArena* arena, Value* src)
{
  src->refcount++;
  return src;
}

// Release --
//
//   Free the resources associated with a value.
//
// Inputs:
//   arena - The arena the value was allocated with.
//   value - The value to free the resources of.
//
// Results:
//   None.
//
// Side effect:
//   The resources for the value are freed. The value may be NULL, in which
//   case no action is taken.

void Release(FblcArena* arena, Value* value)
{
  if (value != NULL) {
    value->refcount--;
    if (value->refcount == 0) {
      if (value->kind < 0) {
        UnionValue* union_value = (UnionValue*)value;
        Release(arena, union_value->field);
      } else {
        StructValue* struct_value = (StructValue*)value;
        for (size_t i = 0; i < value->kind; ++i) {
          Release(arena, struct_value->fields[i]);
        }
      }
      arena->free(arena, value);
    }
  }
}
