// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "internal.h"

#define UNREACHABLE(x) assert(false && x)

static void ValueFree(FbleValueArena* arena, FbleRef* ref);
static void Add(FbleRefCallback* add, FbleValue* value);
static void ValueAdded(FbleRefCallback* add, FbleRef* ref);


// FbleNewValueArena -- see documentation in fble-.h
FbleValueArena* FbleNewValueArena(FbleArena* arena)
{
  return FbleNewRefArena(arena, &ValueFree, &ValueAdded);
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
    FbleRefRetain(arena, &value->ref);
  }
  return value;
}

// FbleValueRelease -- see documentation in fble.h
void FbleValueRelease(FbleValueArena* arena, FbleValue* value)
{
  if (value != NULL) {
    FbleRefRelease(arena, &value->ref);
  }
}

// ValueFree --
//   The 'free' function for values. See documentation in ref.h
static void ValueFree(FbleValueArena* arena, FbleRef* ref)
{
  FbleValue* value = (FbleValue*)ref;
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
      switch (fv->tag) {
        case FBLE_BASIC_FUNC_VALUE: {
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)fv;
          FbleFree(arena_, basic->scope.xs);
          FbleFreeInstrBlock(arena_, basic->body);
          break;
        }

        case FBLE_THUNK_FUNC_VALUE: break;
        case FBLE_PUT_FUNC_VALUE: break;
      }
      FbleFree(arena_, value);
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
          FbleFree(arena_, v->scope.xs);
          FbleFreeInstrBlock(arena_, v->body);
          break;
        }

        case FBLE_EXEC_PROC_VALUE: {
          FbleExecProcValue* v = (FbleExecProcValue*)value;
          FbleFree(arena_, v->bindings.xs);
          FbleFree(arena_, v->scope.xs);
          FbleFreeInstrBlock(arena_, v->body);
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

    case FBLE_PORT_VALUE: {
      FbleFree(arena_, value);
      return;
    }

    case FBLE_REF_VALUE: {
      FbleFree(arena_, value);
      return;
    }

    case FBLE_TYPE_VALUE: {
      FbleFree(arena_, value);
      return;
    }
  }

  UNREACHABLE("Should not get here");
}

// Add --
//   Helper function for implementing ValueAdded. Call the add callback
//   if the value is not null.
//
// Inputs:
//   add - the ref callback.
//   value - the value to add.
//
// Results:
//   none.
//
// Side effects:
//   If value is non-null, the add callback is called for it.
static void Add(FbleRefCallback* add, FbleValue* value)
{
  if (value != NULL) {
    add->callback(add, &value->ref);
  }
}

// ValueAdded --
//   The 'added' function for values. See documentation in ref.h
static void ValueAdded(FbleRefCallback* add, FbleRef* ref)
{
  FbleValue* value = (FbleValue*)ref;
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      for (size_t i = 0; i < sv->fields.size; ++i) {
        Add(add, sv->fields.xs[i]);
      }
      break;
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)value;
      Add(add, uv->arg);
      break;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      switch (fv->tag) {
        case FBLE_BASIC_FUNC_VALUE: {
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)fv;
          for (size_t i = 0; i < basic->scope.size; ++i) {
            Add(add, basic->scope.xs[i]);
          }
          break;
        }

        case FBLE_THUNK_FUNC_VALUE: {
          FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)fv;
          Add(add, &thunk->func->_base);
          Add(add, thunk->arg);
          break;
        }

        case FBLE_PUT_FUNC_VALUE: {
          FblePutFuncValue* put = (FblePutFuncValue*)fv;
          Add(add, put->port);
          break;
        }
      }
      break;
    }

    case FBLE_PROC_VALUE: {
      FbleProcValue* pv = (FbleProcValue*)value;
      switch (pv->tag) {
        case FBLE_GET_PROC_VALUE: {
          FbleGetProcValue* get = (FbleGetProcValue*)value;
          Add(add, get->port);
          break;
        }

        case FBLE_PUT_PROC_VALUE: {
          FblePutProcValue* put = (FblePutProcValue*)value;
          Add(add, put->port);
          Add(add, put->arg);
          break;
        }

        case FBLE_EVAL_PROC_VALUE: {
          FbleEvalProcValue* eval = (FbleEvalProcValue*)value;
          Add(add, eval->result);
          break;
        }

        case FBLE_LINK_PROC_VALUE: {
          FbleLinkProcValue* v = (FbleLinkProcValue*)value;
          for (size_t i = 0; i < v->scope.size; ++i) {
            Add(add, v->scope.xs[i]);
          }
          break;
        }

        case FBLE_EXEC_PROC_VALUE: {
          FbleExecProcValue* v = (FbleExecProcValue*)value;
          for (size_t i = 0; i < v->bindings.size; ++i) {
            Add(add, v->bindings.xs[i]);
          }
          for (size_t i = 0; i < v->scope.size; ++i) {
            Add(add, v->scope.xs[i]);
          }
          break;
        }
      }
      break;
    }

    case FBLE_INPUT_VALUE: {
      FbleInputValue* v = (FbleInputValue*)value;
      for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
        Add(add, elem->value);
      }
      break;
    }

    case FBLE_OUTPUT_VALUE: {
      FbleOutputValue* v = (FbleOutputValue*)value;
      Add(add, &v->dest->_base);
      break;
    }

    case FBLE_PORT_VALUE: {
      break;
    }

    case FBLE_REF_VALUE: {
      FbleRefValue* rv = (FbleRefValue*)value;
      Add(add, rv->value);
      break;
    }

    case FBLE_TYPE_VALUE: {
      break;
    }
  }
}

// FbleNewStructValue -- see documentation in fble.h
FbleValue* FbleNewStructValue(FbleValueArena* arena, FbleValueV args)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FbleStructValue* value = FbleAlloc(arena_, FbleStructValue);
  FbleRefInit(arena, &value->_base.ref);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fields.size = args.size;
  value->fields.xs = FbleArrayAlloc(arena_, FbleValue*, value->fields.size);

  for (size_t i = 0; i < args.size; ++i) {
    value->fields.xs[i] = args.xs[i];
    FbleRefAdd(arena, &value->_base.ref, &args.xs[i]->ref);
    FbleValueRelease(arena, args.xs[i]);
  }
  return &value->_base;
}

// FbleStructValueAccess -- see documentation in fble-value.h
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field)
{
  assert(object->tag == FBLE_STRUCT_VALUE);
  FbleStructValue* value = (FbleStructValue*)object;
  assert(field < value->fields.size);
  return value->fields.xs[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue* FbleNewUnionValue(FbleValueArena* arena, size_t tag, FbleValue* arg)
{
  FbleUnionValue* union_value = FbleAlloc(FbleRefArenaArena(arena), FbleUnionValue);
  FbleRefInit(arena, &union_value->_base.ref);
  union_value->_base.tag = FBLE_UNION_VALUE;
  union_value->tag = tag;
  union_value->arg = arg;
  FbleRefAdd(arena, &union_value->_base.ref, &arg->ref);
  FbleValueRelease(arena, arg);
  return &union_value->_base;
}

// FbleUnionValueTag -- see documentation in fble-value.h
size_t FbleUnionValueTag(FbleValue* object)
{
  assert(object->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue* FbleUnionValueAccess(FbleValue* object)
{
  assert(object->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->arg;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue* value)
{
  return value->tag == FBLE_PROC_VALUE;
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewInputPortValue(FbleValueArena* arena, size_t id)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);
  FblePortValue* get_port = FbleAlloc(arena_, FblePortValue);
  FbleRefInit(arena, &get_port->_base.ref);
  get_port->_base.tag = FBLE_PORT_VALUE;
  get_port->id = id;

  FbleGetProcValue* get = FbleAlloc(arena_, FbleGetProcValue);
  FbleRefInit(arena, &get->_base._base.ref);
  get->_base._base.tag = FBLE_PROC_VALUE;
  get->_base.tag = FBLE_GET_PROC_VALUE;
  get->port = &get_port->_base;
  FbleRefAdd(arena, &get->_base._base.ref, &get->port->ref);
  FbleValueRelease(arena, get->port);
  return &get->_base._base;
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewOutputPortValue(FbleValueArena* arena, size_t id)
{
  FblePortValue* port_value = FbleAlloc(FbleRefArenaArena(arena), FblePortValue);
  FbleRefInit(arena, &port_value->_base.ref);
  port_value->_base.tag = FBLE_PORT_VALUE;
  port_value->id = id;
  return &port_value->_base;
}
