// FblcValue.c --
//
//   This file implements routines for manipulating Fblc values.

#include "FblcInternal.h"


// FblcNewStructValue --
//
//  Allocate a new struct value of the given type.
//
// Inputs:
//   type - The type of the struct value to allocate.
//
// Results:
//   A newly allocated struct value of the given type. The fields of the
//   struct are left uninitialized.
//
// Side effects:
//   Allocates a new struct value. Use FblcCopy to make a (shared) copy of the
//   struct value, and FblcRelease to release the resources associated with
//   the value.

FblcStructValue* FblcNewStructValue(FblcType* type)
{
  assert(type->kind == FBLC_KIND_STRUCT);
  FblcStructValue* value = MALLOC(sizeof(FblcStructValue));
  value->refcount = 1;
  value->fieldv = MALLOC(type->fieldc * sizeof(FblcValue*));
  value->type = type;
  return value;
}

// FblcNewUnionValue --
//
//  Allocate a new union value of the given type.
//
// Inputs:
//   type - The type of the union value to allocate.
//
// Results:
//   A newly allocated union value of the given type. The field and tag of the
//   union are left uninitialized.
//
// Side effects:
//   Allocates a new union value. Use FblcCopy to make a (shared) copy of the
//   union value, and FblcRelease to release the resources associated with
//   the value.

FblcUnionValue* FblcNewUnionValue(FblcType* type)
{
  assert(type->kind == FBLC_KIND_UNION);
  FblcUnionValue* value = MALLOC(sizeof(FblcUnionValue));
  value->refcount = 1;
  value->type = type;
  return value;
}

// FblcCopy --
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

FblcValue* FblcCopy(FblcValue* src)
{
  src->refcount++;
  assert(src->refcount > 0 && "refcount overflow");
  return src;
}

// FblcRelease --
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

void FblcRelease(FblcValue* value)
{
  if (value != NULL) {
    value->refcount--;
    if (value->refcount == 0) {
      if (value->type->kind == FBLC_KIND_STRUCT) {
        FblcStructValue* struct_value = (FblcStructValue*)value;
        for (int i = 0; i < value->type->fieldc; i++) {
          FblcRelease(struct_value->fieldv[i]);
        }
        FREE(struct_value->fieldv);
      } else {
        assert(value->type->kind == FBLC_KIND_UNION);
        FblcUnionValue* union_value = (FblcUnionValue*)value;
        FblcRelease(union_value->field);
      }
      FREE(value);
    }
  }
}

// FblcPrintValue --
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

void FblcPrintValue(FILE* stream, FblcValue* value)
{
  FblcType* type = value->type;
  if (type->kind == FBLC_KIND_STRUCT) {
    FblcStructValue* struct_value = (FblcStructValue*)value;
    fprintf(stream, "%s(", type->name.name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcPrintValue(stream, struct_value->fieldv[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_KIND_UNION) {
    FblcUnionValue* union_value = (FblcUnionValue*)value;
    fprintf(stream, "%s:%s(",
        type->name.name, type->fieldv[union_value->tag].name.name);
    FblcPrintValue(stream, union_value->field);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcTagForField --
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

int FblcTagForField(const FblcType* type, FblcName field)
{
  for (int i = 0; i < type->fieldc; i++) {
    if (FblcNamesEqual(field, type->fieldv[i].name.name)) {
      return i;
    }
  }
  return -1;
}
