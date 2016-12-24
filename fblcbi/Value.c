
#include <assert.h>
#include <stdlib.h>

#include "fblc.h"

// Value --
//   A struct or union value.
//
// Fields:
//   refcount - The number of references to the value.
//   kind - The kind of value and number of fields. If non-negative, the
//   value is a StructValue with kind fields. If negative, the value is a
//   UnionValue with abs(kind) fields.
struct Value {
  size_t refcount;
  ssize_t kind;
};

typedef struct {
  size_t refcount;
  ssize_t kind;
  Value* fields[];
} StructValue;

typedef struct {
  size_t refcount;
  ssize_t kind;
  size_t tag;
  Value* field;
} UnionValue;

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
      switch (Kind(value)) {
        case STRUCT_KIND:
          for (size_t i = 0; i < NumFields(value); ++i) {
            Release(arena, StructField(value, i));
          }
          break;

        case UNION_KIND:
          Release(arena, UnionField(value));
          break;

        default:
          assert(false && "Unknown value kind");
          break;
      }
      arena->free(arena, value);
    }
  }
}

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

Value* NewStruct(FblcArena* arena, size_t fieldc)
{
  StructValue* value = arena->alloc(arena, sizeof(StructValue) + fieldc * sizeof(Value*));
  value->refcount = 1;
  value->kind = fieldc;
  return (Value*)value;
}

ValueKind Kind(Value* value)
{
  return value->kind >= 0 ? STRUCT_KIND : UNION_KIND;
}

size_t NumFields(Value* value)
{
  return abs(value->kind);
}

Value* StructField(Value* this, size_t tag)
{
  return *StructFieldRef(this, tag);
}

Value** StructFieldRef(Value* this, size_t tag)
{
  assert(Kind(this) == STRUCT_KIND);
  assert(tag < NumFields(this));
  StructValue* struct_value = (StructValue*)this;
  return struct_value->fields + tag;
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

Value* NewUnion(FblcArena* arena, size_t fieldc, size_t tag, Value* field)
{
  UnionValue* value = arena->alloc(arena, sizeof(UnionValue));
  value->refcount = 1;
  value->kind = -fieldc;
  value->tag = tag;
  value->field = field;
  return (Value*)value;
}

size_t SizeOfUnionTag(Value* value)
{
  assert(Kind(value) == UNION_KIND);
  size_t fieldc = -value->kind;
  size_t size = 0;
  while ((1 << size) < fieldc) {
    ++size;
  }
  return size;
}

size_t UnionTag(Value* value)
{
  assert(Kind(value) == UNION_KIND);
  UnionValue* union_value = (UnionValue*)value;
  return union_value->tag;
}

Value* UnionField(Value* value)
{
  return *UnionFieldRef(value);
}

Value** UnionFieldRef(Value* value)
{
  assert(Kind(value) == UNION_KIND);
  UnionValue* union_value = (UnionValue*)value;
  return &union_value->field;
}
