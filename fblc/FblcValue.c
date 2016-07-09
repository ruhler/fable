// FblcValue.c --
//
//   This file implements routines for manipulating Fblc values.

#include "FblcInternal.h"


// FblcNewValue --
//
//   Create an uninitialized new value of the given type.
//
// Inputs:
//   type - The type of the value to create.
//
// Result:
//   The newly created value. The value with have the type field set, but the
//   fields will be uninitialized.
//
// Side effects:
//   None.

FblcValue* FblcNewValue(FblcType* type)
{
  int fieldc = type->fieldc;
  if (type->kind == FBLC_KIND_UNION) {
    fieldc = 1;
  }
  FblcValue* value = GC_MALLOC(sizeof(FblcValue) + fieldc * sizeof(FblcValue*));
  value->type = type;
  return value;
}

// FblcNewUnionValue --
//
//   Create a new union value with the given type and tag.
//
// Inputs:
//   type - The type of the union value to create. This should be a union type.
//   tag - The tag of the union value to set.
//
// Result:
//   The new union value, with type and tag set. The field value will be
//   uninitialized.
//
// Side effects:
//   None.

FblcValue* FblcNewUnionValue(FblcType* type, int tag)
{
  assert(type->kind == FBLC_KIND_UNION);
  FblcValue* value = FblcNewValue(type);
  value->tag = tag;
  return value;
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
    fprintf(stream, "%s(", type->name.name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcPrintValue(stream, value->fieldv[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_KIND_UNION) {
    fprintf(stream, "%s:%s(",
        type->name.name, type->fieldv[value->tag].name.name);
    FblcPrintValue(stream, value->fieldv[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}
