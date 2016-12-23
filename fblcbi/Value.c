
#include "fblc.h"


// NewStructValue --
//
//  Allocate a new struct value.
//
// Inputs:
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

StructValue* NewStructValue(size_t fieldc)
{
  StructValue* value = MALLOC(sizeof(StructValue) + fieldc * sizeof(Value*));
  value->refcount = 1;
  value->kind = fieldc;
  return value;
}

// NewUnionValue --
//
//  Allocate a new union value.
//
// Inputs:
//   None.
//
// Results:
//   A newly allocated union value. The field and tag of the union are left
//   uninitialized.
//
// Side effects:
//   Allocates a new union value. Use Copy to make a (shared) copy of the
//   union value, and Release to release the resources associated with
//   the value.

UnionValue* NewUnionValue()
{
  UnionValue* value = MALLOC(sizeof(UnionValue));
  value->refcount = 1;
  value->kind = UNION_KIND;
  return value;
}

// Copy --
//
//   Make a (likely shared) copy of the given value.
//
// Inputs:
//   src - The value to make a copy of.
//
// Results:
//   A copy of the value, which may be the same as the original value.
//
// Side effects:
//   None.

Value* Copy(Value* src)
{
  src->refcount++;
  return src;
}

// Release --
//
//   Free the resources associated with a value.
//
// Inputs:
//   value - The value to free the resources of.
//
// Results:
//   None.
//
// Side effect:
//   The resources for the value are freed. The value may be NULL, in which
//   case no action is taken.

void Release(Value* value)
{
  if (value != NULL) {
    value->refcount--;
    if (value->refcount == 0) {
      if (value->kind == UNION_KIND) {
        UnionValue* union_value = (UnionValue*)value;
        Release(union_value->field);
      } else {
        StructValue* struct_value = (StructValue*)value;
        for (size_t i = 0; i < value->kind; ++i) {
          Release(struct_value->fields[i]);
        }
      }
      FREE(value);
    }
  }
}
