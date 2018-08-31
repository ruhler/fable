// value.c
//   This file implements routines associated with fble values.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

// FbleCopy -- see documentation in fble.h
FbleValue* FbleCopy(FbleArena* arena, FbleValue* src)
{
  if (src != NULL) {
    src->refcount++;
  }

  return src;
}

// FbleRelease -- see documentation in fble.h
void FbleRelease(FbleArena* arena, FbleValue* value)
{
  if (value == NULL) {
    return;
  }

  assert(value->refcount > 0);
  if (--(value->refcount) == 0) {
    switch (value->tag) {
      case FBLE_STRUCT_VALUE: {
        FbleStructValue* sv = (FbleStructValue*)value;
        for (size_t i = 0; i < sv->fields.size; ++i) {
          FbleRelease(arena, sv->fields.xs[i]);
        }
        FbleFree(arena, sv->fields.xs);
        FbleFree(arena, value);
        return;
      }

      case FBLE_UNION_VALUE: {
        FbleUnionValue* uv = (FbleUnionValue*)value;
        FbleRelease(arena, uv->arg);
        FbleFree(arena, value);
        return;
      }

      case FBLE_FUNC_VALUE: {
        FbleFuncValue* fv = (FbleFuncValue*)value;
        FbleFreeFuncValue(arena, fv);
        return;
      }

      case FBLE_PROC_VALUE: assert(false && "TODO: release FBLE_PROC_VALUE"); return;

      case FBLE_INPUT_VALUE: assert(false && "TODO: release FBLE_INPUT_VALUE"); return;

      case FBLE_OUTPUT_VALUE: assert(false && "TODO: release FBLE_OUTPUT_VALUE"); return;

      default: {
        UNREACHABLE("invalid value tag");
        return;
      }
    }

    UNREACHABLE("Should not get here");
  }
}
