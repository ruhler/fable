// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static FbleInstr g_get_instr = { .tag = FBLE_GET_INSTR };
static FbleInstr* g_get_block_instrs[] = { &g_get_instr };
static FbleInstrBlock g_get_block = {
  .refcount = 1,
  .statics = 1,  // port
  .locals = 0,
  .instrs = { .size = 1, .xs = g_get_block_instrs }
};

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);


// FbleNewValueHeap -- see documentation in fble-.h
FbleValueHeap* FbleNewValueHeap(FbleArena* arena)
{
  return FbleNewMarkSweepHeap(arena,
      (void (*)(FbleHeapCallback*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);
}

// FbleFreeValueHeap -- see documentation in fble.h
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  FbleFreeMarkSweepHeap(heap);
}

// FbleValueRetain -- see documentation in fble.h
FbleValue* FbleValueRetain(FbleValueHeap* heap, FbleValue* value)
{
  if (value != NULL) {
    heap->retain(heap, value);
  }
  return value;
}

// FbleValueRelease -- see documentation in fble.h
void FbleValueRelease(FbleValueHeap* heap, FbleValue* value)
{
  if (value != NULL) {
    heap->release(heap, value);
  }
}

// FbleValueAddRef -- see documentation in fble-value.h
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  heap->add_ref(heap, src, dst);
}

// FbleValueDelRef -- see documentation in fble-value.h
void FbleValueDelRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  heap->del_ref(heap, src, dst);
}

// OnFree --
//   The 'on_free' function for values. See documentation in fble-heap.h
static void OnFree(FbleValueHeap* heap, FbleValue* value)
{
  FbleArena* arena = heap->arena;
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: return;
    case FBLE_UNION_VALUE: return;

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      switch (fv->tag) {
        case FBLE_BASIC_FUNC_VALUE: {
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)fv;
          FbleFreeInstrBlock(arena, basic->code);
          return;
        }

        case FBLE_THUNK_FUNC_VALUE: return;
        case FBLE_PUT_FUNC_VALUE: return;
      }
      UNREACHABLE("should never get here");
      return;
    }

    case FBLE_PROC_VALUE: {
      FbleProcValue* v = (FbleProcValue*)value;
      FbleFreeInstrBlock(arena, v->code);
      return;
    }

    case FBLE_LINK_VALUE: {
      FbleLinkValue* v = (FbleLinkValue*)value;
      FbleValues* curr = v->head;
      while (curr != NULL) {
        FbleValues* tmp = curr;
        curr = curr->next;
        FbleFree(arena, tmp);
      }
      return;
    }

    case FBLE_PORT_VALUE: return;
    case FBLE_REF_VALUE: return;
    case FBLE_TYPE_VALUE: return;
  }

  UNREACHABLE("Should not get here");
}

// Ref --
//   Helper function for implementing Refs. Call the callback if the value is
//   not null.
//
// Inputs:
//   callback - the refs callback.
//   value - the value to add.
//
// Results:
//   none.
//
// Side effects:
//   If value is non-null, the callback is called for it.
static void Ref(FbleHeapCallback* callback, FbleValue* value)
{
  if (value != NULL) {
    callback->callback(callback, value);
  }
}

// Refs --
//   The 'refs' function for values. See documentation in fble-heap.h
static void Refs(FbleHeapCallback* callback, FbleValue* value)
{
  switch (value->tag) {
    case FBLE_STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        Ref(callback, sv->fields[i]);
      }
      break;
    }

    case FBLE_UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)value;
      Ref(callback, uv->arg);
      break;
    }

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      switch (fv->tag) {
        case FBLE_BASIC_FUNC_VALUE: {
          FbleBasicFuncValue* basic = (FbleBasicFuncValue*)fv;
          for (size_t i = 0; i < basic->scopec; ++i) {
            Ref(callback, basic->scope[i]);
          }
          break;
        }

        case FBLE_THUNK_FUNC_VALUE: {
          FbleThunkFuncValue* thunk = (FbleThunkFuncValue*)fv;
          Ref(callback, &thunk->func->_base);
          Ref(callback, thunk->arg);
          break;
        }

        case FBLE_PUT_FUNC_VALUE: {
          FblePutFuncValue* put = (FblePutFuncValue*)fv;
          Ref(callback, put->port);
          break;
        }
      }
      break;
    }

    case FBLE_PROC_VALUE: {
      FbleProcValue* v = (FbleProcValue*)value;
      for (size_t i = 0; i < v->scopec; ++i) {
        Ref(callback, v->scope[i]);
      }
      break;
    }

    case FBLE_LINK_VALUE: {
      FbleLinkValue* v = (FbleLinkValue*)value;
      for (FbleValues* elem = v->head; elem != NULL; elem = elem->next) {
        Ref(callback, elem->value);
      }
      break;
    }

    case FBLE_PORT_VALUE: {
      break;
    }

    case FBLE_REF_VALUE: {
      FbleRefValue* rv = (FbleRefValue*)value;
      Ref(callback, rv->value);
      break;
    }

    case FBLE_TYPE_VALUE: {
      break;
    }
  }
}

// FbleNewStructValue -- see documentation in fble.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, FbleValueV args)
{
  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * args.size);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fieldc = args.size;

  for (size_t i = 0; i < args.size; ++i) {
    value->fields[i] = args.xs[i];
    FbleValueAddRef(heap, &value->_base, args.xs[i]);
    FbleValueRelease(heap, args.xs[i]);
  }
  return &value->_base;
}

// FbleStructValueAccess -- see documentation in fble-value.h
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field)
{
  assert(object->tag == FBLE_STRUCT_VALUE);
  FbleStructValue* value = (FbleStructValue*)object;
  assert(field < value->fieldc);
  return value->fields[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg)
{
  FbleUnionValue* union_value = FbleNewValue(heap, FbleUnionValue);
  union_value->_base.tag = FBLE_UNION_VALUE;
  union_value->tag = tag;
  union_value->arg = arg;
  FbleValueAddRef(heap, &union_value->_base, arg);
  FbleValueRelease(heap, arg);
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

// FbleNewGetValue -- see documentation in value.h
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port)
{
  assert(port->tag == FBLE_LINK_VALUE || port->tag == FBLE_PORT_VALUE);

  FbleProcValue* get = FbleNewValueExtra(heap, FbleProcValue, sizeof(FbleValue*));
  get->_base.tag = FBLE_PROC_VALUE;
  get->scopec = 1;
  get->scope[0] = port;
  FbleValueAddRef(heap, &get->_base, port);
  get->code = &g_get_block;
  get->code->refcount++;
  return &get->_base;
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, size_t id)
{
  FblePortValue* get_port = FbleNewValue(heap, FblePortValue);
  get_port->_base.tag = FBLE_PORT_VALUE;
  get_port->id = id;

  FbleValue* get = FbleNewGetValue(heap, &get_port->_base);
  FbleValueRelease(heap, &get_port->_base);
  return get;
}

// FbleNewPutValue -- see documentation in value.h
FbleValue* FbleNewPutValue(FbleValueHeap* heap, FbleValue* link)
{
  FblePutFuncValue* put = FbleNewValue(heap, FblePutFuncValue);
  put->_base._base.tag = FBLE_FUNC_VALUE;
  put->_base.tag = FBLE_PUT_FUNC_VALUE;
  put->_base.argc = 1;
  put->port = link;
  FbleValueAddRef(heap, &put->_base._base, put->port);
  return &put->_base._base;
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, size_t id)
{
  FblePortValue* port_value = FbleNewValue(heap, FblePortValue);
  port_value->_base.tag = FBLE_PORT_VALUE;
  port_value->id = id;
  FbleValue* put = FbleNewPutValue(heap, &port_value->_base);
  FbleValueRelease(heap, &port_value->_base);
  return put;
}
