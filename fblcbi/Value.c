
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
      if (IsUnionValue(value)) {
        Release(arena, GetUnionField(value));
      } else {
        assert(IsStructValue(value));
        for (size_t i = 0; i < NumStructFields(value); ++i) {
          Release(arena, GetStructField(value, i));
        }
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

Value* NewStructValue(FblcArena* arena, size_t fieldc)
{
  StructValue* value = arena->alloc(arena, sizeof(StructValue) + fieldc * sizeof(Value*));
  value->refcount = 1;
  value->kind = fieldc;
  return (Value*)value;
}

bool IsStructValue(Value* value)
{
  return value->kind >= 0;
}

size_t NumStructFields(Value* value)
{
  assert(IsStructValue(value));
  return value->kind;
}

Value* GetStructField(Value* this, size_t tag)
{
  assert(IsStructValue(this));
  assert(tag < NumStructFields(this));
  StructValue* struct_value = (StructValue*)this;
  return struct_value->fields[tag];
}

Value** GetStructFieldRef(Value* this, size_t tag)
{
  assert(IsStructValue(this));
  assert(tag < NumStructFields(this));
  StructValue* struct_value = (StructValue*)this;
  return struct_value->fields + tag;
}

void SetStructField(Value* this, size_t tag, Value* field)
{
  assert(IsStructValue(this));
  assert(tag < NumStructFields(this));
  StructValue* struct_value = (StructValue*)this;
  struct_value->fields[tag] = field;
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

Value* NewUnionValue(FblcArena* arena, size_t fieldc)
{
  UnionValue* value = arena->alloc(arena, sizeof(UnionValue));
  value->refcount = 1;
  value->kind = -fieldc;
  return (Value*)value;
}

bool IsUnionValue(Value* value)
{
  return value->kind < 0;
}

size_t SizeOfUnionTag(Value* value)
{
  assert(IsUnionValue(value));
  size_t fieldc = -value->kind;
  size_t size = 0;
  while ((1 << size) < fieldc) {
    ++size;
  }
  return size;
}

size_t GetUnionTag(Value* value)
{
  assert(IsUnionValue(value));
  UnionValue* union_value = (UnionValue*)value;
  return union_value->tag;
}

Value* GetUnionField(Value* value)
{
  assert(IsUnionValue(value));
  UnionValue* union_value = (UnionValue*)value;
  return union_value->field;
}

Value** GetUnionFieldRef(Value* value)
{
  assert(IsUnionValue(value));
  UnionValue* union_value = (UnionValue*)value;
  return &union_value->field;
}

void SetUnionField(Value* this, size_t tag, Value* field)
{
  assert(IsUnionValue(this));
  UnionValue* union_value = (UnionValue*)this;
  union_value->tag = tag;
  union_value->field = field;
}
