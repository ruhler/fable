// Value.c --
//
//   This file implements routines for manipulating  values.

#include "Internal.h"

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
  assert(src->refcount > 0 && "refcount overflow");
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
      if (value->type->tag == STRUCT_DECL) {
        StructValue* struct_value = (StructValue*)value;
        for (int i = 0; i < value->type->fieldc; i++) {
          Release(struct_value->fieldv[i]);
        }
        FREE(struct_value->fieldv);
      } else {
        assert(value->type->tag == UNION_DECL);
        UnionValue* union_value = (UnionValue*)value;
        Release(union_value->field);
      }
      FREE(value);
    }
  }
}

// PrintValue --
//
//   Print a value in standard format to the given FILE stream.
//
// Inputs:
//   stream - The stream to print the value to.
//   value - The value to print.
//
// Result:
//   None.
//
// Side effects:
//   The value is printed to the given file stream.

void PrintValue(FILE* stream, Value* value)
{
  TypeDecl* type = value->type;
  if (type->tag == STRUCT_DECL) {
    StructValue* struct_value = (StructValue*)value;
    fprintf(stream, "%s(", type->name.name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintValue(stream, struct_value->fieldv[i]);
    }
    fprintf(stream, ")");
  } else if (type->tag == UNION_DECL) {
    UnionValue* union_value = (UnionValue*)value;
    fprintf(stream, "%s:%s(",
        type->name.name, type->fieldv[union_value->tag].name.name);
    PrintValue(stream, union_value->field);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// TagForField --
//
//   Return the tag corresponding to the named field for the given type.
//
// Inputs:
//   type - The type in question.
//   field - The field to get the tag for.
//
// Result:
//   The index in the list of fields for the type containing that field name,
//   or -1 if there is no such field.
//
// Side effects:
//   None.

int TagForField(TypeDecl* type, Name field)
{
  for (int i = 0; i < type->fieldc; i++) {
    if (NamesEqual(field, type->fieldv[i].name.name)) {
      return i;
    }
  }
  return -1;
}
