
#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fblc.h"

#define UNREACHABLE(x) assert(false && x)


// FblcNewStruct -- See documentation in fblc.h.
FblcValue* FblcNewStruct(FblcArena* arena, size_t fieldc)
{
  FblcValue* value = arena->alloc(arena, sizeof(FblcValue) + fieldc * sizeof(FblcValue*), FBLC_ALLOC_MSG(__FILE__, __LINE__));
  value->refcount = 1;
  value->kind = FBLC_STRUCT_KIND;
  value->fieldc = fieldc;
  value->tag = -1;
  return value;
}

// FblcNewUnion -- see documentation in fblc.h.
FblcValue* FblcNewUnion(FblcArena* arena, size_t fieldc, FblcFieldId tag, FblcValue* field)
{
  FblcValue* value = arena->alloc(arena, sizeof(FblcValue) + sizeof(FblcValue*), FBLC_ALLOC_MSG(__FILE__, __LINE__));
  value->refcount = 1;
  value->kind = FBLC_UNION_KIND;
  value->fieldc = fieldc;
  value->tag = tag;
  value->fields[0] = field;
  return value;
}

// FblcCopy -- see documentation in fblc.h
FblcValue* FblcCopy(FblcArena* arena, FblcValue* src)
{
  src->refcount++;
  return src;
}

// FblcRelease -- see documentation in fblc.h
void FblcRelease(FblcArena* arena, FblcValue* value)
{
  if (value != NULL) {
    value->refcount--;
    if (value->refcount == 0) {
      switch (value->kind) {
        case FBLC_STRUCT_KIND:
          for (size_t i = 0; i < value->fieldc; ++i) {
            FblcRelease(arena, value->fields[i]);
          }
          break;

        case FBLC_UNION_KIND:
          FblcRelease(arena, *value->fields);
          break;

        default:
          UNREACHABLE("Invalid value kind");
          break;
      }
      arena->free(arena, value);
    }
  }
}
