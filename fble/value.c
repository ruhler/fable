// value.c
//   This file implements routines associated with fble values.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

static void FreeValue(FbleArena* arena, FbleValue* value);

// FbleValueRetain -- see documentation in fble.h
FbleValue* FbleValueRetain(FbleValue* value)
{
  if (value != NULL) {
    assert(value->strong_ref_count > 0);
    value->strong_ref_count++;
  }
  return value;
}

// FbleValueRelease -- see documentation in fble.h
void FbleValueRelease(FbleArena* arena, FbleValue* value)
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
          FbleValueRelease(arena, sv->fields.xs[i]);
        }
        break;
      }

      case FBLE_UNION_VALUE: {
        FbleUnionValue* uv = (FbleUnionValue*)value;
        FbleValueRelease(arena, uv->arg);
        break;
      }

      case FBLE_FUNC_VALUE: {
        FbleFuncValue* fv = (FbleFuncValue*)value;
        FbleVStack* vs = fv->context;
        while (vs != NULL) {
          FbleValueRelease(arena, vs->value);
          vs = vs->tail;
        }
        break;
      }

      case FBLE_PROC_VALUE: {
        FbleProcValue* pv = (FbleProcValue*)value;
        switch (pv->tag) {
          case FBLE_GET_PROC_VALUE: {
            FbleGetProcValue* get = (FbleGetProcValue*)value;
            FbleValueRelease(arena, get->port);
            break;
          }

          case FBLE_PUT_PROC_VALUE: {
            FblePutProcValue* put = (FblePutProcValue*)value;
            FbleValueRelease(arena, put->port);
            FbleValueRelease(arena, put->arg);
            break;
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)value;
            FbleValueRelease(arena, eval->result);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* v = (FbleLinkProcValue*)value;
            for (FbleVStack* vs = v->context; vs != NULL; vs = vs->tail) {
              FbleValueRelease(arena, vs->value);
            }
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* v = (FbleExecProcValue*)value;
            for (size_t i = 0; i < v->bindings.size; ++i) {
              FbleValueRelease(arena, v->bindings.xs[i]);
            }
            for (FbleVStack* vs = v->context; vs != NULL; vs = vs->tail) {
              FbleValueRelease(arena, vs->value);
            }
            break;
          }
        }
        break;
      }

      case FBLE_INPUT_VALUE: {
        FbleInputValue* v = (FbleInputValue*)value;
        for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
          FbleValueRelease(arena, elem->value);
        }
        break;
      }

      case FBLE_OUTPUT_VALUE: {
        FbleOutputValue* v = (FbleOutputValue*)value;
        FbleValueRelease(arena, &v->dest->_base);
        break;
      }

      case FBLE_REF_VALUE: {
        FbleRefValue* rv = (FbleRefValue*)value;
        FbleValueRelease(arena, rv->value);
        break;
      }
    }

    FreeValue(arena, value);
  } else {
    value->strong_ref_count--;
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
      FbleFreeInstrs(arena, fv->body);
      FbleFree(arena, fv);
      return;
    }

    case FBLE_PROC_VALUE: {
      FbleProcValue* pv = (FbleProcValue*)value;
      switch (pv->tag) {
        case FBLE_GET_PROC_VALUE: break;
        case FBLE_PUT_PROC_VALUE: break;
        case FBLE_EVAL_PROC_VALUE: break;

        case FBLE_LINK_PROC_VALUE: {
          FbleLinkProcValue* v = (FbleLinkProcValue*)value;
          FbleVStack* vs = v->context;
          while (vs != NULL) {
            FbleVStack* tmp = vs;
            vs = vs->tail;
            FbleFree(arena, tmp);
          }
          FbleFreeInstrs(arena, v->body);
          break;
        }

        case FBLE_EXEC_PROC_VALUE: {
          FbleExecProcValue* v = (FbleExecProcValue*)value;
          FbleFree(arena, v->bindings.xs);
          FbleVStack* vs = v->context;
          while (vs != NULL) {
            FbleVStack* tmp = vs;
            vs = vs->tail;
            FbleFree(arena, tmp);
          }
          FbleFreeInstrs(arena, v->body);
          break;
        }
      }

      FbleFree(arena, value);
      return;
    }

    case FBLE_INPUT_VALUE: {
      FbleInputValue* v = (FbleInputValue*)value;
      FbleValues* curr = v->head;
      while (curr != NULL) {
        FbleValues* tmp = curr;
        curr = curr->next;
        FbleFree(arena, tmp);
      }
      FbleFree(arena, value);
      return;
    }

    case FBLE_OUTPUT_VALUE: {
      FbleFree(arena, value);
      return;
    }

    case FBLE_REF_VALUE: {
      FbleFree(arena, value);
      return;
    }
  }

  UNREACHABLE("Should not get here");
}

// FbleNewStructValue -- see documentation in fble.h
FbleValue* FbleNewStructValue(FbleArena* arena, FbleValueV* args)
{
  FbleStructValue* value = FbleAlloc(arena, FbleStructValue);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->_base.strong_ref_count = 1;
  value->fields.size = args->size;
  value->fields.xs = FbleArenaAlloc(arena, value->fields.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));

  for (size_t i = 0; i < args->size; ++i) {
    value->fields.xs[i] = FbleValueRetain(args->xs[i]);
  }
  return &value->_base;
}

// FbleNewUnionValue -- see documentation in fble.h
FbleValue* FbleNewUnionValue(FbleArena* arena, size_t tag, FbleValue* arg)
{
  FbleUnionValue* union_value = FbleAlloc(arena, FbleUnionValue);
  union_value->_base.tag = FBLE_UNION_VALUE;
  union_value->_base.strong_ref_count = 1;
  union_value->tag = tag;
  union_value->arg = FbleValueRetain(arg);
  return &union_value->_base;
}
