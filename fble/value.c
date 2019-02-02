// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

static void FreeValue(FbleValueArena* arena, FbleValue* value);


// FbleNewValueArena -- see documentation in fble-.h
FbleValueArena* FbleNewValueArena(FbleArena* arena)
{
  return FbleNewRefArena(arena, NULL, NULL);
}

// FbleDeleteValueArena -- see documentation in fble.h
void FbleDeleteValueArena(FbleValueArena* arena)
{
  FbleDeleteRefArena(arena);
}

// FbleValueRetain -- see documentation in fble.h
FbleValue* FbleValueRetain(FbleValueArena* arena, FbleValue* value)
{
  if (value != NULL) {
    assert(value->strong_ref_count > 0);
    value->strong_ref_count++;
  }
  return value;
}

// FbleValueRelease -- see documentation in fble.h
void FbleValueRelease(FbleValueArena* arena, FbleValue* value)
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
static void FreeValue(FbleValueArena* arena, FbleValue* value)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      FbleFree(arena_, sv->fields.xs);
      FbleFree(arena_, value);
      return;
    }

    case FBLE_UNION_VALUE: {
      FbleFree(arena_, value);
      return;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      FbleVStack* vs = fv->context;
      while (vs != NULL) {
        FbleVStack* tmp = vs;
        vs = vs->tail;
        FbleFree(arena_, tmp);
      }
      FbleFreeInstrs(arena_, fv->body);
      FbleFree(arena_, fv);
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
            FbleFree(arena_, tmp);
          }
          FbleFreeInstrs(arena_, v->body);
          break;
        }

        case FBLE_EXEC_PROC_VALUE: {
          FbleExecProcValue* v = (FbleExecProcValue*)value;
          FbleFree(arena_, v->bindings.xs);
          FbleVStack* vs = v->context;
          while (vs != NULL) {
            FbleVStack* tmp = vs;
            vs = vs->tail;
            FbleFree(arena_, tmp);
          }
          FbleFreeInstrs(arena_, v->body);
          break;
        }
      }

      FbleFree(arena_, value);
      return;
    }

    case FBLE_INPUT_VALUE: {
      FbleInputValue* v = (FbleInputValue*)value;
      FbleValues* curr = v->head;
      while (curr != NULL) {
        FbleValues* tmp = curr;
        curr = curr->next;
        FbleFree(arena_, tmp);
      }
      FbleFree(arena_, value);
      return;
    }

    case FBLE_OUTPUT_VALUE: {
      FbleFree(arena_, value);
      return;
    }

    case FBLE_REF_VALUE: {
      FbleFree(arena_, value);
      return;
    }
  }

  UNREACHABLE("Should not get here");
}

// FbleNewStructValue -- see documentation in fble.h
FbleValue* FbleNewStructValue(FbleValueArena* arena, FbleValueV* args)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleStructValue* value = FbleAlloc(arena_, FbleStructValue);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->_base.strong_ref_count = 1;
  value->fields.size = args->size;
  value->fields.xs = FbleArenaAlloc(arena_, value->fields.size * sizeof(FbleValue*), FbleAllocMsg(__FILE__, __LINE__));

  for (size_t i = 0; i < args->size; ++i) {
    value->fields.xs[i] = FbleValueRetain(arena, args->xs[i]);
  }
  return &value->_base;
}

// FbleNewUnionValue -- see documentation in fble.h
FbleValue* FbleNewUnionValue(FbleValueArena* arena, size_t tag, FbleValue* arg)
{
  FbleUnionValue* union_value = FbleAlloc(FbleRefArenaArena(arena), FbleUnionValue);
  union_value->_base.tag = FBLE_UNION_VALUE;
  union_value->_base.strong_ref_count = 1;
  union_value->tag = tag;
  union_value->arg = FbleValueRetain(arena, arg);
  return &union_value->_base;
}
