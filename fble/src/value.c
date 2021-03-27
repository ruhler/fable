// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble.h"
#include "execute.h"     // for FbleStandardRunFunction
#include "heap.h"
#include "isa.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);


// FbleNewValueHeap -- see documentation in fble-.h
FbleValueHeap* FbleNewValueHeap(FbleArena* arena)
{
  return FbleNewHeap(arena,
      (void (*)(FbleHeapCallback*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);
}

// FbleFreeValueHeap -- see documentation in fble.h
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  FbleFreeHeap(heap);
}

// FbleRetainValue -- see documentation in fble-value.h
void FbleRetainValue(FbleValueHeap* heap, FbleValue* value)
{
  heap->retain(heap, value);
}

// FbleReleaseValue -- see documentation in fble-value.h
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
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

// FbleValueFullGc -- see documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap)
{
  heap->full_gc(heap);
}

// OnFree --
//   The 'on_free' function for values. See documentation in heap.h
static void OnFree(FbleValueHeap* heap, FbleValue* value)
{
  FbleArena* arena = heap->arena;
  switch (value->tag) {
    case FBLE_TYPE_VALUE: return;
    case FBLE_STRUCT_VALUE: return;
    case FBLE_UNION_VALUE: return;

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
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

    case FBLE_THUNK_VALUE: {
      FbleThunkValue* v = (FbleThunkValue*)value;
      FbleFree(arena, v->locals.xs);
      return;
    }
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
//   The 'refs' function for values. See documentation in heap.h
static void Refs(FbleHeapCallback* callback, FbleValue* value)
{
  switch (value->tag) {
    case FBLE_TYPE_VALUE: break;

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
      FbleFuncValue* v = (FbleFuncValue*)value;
      for (size_t i = 0; i < v->code->statics; ++i) {
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

    case FBLE_THUNK_VALUE: {
      FbleThunkValue* rv = (FbleThunkValue*)value;
      Ref(callback, rv->value);
      Ref(callback, &rv->tail->_base);
      Ref(callback, &rv->func->_base);
      for (size_t i = 0; i < rv->locals.size; ++i) {
        Ref(callback, rv->locals.xs[i]);
      }
      break;
    }
  }
}

// FbleNewStructValue -- see documentation in fble-value.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, FbleValueV args)
{
  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * args.size);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fieldc = args.size;

  for (size_t i = 0; i < args.size; ++i) {
    value->fields[i] = args.xs[i];
    FbleValueAddRef(heap, &value->_base, args.xs[i]);
  }
  return &value->_base;
}

// FbleStructValueAccess -- see documentation in fble-value.h
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field)
{
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == FBLE_STRUCT_VALUE);
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
  return &union_value->_base;
}
// FbleNewEnumValue -- see documentation in fble-value.h
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag)
{
  FbleValueV args = { .size = 0, .xs = NULL, };
  FbleValue* unit = FbleNewStructValue(heap, args);
  FbleValue* result = FbleNewUnionValue(heap, tag, unit);
  FbleReleaseValue(heap, unit);
  return result;
}

// FbleUnionValueTag -- see documentation in fble-value.h
size_t FbleUnionValueTag(FbleValue* object)
{
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue* FbleUnionValueAccess(FbleValue* object)
{
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->arg;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue* value)
{
  FbleProcValue* proc = (FbleProcValue*)value;
  return value->tag == FBLE_PROC_VALUE && proc->argc == 0;
}

// FbleNewGetValue -- see documentation in value.h
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port)
{
  static FbleGetInstr iget = {
    ._base = { .tag = FBLE_GET_INSTR, .profile_ops = NULL },
    .port = { .section = FBLE_STATICS_FRAME_SECTION, .index = 0},
    .dest = 0,
  };

  static FbleReturnInstr irtn = {
    ._base = { .tag = FBLE_RETURN_INSTR, .profile_ops = NULL },
    .result = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0}
  };

  static FbleInstr* instrs[] = {
    &iget._base,
    &irtn._base,
  };

  static FbleInstrBlock code = {
    .refcount = 1,
    .magic = FBLE_INSTR_BLOCK_MAGIC,
    .statics = 1,  // port
    .locals = 1,   // result
    .instrs = { .size = 2, .xs = instrs }
  };

  assert(port->tag == FBLE_LINK_VALUE || port->tag == FBLE_PORT_VALUE);

  FbleProcValue* get = FbleNewValueExtra(heap, FbleProcValue, sizeof(FbleValue*));
  get->_base.tag = FBLE_PROC_VALUE;
  get->argc = 0;
  get->code = &code;
  get->code->refcount++;
  get->run = &FbleStandardRunFunction;
  get->scope[0] = port;
  FbleValueAddRef(heap, &get->_base, port);
  return &get->_base;
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, FbleValue** data)
{
  FblePortValue* get_port = FbleNewValue(heap, FblePortValue);
  get_port->_base.tag = FBLE_PORT_VALUE;
  get_port->data = data;

  FbleValue* get = FbleNewGetValue(heap, &get_port->_base);
  FbleReleaseValue(heap, &get_port->_base);
  return get;
}

// FbleNewPutValue -- see documentation in value.h
FbleValue* FbleNewPutValue(FbleValueHeap* heap, FbleValue* link)
{
  static FblePutInstr iput = {
    ._base = { .tag = FBLE_PUT_INSTR, .profile_ops = NULL },
    .port = { .section = FBLE_STATICS_FRAME_SECTION, .index = 0},
    .arg = { .section = FBLE_STATICS_FRAME_SECTION, .index = 1},
    .dest = 0,
  };

  static FbleReturnInstr irtn0 = {
    ._base = { .tag = FBLE_RETURN_INSTR, .profile_ops = NULL },
    .result = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0}
  };

  static FbleInstr* proc_instrs[] = {
    &iput._base,
    &irtn0._base,
  };

  static FbleInstrBlock proc_code = {
    .refcount = 1,
    .magic = FBLE_INSTR_BLOCK_MAGIC,
    .statics = 2,  // port, arg
    .locals = 1,   // result
    .instrs = { .size = 2, .xs = proc_instrs }
  };

  static FbleFrameIndex proc_scope[] = {
    { .section = FBLE_STATICS_FRAME_SECTION, .index = 0 },  // port
    { .section = FBLE_LOCALS_FRAME_SECTION, .index = 0 },   // arg
  };

  static FbleProcValueInstr iproc = {
    ._base = { .tag = FBLE_PROC_VALUE_INSTR, .profile_ops = NULL },
    .argc = 0,
    .code = &proc_code,
    .scope = { .size = 2, .xs = proc_scope },
    .dest = 1,
  };
  iproc.code->refcount++;

  static FbleReturnInstr irtn1 = {
    ._base = { .tag = FBLE_RETURN_INSTR, .profile_ops = NULL },
    .result = { .section = FBLE_LOCALS_FRAME_SECTION, .index = 1 }
  };

  static FbleInstr* func_instrs[] = {
    &iproc._base,
    &irtn1._base,
  };

  static FbleInstrBlock func_code = {
    .refcount = 1,
    .magic = FBLE_INSTR_BLOCK_MAGIC,
    .statics = 1,  // port
    .locals = 2,   // arg, result
    .instrs = { .size = 2, .xs = func_instrs }
  };
  func_code.refcount++;

  FbleFuncValue* put = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*));
  put->_base.tag = FBLE_FUNC_VALUE;
  put->argc = 1;
  put->code = &func_code;
  put->run = &FbleStandardRunFunction;
  put->scope[0] = link;
  FbleValueAddRef(heap, &put->_base, link);
  return &put->_base;
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, FbleValue** data)
{
  FblePortValue* port_value = FbleNewValue(heap, FblePortValue);
  port_value->_base.tag = FBLE_PORT_VALUE;
  port_value->data = data;
  FbleValue* put = FbleNewPutValue(heap, &port_value->_base);
  FbleReleaseValue(heap, &port_value->_base);
  return put;
}

// FbleStrictValue -- see documentation in value.h
FbleValue* FbleStrictValue(FbleValue* value)
{
  while (value != NULL && value->tag == FBLE_THUNK_VALUE) {
    FbleThunkValue* thunk = (FbleThunkValue*)value;
    value = thunk->value;
  }
  return value;
}
