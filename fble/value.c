// value.c
//   This file implements routines associated with fble values.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

static void DropBreakCycleRef(FbleValue* value);
static void BreakCycle(FbleArena* arena, FbleValue* value);
static void FreeValue(FbleArena* arena, FbleValue* value);

// FbleTakeStrongRef -- see documentation in fble.h
FbleValue* FbleTakeStrongRef(FbleValue* value)
{
  if (value != NULL) {
    assert(value->strong_ref_count > 0);
    if (value->strong_ref_count++ == value->break_cycle_ref_count) {
      switch (value->tag) {
        case FBLE_STRUCT_VALUE: {
          FbleStructValue* sv = (FbleStructValue*)value;
          for (size_t i = 0; i < sv->fields.size; ++i) {
            DropBreakCycleRef(sv->fields.xs[i]);
          }
          break;
        }

        case FBLE_UNION_VALUE: {
          FbleUnionValue* uv = (FbleUnionValue*)value;
          DropBreakCycleRef(uv->arg);
          break;
        }

        case FBLE_FUNC_VALUE: {
          FbleFuncValue* fv = (FbleFuncValue*)value;
          FbleVStack* vs = fv->context;
          while (vs != NULL) {
            DropBreakCycleRef(vs->value);
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

// FbleBreakCycleRef -- see documentation in fble.h
FbleValue* FbleBreakCycleRef(FbleArena* arena, FbleValue* value)
{
  if (value != NULL) {
    value->break_cycle_ref_count++;
    if (value->break_cycle_ref_count == value->strong_ref_count) {
      BreakCycle(arena, value);
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
        FbleDropStrongRef(arena, rv->value);
        break;
      }
    }

    FreeValue(arena, value);
  } else {
    value->strong_ref_count--;
    if (value->strong_ref_count == value->break_cycle_ref_count) {
      BreakCycle(arena, value);
    }
  }
}

// DropBreakCycleRef --
//
//   Decrement the break cycle reference count of a value.
//
// Inputs:
//   value - The value to decrement the break cycle reference count of. The
//           value may be NULL, in which case no action is performed.
//
// Results:
//   None.
//
// Side effect:
//   Decrements the break cycle reference count of the value.
static void DropBreakCycleRef(FbleValue* value)
{
  if (value == NULL) {
    return;
  }

  assert(value->break_cycle_ref_count > 0);
  if (value->break_cycle_ref_count-- == value->strong_ref_count) {
    switch (value->tag) {
      case FBLE_STRUCT_VALUE: {
        FbleStructValue* sv = (FbleStructValue*)value;
        for (size_t i = 0; i < sv->fields.size; ++i) {
          DropBreakCycleRef(sv->fields.xs[i]);
        }
        break;
      }

      case FBLE_UNION_VALUE: {
        FbleUnionValue* uv = (FbleUnionValue*)value;
        DropBreakCycleRef(uv->arg);
        break;
      }

      case FBLE_FUNC_VALUE: {
        FbleFuncValue* fv = (FbleFuncValue*)value;
        FbleVStack* vs = fv->context;
        while (vs != NULL) {
          DropBreakCycleRef(vs->value);
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

// BreakCycle --
//   Called when the strong ref count and break cycle ref count of value
//   become equal.
//
// Inputs:
//   arena - arena for deallocation
//   value - the value to break the cycle of.
//
// Results: 
//   none.
//
// Side effects:
//   Propagates the broken cycle to references and, if appropriate, breaks the
//   cycle through ref values.
static void BreakCycle(FbleArena* arena, FbleValue* value)
{
  assert(value->strong_ref_count > 0);
  assert(value->strong_ref_count == value->break_cycle_ref_count);
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      for (size_t i = 0; i < sv->fields.size; ++i) {
        FbleBreakCycleRef(arena, sv->fields.xs[i]);
      }
      break;
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)value;
      FbleBreakCycleRef(arena, uv->arg);
      break;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      FbleVStack* vs = fv->context;
      while (vs != NULL) {
        FbleBreakCycleRef(arena, vs->value);
        vs = vs->tail;
      }
      break;
    }

    case FBLE_PROC_VALUE: assert(false && "TODO"); break;
    case FBLE_INPUT_VALUE: assert(false && "TODO"); break;
    case FBLE_OUTPUT_VALUE: assert(false && "TODO"); break;

    case FBLE_REF_VALUE: {
      // TODO: drop a strong ref to this value if all sibling refs are fully
      // broken.
      break;
    }
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
