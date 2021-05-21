// value.c
//   This file implements routines associated with fble values.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL

#include "fble-alloc.h"   // for FbleAlloc, FbleFree, etc.
#include "fble-vector.h"  // for FbleVectorInit, etc.

#include "heap.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);

static FbleExecStatus GetRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void GetAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleExecStatus PutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void PutAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleExecStatus PartialPutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void PartialPutAbortFunction(FbleValueHeap* heap, FbleStack* stack);


// FbleNewValueHeap -- see documentation in fble-.h
FbleValueHeap* FbleNewValueHeap()
{
  return FbleNewHeap(
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
  FbleRetainHeapObject(heap, value);
}

// FbleReleaseValue -- see documentation in fble-value.h
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
{
  if (value != NULL) {
    FbleReleaseHeapObject(heap, value);
  }
}

// FbleValueAddRef -- see documentation in fble-value.h
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  FbleHeapObjectAddRef(heap, src, dst);
}

// FbleValueFullGc -- see documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap)
{
  FbleHeapFullGc(heap);
}

// OnFree --
//   The 'on_free' function for values. See documentation in heap.h
static void OnFree(FbleValueHeap* heap, FbleValue* value)
{
  switch (value->tag) {
    case FBLE_TYPE_VALUE: return;
    case FBLE_STRUCT_VALUE: return;
    case FBLE_UNION_VALUE: return;

    case FBLE_FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
      FbleFreeExecutable(v->executable);
      return;
    }

    case FBLE_LINK_VALUE: {
      FbleLinkValue* v = (FbleLinkValue*)value;
      FbleValues* curr = v->head;
      while (curr != NULL) {
        FbleValues* tmp = curr;
        curr = curr->next;
        FbleFree(tmp);
      }
      return;
    }

    case FBLE_PORT_VALUE: return;
    case FBLE_REF_VALUE: return;
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
      for (size_t i = 0; i < v->executable->statics; ++i) {
        Ref(callback, v->statics[i]);
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
      FbleRefValue* v = (FbleRefValue*)value;
      Ref(callback, v->value);
      break;
    }
  }
}

// FbleNewStructValue -- see documentation in fble-value.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, ...)
{
  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue*) * argc);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fieldc = argc;

  va_list ap;
  va_start(ap, argc);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = va_arg(ap, FbleValue*);
    value->fields[i] = arg;
    if (arg != NULL) {
      FbleValueAddRef(heap, &value->_base, arg);
    }
  }
  va_end(ap);
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
  FbleValue* unit = FbleNewStructValue(heap, 0);
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

// FbleNewFuncValue -- see documentation in value.h
FbleFuncValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id)
{
  FbleFuncValue* v = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue*) * executable->statics);
  v->_base.tag = FBLE_FUNC_VALUE;
  v->profile_base_id = profile_base_id;
  v->executable = executable;
  v->executable->refcount++;
  return v;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue* value)
{
  FbleProcValue* proc = (FbleProcValue*)value;
  return value->tag == FBLE_PROC_VALUE && proc->executable->args == 0;
}

// GetRunFunction --
//   FbleExecutable.run function for Get value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus GetRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  assert(thread->stack->pc == 0);
  assert(thread->stack->func->executable->args == 0);
  assert(thread->stack->func->executable->statics == 1);
  assert(thread->stack->func->executable->locals == 0);
  assert(thread->stack->func->executable->run == &GetRunFunction);

  FbleValue* get_port = thread->stack->func->statics[0];
  if (get_port->tag == FBLE_LINK_VALUE) {
    FbleLinkValue* link = (FbleLinkValue*)get_port;

    if (link->head == NULL) {
      // Blocked on get.
      return FBLE_EXEC_BLOCKED;
    }

    if (thread->profile != NULL) {
      FbleProfileEnterBlock(thread->profile, thread->stack->func->profile_base_id);
      FbleProfileExitBlock(thread->profile);
    }

    FbleValues* head = link->head;
    link->head = link->head->next;
    if (link->head == NULL) {
      link->tail = NULL;
    }

    FbleRetainValue(heap, head->value);
    FbleThreadReturn(heap, thread, head->value);
    FbleFree(head);
    return FBLE_EXEC_FINISHED;
  }

  assert(get_port->tag == FBLE_PORT_VALUE);
  FblePortValue* port = (FblePortValue*)get_port;
  if (*port->data == NULL) {
    // Blocked on get.
    return FBLE_EXEC_BLOCKED;
  }

  if (thread->profile != NULL) {
    FbleProfileEnterBlock(thread->profile, thread->stack->func->profile_base_id);
    FbleProfileExitBlock(thread->profile);
  }

  FbleThreadReturn(heap, thread, *port->data);
  *port->data = NULL;
  *io_activity = true;
  return FBLE_EXEC_FINISHED;
}

// GetAbortFunction
//   FbleExecutable.abort function for Get value.
//
// See documentation of FbleExecutable.abort in execute.h
static void GetAbortFunction(FbleValueHeap* heap, FbleStack* stack)
{
  *(stack->result) = NULL;
}

// PutRunFunction --
//   FbleExecutable.run function for Put value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus PutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  assert(thread->stack->pc == 0);
  assert(thread->stack->func->executable->args == 0);
  assert(thread->stack->func->executable->statics == 2);
  assert(thread->stack->func->executable->locals == 0);
  assert(thread->stack->func->executable->run == &PutRunFunction);

  FbleValue* put_port = thread->stack->func->statics[0];
  FbleValue* arg = thread->stack->func->statics[1];
  if (put_port->tag == FBLE_LINK_VALUE) {
    if (thread->profile != NULL) {
      FbleProfileEnterBlock(thread->profile, thread->stack->func->profile_base_id);
      FbleProfileExitBlock(thread->profile);
    }

    FbleLinkValue* link = (FbleLinkValue*)put_port;

    FbleValues* tail = FbleAlloc(FbleValues);
    tail->value = arg;
    tail->next = NULL;

    if (link->head == NULL) {
      link->head = tail;
      link->tail = tail;
    } else {
      assert(link->tail != NULL);
      link->tail->next = tail;
      link->tail = tail;
    }

    FbleValueAddRef(heap, &link->_base, tail->value);
    FbleThreadReturn(heap, thread, FbleNewStructValue(heap, 0));
    *io_activity = true;
    return FBLE_EXEC_FINISHED;
  }

  assert(put_port->tag == FBLE_PORT_VALUE);
  FblePortValue* port = (FblePortValue*)put_port;
  if (*port->data != NULL) {
    // Blocked on put.
    return FBLE_EXEC_BLOCKED;
  }

  if (thread->profile != NULL) {
    FbleProfileEnterBlock(thread->profile, thread->stack->func->profile_base_id);
    FbleProfileExitBlock(thread->profile);
  }

  FbleRetainValue(heap, arg);
  *port->data = arg;
  FbleThreadReturn(heap, thread, FbleNewStructValue(heap, 0));
  *io_activity = true;
  return FBLE_EXEC_FINISHED;
}

// PutAbortFunction
//   FbleExecutable.abort function for Get value.
//
// See documentation of FbleExecutable.abort in execute.h
static void PutAbortFunction(FbleValueHeap* heap, FbleStack* stack)
{
  *(stack->result) = NULL;
}

// PartialPutRunFunction --
//   FbleExecutable.run function for partially applied put value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus PartialPutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  assert(thread->stack->pc == 0);
  assert(thread->stack->func->executable->args == 1);
  assert(thread->stack->func->executable->statics == 1);
  assert(thread->stack->func->executable->locals == 1);
  assert(thread->stack->func->executable->run == &PartialPutRunFunction);

  static FbleExecutable executable = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .args = 0,
    .statics = 2,
    .locals = 0,
    .profile_blocks = { .size = 0, .xs = NULL },
    .run = &PutRunFunction,
    .abort = &PutAbortFunction,
    .on_free = NULL,
  };

  if (thread->profile != NULL) {
    FbleProfileEnterBlock(thread->profile, thread->stack->func->profile_base_id);
    FbleProfileExitBlock(thread->profile);
  }

  FbleFuncValue* put = FbleNewFuncValue(heap, &executable, thread->stack->func->profile_base_id + 1);

  FbleValue* link = thread->stack->func->statics[0];
  FbleValue* arg = thread->stack->locals[0];
  put->statics[0] = link;
  FbleValueAddRef(heap, &put->_base, link);
  put->statics[1] = arg;
  FbleValueAddRef(heap, &put->_base, arg);

  FbleReleaseValue(heap, thread->stack->locals[0]);
  FbleThreadReturn(heap, thread, &put->_base);
  return FBLE_EXEC_FINISHED;
}

// PartialPutAbortFunction
//   FbleExecutable.abort function for Get value.
//
// See documentation of FbleExecutable.abort in execute.h
static void PartialPutAbortFunction(FbleValueHeap* heap, FbleStack* stack)
{
  // The only time abort should be called is if we haven't had a chance to run
  // the function yet. In this case we need to clean up its single argument.
  FbleReleaseValue(heap, stack->locals[0]);
  *(stack->result) = NULL;
}

// FbleNewGetValue -- see documentation in value.h
FbleValue* FbleNewGetValue(FbleValueHeap* heap, FbleValue* port, FbleBlockId profile)
{
  assert(port->tag == FBLE_LINK_VALUE || port->tag == FBLE_PORT_VALUE);

  static FbleExecutable executable = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .args = 0,
    .statics = 1,
    .locals = 0,
    .profile_blocks = { .size = 0, .xs = NULL },
    .run = &GetRunFunction,
    .abort = &GetAbortFunction,
    .on_free = NULL
  };

  FbleProcValue* get = FbleNewFuncValue(heap, &executable, profile);
  get->statics[0] = port;
  FbleValueAddRef(heap, &get->_base, port);
  return &get->_base;
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, FbleValue** data, FbleBlockId profile)
{
  FblePortValue* get_port = FbleNewValue(heap, FblePortValue);
  get_port->_base.tag = FBLE_PORT_VALUE;
  get_port->data = data;

  FbleValue* get = FbleNewGetValue(heap, &get_port->_base, profile);
  FbleReleaseValue(heap, &get_port->_base);
  return get;
}

// FbleNewPutValue -- see documentation in value.h
FbleValue* FbleNewPutValue(FbleValueHeap* heap, FbleValue* link, FbleBlockId profile)
{
  static FbleExecutable executable = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .args = 1,
    .statics = 1,
    .locals = 1,
    .profile_blocks = { .size = 0, .xs = NULL },
    .run = &PartialPutRunFunction,
    .abort = &PartialPutAbortFunction,
    .on_free = NULL,
  };

  FbleFuncValue* put = FbleNewFuncValue(heap, &executable, profile);
  put->statics[0] = link;
  FbleValueAddRef(heap, &put->_base, link);
  return &put->_base;
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, FbleValue** data, FbleBlockId profile)
{
  FblePortValue* port_value = FbleNewValue(heap, FblePortValue);
  port_value->_base.tag = FBLE_PORT_VALUE;
  port_value->data = data;
  FbleValue* put = FbleNewPutValue(heap, &port_value->_base, profile);
  FbleReleaseValue(heap, &port_value->_base);
  return put;
}

// FbleStrictValue -- see documentation in value.h
FbleValue* FbleStrictValue(FbleValue* value)
{
  while (value != NULL && value->tag == FBLE_REF_VALUE) {
    FbleRefValue* ref = (FbleRefValue*)value;
    value = ref->value;
  }
  return value;
}
