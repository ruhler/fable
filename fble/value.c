// value.c
//   This file implements routines associated with fble values.

#include "fble.h"

#define UNREACHABLE(x) assert(false && x)

// FbleCopy -- see documentation in fble.h
FbleValue* FbleCopy(FbleArena* arena, FbleValue* src)
{
  src->refcount++;
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
      case FBLE_TYPE_TYPE_VALUE: FbleFree(arena, value); return;
      case FBLE_FUNC_TYPE_VALUE: assert(false && "TODO: release FBLE_FUNC_TYPE_VALUE"); return;
      case FBLE_FUNC_VALUE: assert(false && "TODO: release FBLE_FUNC_VALUE"); return;
      case FBLE_STRUCT_TYPE_VALUE: assert(false && "TODO: release FBLE_STRUCT_TYPE_VALUE"); return;
      case FBLE_STRUCT_VALUE: assert(false && "TODO: release FBLE_STRUCT_VALUE"); return;
      case FBLE_UNION_TYPE_VALUE: assert(false && "TODO: release FBLE_UNION_TYPE_VALUE"); return;
      case FBLE_UNION_VALUE: assert(false && "TODO: release FBLE_UNION_VALUE"); return;
      case FBLE_PROC_TYPE_VALUE: assert(false && "TODO: release FBLE_PROC_TYPE_VALUE"); return;
      case FBLE_INPUT_TYPE_VALUE: assert(false && "TODO: release FBLE_INPUT_TYPE_VALUE"); return;
      case FBLE_OUTPUT_TYPE_VALUE: assert(false && "TODO: release FBLE_OUTPUT_TYPE_VALUE"); return;
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
