// value.c
//   This file implements routines associated with fble values.

#include "fble-internal.h"

#define UNREACHABLE(x) assert(false && x)

static void DropBreakCycleRef(FbleValue* value);
static void BreakCycle(FbleArena* arena, FbleValue* value);
static void UnBreakCycle(FbleValue* value);
static void FreeValue(FbleArena* arena, FbleValue* value);

// FbleTakeStrongRef -- see documentation in fble.h
FbleValue* FbleTakeStrongRef(FbleValue* value)
{
  if (value != NULL) {
    assert(value->strong_ref_count > 0);
    if (value->strong_ref_count++ == value->break_cycle_ref_count) {
      UnBreakCycle(value);
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

      case FBLE_PROC_VALUE: {
        FbleProcValue* pv = (FbleProcValue*)value;
        switch (pv->tag) {
          case FBLE_GET_PROC_VALUE: {
            FbleGetProcValue* get = (FbleGetProcValue*)value;
            FbleDropStrongRef(arena, get->port);
            break;
          }

          case FBLE_PUT_PROC_VALUE: {
            FblePutProcValue* put = (FblePutProcValue*)value;
            FbleDropStrongRef(arena, put->port);
            FbleDropStrongRef(arena, put->arg);
            break;
          }

          case FBLE_EVAL_PROC_VALUE: {
            FbleEvalProcValue* eval = (FbleEvalProcValue*)value;
            FbleDropStrongRef(arena, eval->result);
            break;
          }

          case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* v = (FbleLinkProcValue*)value;
            for (FbleVStack* vs = v->context; vs != NULL; vs = vs->tail) {
              FbleDropStrongRef(arena, vs->value);
            }
            break;
          }

          case FBLE_EXEC_PROC_VALUE: {
            FbleExecProcValue* v = (FbleExecProcValue*)value;
            for (size_t i = 0; i < v->bindings.size; ++i) {
              FbleDropStrongRef(arena, v->bindings.xs[i]);
            }
            for (FbleVStack* vs = v->context; vs != NULL; vs = vs->tail) {
              FbleDropStrongRef(arena, vs->value);
            }
            break;
          }
        }
        break;
      }

      case FBLE_INPUT_VALUE: {
        FbleInputValue* v = (FbleInputValue*)value;
        for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
          FbleDropStrongRef(arena, elem->value);
        }
        break;
      }

      case FBLE_OUTPUT_VALUE: {
        FbleOutputValue* v = (FbleOutputValue*)value;
        FbleDropStrongRef(arena, &v->dest->_base);
        break;
      }

      case FBLE_REF_VALUE: {
        FbleRefValue* rv = (FbleRefValue*)value;
        if (rv->broke_cycle) {
          DropBreakCycleRef(rv->value);
        }
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
    UnBreakCycle(value);
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

    case FBLE_PROC_VALUE: {
      FbleProcValue* pv = (FbleProcValue*)value;
      switch (pv->tag) {
        case FBLE_GET_PROC_VALUE: {
          FbleGetProcValue* get = (FbleGetProcValue*)value;
          FbleBreakCycleRef(arena, get->port);
          break;
        }

        case FBLE_PUT_PROC_VALUE: {
          FblePutProcValue* put = (FblePutProcValue*)value;
          FbleBreakCycleRef(arena, put->port);
          FbleBreakCycleRef(arena, put->arg);
          break;
        }

        case FBLE_EVAL_PROC_VALUE: {
          FbleEvalProcValue* eval = (FbleEvalProcValue*)value;
          FbleBreakCycleRef(arena, eval->result);
          break;
        }

        case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* v = (FbleLinkProcValue*)value;
            FbleVStack* vs = v->context;
            while (vs != NULL) {
              FbleBreakCycleRef(arena, vs->value);
              vs = vs->tail;
            }
            break;
        }

        case FBLE_EXEC_PROC_VALUE: {
          FbleExecProcValue* v = (FbleExecProcValue*)value;
          for (size_t i = 0; i < v->bindings.size; ++i) {
            FbleBreakCycleRef(arena, v->bindings.xs[i]);
          }
          for (FbleVStack* vs = v->context; vs != NULL; vs = vs->tail) {
            FbleBreakCycleRef(arena, vs->value);
          }
          break;
        }
      }
      break;
    }

    case FBLE_INPUT_VALUE: {
      FbleInputValue* v = (FbleInputValue*)value;
      for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
        FbleBreakCycleRef(arena, elem->value);
      }
      break;
    }

    case FBLE_OUTPUT_VALUE: {
      FbleOutputValue* v = (FbleOutputValue*)value;
      FbleBreakCycleRef(arena, &v->dest->_base);
      break;
    }

    case FBLE_REF_VALUE: {
      FbleRefValue* rv = (FbleRefValue*)value;

      // Check if all siblings can be broken now.
      bool can_break = true;
      FbleRefValue* sibling = rv;
      do {
        if (sibling->_base.strong_ref_count != sibling->_base.break_cycle_ref_count) {
          can_break = false;
          break;
        }
        sibling = sibling->siblings;
      } while (sibling != rv);

      if (can_break) {
        // Break the references of all the siblings.
        // 1. Grab strong references to all the siblings to make sure they
        // don't get freed out from under us.
        sibling = rv;
        do {
          FbleTakeStrongRef(&sibling->_base);
          sibling = sibling->siblings;
        } while (sibling != rv);

        // 2. Break the references of all the siblings.
        sibling = rv;
        do {
          FbleValue* value = sibling->value;
          sibling->value = NULL;
          if (sibling->broke_cycle) {
            DropBreakCycleRef(value);
          }
          FbleDropStrongRef(arena, value);
          sibling = sibling->siblings;
        } while (sibling != rv);

        // 3. Release our strong references to the siblings.
        while (rv->siblings != rv) {
          FbleDropStrongRef(arena, &(rv->siblings->_base));
        }
        FbleDropStrongRef(arena, &rv->_base);
      }
      break;
    }
  }
}

// UnBreakCycle --
//   Called when the strong ref count and break cycle ref count of value
//   become unequal.
//
// Inputs:
//   value - the value to unbreak the cycle of.
//
// Results: 
//   none.
//
// Side effects:
//   Propagates the unbroken cycle to references.
static void UnBreakCycle(FbleValue* value)
{
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

    case FBLE_PROC_VALUE: {
      FbleProcValue* pv = (FbleProcValue*)value;
      switch (pv->tag) {
        case FBLE_GET_PROC_VALUE: {
          FbleGetProcValue* get = (FbleGetProcValue*)value;
          DropBreakCycleRef(get->port);
          break;
        }

        case FBLE_PUT_PROC_VALUE: {
          FblePutProcValue* put = (FblePutProcValue*)value;
          DropBreakCycleRef(put->port);
          DropBreakCycleRef(put->arg);
          break;
        }

        case FBLE_EVAL_PROC_VALUE: {
          FbleEvalProcValue* eval = (FbleEvalProcValue*)value;
          DropBreakCycleRef(eval->result);
          break;
        }

        case FBLE_LINK_PROC_VALUE: {
            FbleLinkProcValue* v = (FbleLinkProcValue*)value;
            FbleVStack* vs = v->context;
            while (vs != NULL) {
              DropBreakCycleRef(vs->value);
              vs = vs->tail;
            }
            break;
        }

        case FBLE_EXEC_PROC_VALUE: {
          FbleExecProcValue* v = (FbleExecProcValue*)value;
          for (size_t i = 0; i < v->bindings.size; ++i) {
            DropBreakCycleRef(v->bindings.xs[i]);
          }
          for (FbleVStack* vs = v->context; vs != NULL; vs = vs->tail) {
            DropBreakCycleRef(vs->value);
          }
          break;
        }
      }
      break;
    }

    case FBLE_INPUT_VALUE: {
      FbleInputValue* v = (FbleInputValue*)value;
      for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
        DropBreakCycleRef(elem->value);
      }
      break;
    }

    case FBLE_OUTPUT_VALUE: {
      FbleOutputValue* v = (FbleOutputValue*)value;
      DropBreakCycleRef(&v->dest->_base);
      break;
    }

    case FBLE_REF_VALUE: {
      // Nothing to do here. 
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
      break;
    }

    case FBLE_OUTPUT_VALUE: {
      FbleFree(arena, value);
      break;
    }

    case FBLE_REF_VALUE: {
      FbleRefValue* rv = (FbleRefValue*)value;

      // Remove this ref from its list of siblings.
      FbleRefValue* sibling = rv->siblings;
      while (sibling->siblings != rv) {
        sibling = sibling->siblings;
      }
      sibling->siblings = rv->siblings;

      FbleFree(arena, value);
      return;
    }
  }

  UNREACHABLE("Should not get here");
}
