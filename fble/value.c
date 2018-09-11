// value.c
//   This file implements routines associated with fble values.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

static void FreeValue(FbleArena* arena, FbleValue* value);

// FbleTakeStrongRef -- see documentation in fble.h
FbleValue* FbleTakeStrongRef(FbleValue* value)
{
  if (value != NULL) {
    if (value->strong_ref_count++ == 0) {
      switch (value->tag) {
        case FBLE_STRUCT_VALUE: {
          FbleStructValue* sv = (FbleStructValue*)value;
          for (size_t i = 0; i < sv->fields.size; ++i) {
            FbleTakeStrongRef(sv->fields.xs[i]);
          }
          break;
        }

        case FBLE_UNION_VALUE: {
          FbleUnionValue* uv = (FbleUnionValue*)value;
          FbleTakeStrongRef(uv->arg);
          break;
        }

        case FBLE_FUNC_VALUE: {
          FbleFuncValue* fv = (FbleFuncValue*)value;
          FbleVStack* vs = fv->context;
          while (vs != NULL) {
            FbleTakeStrongRef(vs->value);
            vs = vs->tail;
          }
          break;
        }

        case FBLE_PROC_VALUE: assert(false && "TODO"); break;
        case FBLE_INPUT_VALUE: assert(false && "TODO"); break;
        case FBLE_OUTPUT_VALUE: assert(false && "TODO"); break;

        case FBLE_REF_VALUE: {
          FbleRefValue* rv = (FbleRefValue*)value;
          FbleTakeWeakRef(rv->value);
          break;
        }
      }
    }
  }

  return value;
}

// FbleTakeWeakRef -- see documentation in fble.h
FbleValue* FbleTakeWeakRef(FbleValue* value)
{
  if (value != NULL) {
    if (value->weak_ref_count++ == 0) {
      switch (value->tag) {
        case FBLE_STRUCT_VALUE: {
          FbleStructValue* sv = (FbleStructValue*)value;
          for (size_t i = 0; i < sv->fields.size; ++i) {
            FbleTakeWeakRef(sv->fields.xs[i]);
          }
          break;
        }

        case FBLE_UNION_VALUE: {
          FbleUnionValue* uv = (FbleUnionValue*)value;
          FbleTakeWeakRef(uv->arg);
          break;
        }

        case FBLE_FUNC_VALUE: {
          FbleFuncValue* fv = (FbleFuncValue*)value;
          FbleVStack* vs = fv->context;
          while (vs != NULL) {
            FbleTakeWeakRef(vs->value);
            vs = vs->tail;
          }
          break;
        }

        case FBLE_PROC_VALUE: assert(false && "TODO"); break;
        case FBLE_INPUT_VALUE: assert(false && "TODO"); break;
        case FBLE_OUTPUT_VALUE: assert(false && "TODO"); break;

        case FBLE_REF_VALUE: {
          // Nothing to do here. 
          break;
        }
      }
    }
  }

  return value;
}

// FbleDropStrongRef -- see documentation in fble.h
void FbleDropStrongRef(FbleArena* arena, FbleValue* value)
{
  if (value == NULL) {
    return;
  }

  assert(value->strong_ref_count > 0);
  if (value->strong_ref_count == 1) {
    switch (value->tag) {
      case FBLE_STRUCT_VALUE: {
        FbleStructValue* sv = (FbleStructValue*)value;
        for (size_t i = 0; i < sv->fields.size; ++i) {
          FbleDropStrongRef(arena, sv->fields.xs[i]);
        }
        break;
      }

      case FBLE_UNION_VALUE: {
        FbleUnionValue* uv = (FbleUnionValue*)value;
        FbleDropStrongRef(arena, uv->arg);
        break;
      }

      case FBLE_FUNC_VALUE: {
        FbleFuncValue* fv = (FbleFuncValue*)value;
        FbleVStack* vs = fv->context;
        while (vs != NULL) {
          FbleDropStrongRef(arena, vs->value);
          vs = vs->tail;
        }
        break;
      }

      case FBLE_PROC_VALUE: assert(false && "TODO"); break;
      case FBLE_INPUT_VALUE: assert(false && "TODO"); break;
      case FBLE_OUTPUT_VALUE: assert(false && "TODO"); break;

      case FBLE_REF_VALUE: {
        FbleRefValue* rv = (FbleRefValue*)value;
        if (rv->strong) {
          FbleDropStrongRef(arena, rv->value);
        } else {
          FbleDropWeakRef(arena, rv->value);
        }
        break;
      }
    }
  }

  value->strong_ref_count--;
  if (value->strong_ref_count == 0 && value->weak_ref_count == 0) {
    FreeValue(arena, value);
  }
}

// FbleDropWeakRef -- see documentation in fble.h
void FbleDropWeakRef(FbleArena* arena, FbleValue* value)
{
  if (value == NULL) {
    return;
  }

  assert(value->weak_ref_count > 0);
  if (value->weak_ref_count == 1) {
    switch (value->tag) {
      case FBLE_STRUCT_VALUE: {
        FbleStructValue* sv = (FbleStructValue*)value;
        for (size_t i = 0; i < sv->fields.size; ++i) {
          FbleDropWeakRef(arena, sv->fields.xs[i]);
        }
        break;
      }

      case FBLE_UNION_VALUE: {
        FbleUnionValue* uv = (FbleUnionValue*)value;
        FbleDropWeakRef(arena, uv->arg);
        break;
      }

      case FBLE_FUNC_VALUE: {
        FbleFuncValue* fv = (FbleFuncValue*)value;
        FbleVStack* vs = fv->context;
        while (vs != NULL) {
          FbleDropWeakRef(arena, vs->value);
          vs = vs->tail;
        }
        break;
      }

      case FBLE_PROC_VALUE: assert(false && "TODO"); break;
      case FBLE_INPUT_VALUE: assert(false && "TODO"); break;
      case FBLE_OUTPUT_VALUE: assert(false && "TODO"); break;

      case FBLE_REF_VALUE: {
        // Nothing to do here.
        break;
      }
    }
  }

  value->weak_ref_count--;
  if (value->strong_ref_count == 0 && value->weak_ref_count == 0) {
    FreeValue(arena, value);
  }
}

// FreeValue --
//   Free resources associated with the given value.
//
// Inputs:
//   arena - arena the value was allocated with.
//   value - the value to free resources for.
//
// Results:
//   None.
//
// Side effects:
//   Frees resources associated with the given value.
static void FreeValue(FbleArena* arena, FbleValue* value)
{
  assert(value->strong_ref_count == 0 && value->weak_ref_count == 0);
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      FbleFree(arena, sv->fields.xs);
      FbleFree(arena, value);
      return;
    }

    case FBLE_UNION_VALUE: {
      FbleFree(arena, value);
      return;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      FbleVStack* vs = fv->context;
      while (vs != NULL) {
        FbleVStack* tmp = vs;
        vs = vs->tail;
        FbleFree(arena, tmp);
      }
      FbleFree(arena, fv);
      return;
    }

    case FBLE_PROC_VALUE: assert(false && "TODO"); return;
    case FBLE_INPUT_VALUE: assert(false && "TODO"); return;
    case FBLE_OUTPUT_VALUE: assert(false && "TODO"); return;

    case FBLE_REF_VALUE: {
      FbleFree(arena, value);
      return;
    }
  }

  UNREACHABLE("Should not get here");
}
