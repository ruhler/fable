// FblcValue.c --
//
//   This file implements routines for manipulating Fblc values.

#include "FblcInternal.h"


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
//   The resources for the value are freed.

void FblcRelease(FblcValue* value)
{
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
