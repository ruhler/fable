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

// FbleStructValue --
//   FBLE_STRUCT_VALUE
//
// Represents an unpacked struct value.
typedef struct {
  FbleUnpackedValue _base;
  size_t fieldc;
  FbleValue fields[];
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
//
// Represents an unpacked union value.
typedef struct {
  FbleUnpackedValue _base;
  size_t tag;
  FbleValue arg;
} FbleUnionValue;

// FbleValues --
//   A non-circular singly linked list of values.
typedef struct FbleValues {
  FbleValue value;
  struct FbleValues* next;
} FbleValues;

// FbleLinkValue -- FBLE_LINK_VALUE
//   Holds the list of values on a link. Values are added to the tail and taken
//   from the head. If there are no values on the list, both head and tail are
//   set to NULL.
typedef struct {
  FbleUnpackedValue _base;
  FbleValues* head;
  FbleValues* tail;
} FbleLinkValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
//
// Fields:
//   data - a pointer to a value owned externally where data should be put to
//          and got from.
typedef struct {
  FbleUnpackedValue _base;
  FbleValue* data;
} FblePortValue;

// FbleRefValue --
//   FBLE_REF_VALUE
//
// An implementation-specific value introduced to support recursive values and
// not yet computed values.
//
// A ref value holds a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are ref
// values.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
typedef struct {
  FbleUnpackedValue _base;
  FbleValue value;
} FbleRefValue;

FbleValue FbleNullValue = { .unpacked = NULL };
FbleValue FbleGenericTypeValue = { .packed = 1 };

static void OnFree(FbleValueHeap* heap, FbleUnpackedValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue value);
static void Refs(FbleHeapCallback* callback, FbleUnpackedValue* value);

static FbleExecStatus GetRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void GetAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleExecStatus PutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void PutAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleExecStatus PartialPutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void PartialPutAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleValue NewGetValue(FbleValueHeap* heap, FbleValue port, FbleBlockId profile);
static FbleValue NewPutValue(FbleValueHeap* heap, FbleValue link, FbleBlockId profile);


// FbleWrapUnpackedValue -- see documentation in value.h
//
// TODO: Is there any way we could turn this into a MACRO to avoid the
// function call overhead for what is essentially a cast?
FbleValue FbleWrapUnpackedValue(FbleUnpackedValue* unpacked)
{
  FbleValue value;
  value.unpacked = unpacked;
  return value;
}

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

// FbleValueIsNull -- see documentation in fble-value.h
bool FbleValueIsNull(FbleValue value)
{
  return value.unpacked == NULL;
}

// FbleRetainValue -- see documentation in fble-value.h
void FbleRetainValue(FbleValueHeap* heap, FbleValue value)
{
  if (FbleValueIsUnpacked(value)) {
    FbleRetainHeapObject(heap, value.unpacked);
  }
}

// FbleReleaseValue -- see documentation in fble-value.h
void FbleReleaseValue(FbleValueHeap* heap, FbleValue value)
{
  if (FbleValueIsUnpacked(value) && value.unpacked != NULL) {
    FbleReleaseHeapObject(heap, value.unpacked);
  }
}

// FbleValueAddRef -- see documentation in fble-value.h
void FbleValueAddRef(FbleValueHeap* heap, FbleValue src, FbleValue dst)
{
  if (FbleValueIsUnpacked(src) && FbleValueIsUnpacked(dst)) {
    FbleHeapObjectAddRef(heap, src.unpacked, dst.unpacked);
  }
}

// FbleValueFullGc -- see documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap)
{
  FbleHeapFullGc(heap);
}

// OnFree --
//   The 'on_free' function for values. See documentation in heap.h
static void OnFree(FbleValueHeap* heap, FbleUnpackedValue* value)
{
  switch (value->tag) {
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
static void Ref(FbleHeapCallback* callback, FbleValue value)
{
  if (FbleValueIsUnpacked(value) && value.unpacked != NULL) {
    callback->callback(callback, value.unpacked);
  }
}

// Refs --
//   The 'refs' function for values. See documentation in heap.h
static void Refs(FbleHeapCallback* callback, FbleUnpackedValue* value)
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
FbleValue FbleNewStructValue(FbleValueHeap* heap, size_t argc, ...)
{
  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue) * argc);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fieldc = argc;

  va_list ap;
  va_start(ap, argc);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue arg = va_arg(ap, FbleValue);
    value->fields[i] = arg;
    if (FbleValueIsUnpacked(arg) && arg.unpacked != NULL) {
      FbleValueAddRef(heap, FbleWrapUnpackedValue(&value->_base), arg);
    }
  }
  va_end(ap);
  return FbleWrapUnpackedValue(&value->_base);
}

// FbleNewStructValue_ -- see documentation in fble-value.h
FbleValue FbleNewStructValue_(FbleValueHeap* heap, size_t argc, FbleValue* args)
{
  FbleStructValue* value = FbleNewValueExtra(heap, FbleStructValue, sizeof(FbleValue) * argc);
  value->_base.tag = FBLE_STRUCT_VALUE;
  value->fieldc = argc;

  for (size_t i = 0; i < argc; ++i) {
    FbleValue arg = args[i];
    value->fields[i] = arg;
    if (FbleValueIsUnpacked(arg) && arg.unpacked != NULL) {
      FbleValueAddRef(heap, FbleWrapUnpackedValue(&value->_base), arg);
    }
  }

  return FbleWrapUnpackedValue(&value->_base);
}

// FbleStructValueAccess -- see documentation in fble-value.h
FbleValue FbleStructValueAccess(FbleValue object, size_t field)
{
  object = FbleStrictValue(object);
  assert(FbleValueIsUnpacked(object) && "TODO: implement packed values");
  assert(object.unpacked != NULL && object.unpacked->tag == FBLE_STRUCT_VALUE);
  FbleStructValue* value = (FbleStructValue*)object.unpacked;
  assert(field < value->fieldc);
  return value->fields[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue arg)
{
  FbleUnionValue* union_value = FbleNewValue(heap, FbleUnionValue);
  union_value->_base.tag = FBLE_UNION_VALUE;
  union_value->tag = tag;
  union_value->arg = arg;
  FbleValueAddRef(heap, FbleWrapUnpackedValue(&union_value->_base), arg);
  return FbleWrapUnpackedValue(&union_value->_base);
}
// FbleNewEnumValue -- see documentation in fble-value.h
FbleValue FbleNewEnumValue(FbleValueHeap* heap, size_t tag)
{
  FbleValue unit = FbleNewStructValue(heap, 0);
  FbleValue result = FbleNewUnionValue(heap, tag, unit);
  FbleReleaseValue(heap, unit);
  return result;
}

// FbleUnionValueTag -- see documentation in fble-value.h
size_t FbleUnionValueTag(FbleValue object)
{
  object = FbleStrictValue(object);
  assert(FbleValueIsUnpacked(object) && "TODO: support packed objects");
  assert(object.unpacked != NULL && object.unpacked->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object.unpacked;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue FbleUnionValueAccess(FbleValue object)
{
  object = FbleStrictValue(object);
  assert(FbleValueIsUnpacked(object) && "TODO: support packed objects");
  assert(object.unpacked != NULL && object.unpacked->tag == FBLE_UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object.unpacked;
  return value->arg;
}

// FbleNewFuncValue -- see documentation in value.h
FbleFuncValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id)
{
  FbleFuncValue* v = FbleNewValueExtra(heap, FbleFuncValue, sizeof(FbleValue) * executable->statics);
  v->_base.tag = FBLE_FUNC_VALUE;
  v->profile_base_id = profile_base_id;
  v->executable = executable;
  v->executable->refcount++;
  return v;
}

// FbleFuncValueStatics -- see documentation in value.h
FbleValue* FbleFuncValueStatics(FbleValue func)
{
  assert(FbleValueIsUnpacked(func));
  FbleFuncValue* func_value = (FbleFuncValue*)func.unpacked;
  return func_value->statics;
}

// FbleFuncValueProfileBaseId -- see documentation in value.h
size_t FbleFuncValueProfileBaseId(FbleValue func)
{
  assert(FbleValueIsUnpacked(func));
  FbleFuncValue* func_value = (FbleFuncValue*)func.unpacked;
  return func_value->profile_base_id;
}

// FbleFuncValueExecutable -- see documentation in value.h
FbleExecutable* FbleFuncValueExecutable(FbleValue func)
{
  assert(FbleValueIsUnpacked(func));
  FbleFuncValue* func_value = (FbleFuncValue*)func.unpacked;
  return func_value->executable;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue value)
{
  if (FbleValueIsUnpacked(value)) {
    FbleProcValue* proc = (FbleProcValue*)value.unpacked;
    return value.unpacked->tag == FBLE_PROC_VALUE && proc->executable->args == 0;
  }
  return false;
}

// GetRunFunction --
//   FbleExecutable.run function for Get value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus GetRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleValue get_port = FbleFuncValueStatics(thread->stack->func)[0];
  assert(FbleValueIsUnpacked(get_port));
  if (get_port.unpacked->tag == FBLE_LINK_VALUE) {
    FbleLinkValue* link = (FbleLinkValue*)get_port.unpacked;

    if (link->head == NULL) {
      // Blocked on get.
      return FBLE_EXEC_BLOCKED;
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

  assert(get_port.unpacked->tag == FBLE_PORT_VALUE);
  FblePortValue* port = (FblePortValue*)get_port.unpacked;
  if (FbleValueIsNull(*port->data)) {
    // Blocked on get.
    return FBLE_EXEC_BLOCKED;
  }

  FbleThreadReturn(heap, thread, *port->data);
  *port->data = FbleNullValue;
  *io_activity = true;
  return FBLE_EXEC_FINISHED;
}

// GetAbortFunction
//   FbleExecutable.abort function for Get value.
//
// See documentation of FbleExecutable.abort in execute.h
static void GetAbortFunction(FbleValueHeap* heap, FbleStack* stack)
{
  *(stack->result) = FbleNullValue;
}

// PutRunFunction --
//   FbleExecutable.run function for Put value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus PutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleValue* statics = FbleFuncValueStatics(thread->stack->func);
  FbleValue put_port = statics[0];
  FbleValue arg = statics[1];
  if (put_port.unpacked->tag == FBLE_LINK_VALUE) {
    FbleLinkValue* link = (FbleLinkValue*)put_port.unpacked;

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

    FbleValueAddRef(heap, FbleWrapUnpackedValue(&link->_base), tail->value);
    FbleThreadReturn(heap, thread, FbleNewStructValue(heap, 0));
    *io_activity = true;
    return FBLE_EXEC_FINISHED;
  }

  assert(put_port.unpacked->tag == FBLE_PORT_VALUE);
  FblePortValue* port = (FblePortValue*)put_port.unpacked;
  if (!FbleValueIsNull(*port->data)) {
    // Blocked on put.
    return FBLE_EXEC_BLOCKED;
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
  *(stack->result) = FbleNullValue;
}

// PartialPutRunFunction --
//   FbleExecutable.run function for partially applied put value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus PartialPutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  static FbleExecutable executable = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .args = 0,
    .statics = 2,
    .locals = 0,
    .profile = 0,
    .profile_blocks = { .size = 0, .xs = NULL },
    .run = &PutRunFunction,
    .abort = &PutAbortFunction,
    .on_free = NULL,
  };

  FbleFuncValue* put = FbleNewFuncValue(heap, &executable, FbleFuncValueProfileBaseId(thread->stack->func) + 1);

  FbleValue link = FbleFuncValueStatics(thread->stack->func)[0];
  FbleValue arg = thread->stack->locals[0];
  put->statics[0] = link;
  FbleValueAddRef(heap, FbleWrapUnpackedValue(&put->_base), link);
  put->statics[1] = arg;
  FbleValueAddRef(heap, FbleWrapUnpackedValue(&put->_base), arg);

  FbleReleaseValue(heap, thread->stack->locals[0]);
  FbleThreadReturn(heap, thread, FbleWrapUnpackedValue(&put->_base));
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
  *(stack->result) = FbleNullValue;
}

// NewGetValue --
//   Create a new get proc value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   port - the port value to get from.
//   profile - the id of a profile block to use for when the get is executed.
//
// Results:
//   A newly allocated get value.
//
// Side effects:
//   The returned get value must be freed using FbleReleaseValue when no
//   longer in use. This function does not take ownership of the port value.
//   argument.
static FbleValue NewGetValue(FbleValueHeap* heap, FbleValue port, FbleBlockId profile)
{
  assert(FbleValueIsUnpacked(port));
  assert(port.unpacked->tag == FBLE_LINK_VALUE || port.unpacked->tag == FBLE_PORT_VALUE);

  static FbleExecutable executable = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .args = 0,
    .statics = 1,
    .locals = 0,
    .profile = 0,
    .profile_blocks = { .size = 0, .xs = NULL },
    .run = &GetRunFunction,
    .abort = &GetAbortFunction,
    .on_free = NULL
  };

  FbleProcValue* get = FbleNewFuncValue(heap, &executable, profile);
  get->statics[0] = port;
  FbleValueAddRef(heap, FbleWrapUnpackedValue(&get->_base), port);
  return FbleWrapUnpackedValue(&get->_base);
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue FbleNewInputPortValue(FbleValueHeap* heap, FbleValue* data, FbleBlockId profile)
{
  FblePortValue* get_port = FbleNewValue(heap, FblePortValue);
  get_port->_base.tag = FBLE_PORT_VALUE;
  get_port->data = data;

  FbleValue get = NewGetValue(heap, FbleWrapUnpackedValue(&get_port->_base), profile);
  FbleReleaseValue(heap, FbleWrapUnpackedValue(&get_port->_base));
  return get;
}

// NewPutValue --
//   Create a new put value for the given link.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   link - the link to put to. Borrowed.
//   profile - the first of two consecutive ids of profile blocks to use for
//             when the first argument is applied to the put and when the put
//             is executed.
//
// Results:
//   A newly allocated put value.
//
// Side effects:
//   The returned put value must be freed using FbleReleaseValue when no
//   longer in use. This function does not take ownership of the link value.
static FbleValue NewPutValue(FbleValueHeap* heap, FbleValue link, FbleBlockId profile)
{
  static FbleExecutable executable = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .args = 1,
    .statics = 1,
    .locals = 1,
    .profile = 0,
    .profile_blocks = { .size = 0, .xs = NULL },
    .run = &PartialPutRunFunction,
    .abort = &PartialPutAbortFunction,
    .on_free = NULL,
  };

  FbleFuncValue* put = FbleNewFuncValue(heap, &executable, profile);
  put->statics[0] = link;
  FbleValueAddRef(heap, FbleWrapUnpackedValue(&put->_base), link);
  return FbleWrapUnpackedValue(&put->_base);
}

// FbleNewLinkValue -- see documentation in value.h
void FbleNewLinkValue(FbleValueHeap* heap, FbleBlockId profile, FbleValue* get, FbleValue* put)
{
  FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);
  link->_base.tag = FBLE_LINK_VALUE;
  link->head = NULL;
  link->tail = NULL;

  *get = NewGetValue(heap, FbleWrapUnpackedValue(&link->_base), profile);
  *put = NewPutValue(heap, FbleWrapUnpackedValue(&link->_base), profile + 1);
  FbleReleaseValue(heap, FbleWrapUnpackedValue(&link->_base));
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue FbleNewOutputPortValue(FbleValueHeap* heap, FbleValue* data, FbleBlockId profile)
{
  FblePortValue* port_value = FbleNewValue(heap, FblePortValue);
  port_value->_base.tag = FBLE_PORT_VALUE;
  port_value->data = data;
  FbleValue put = NewPutValue(heap, FbleWrapUnpackedValue(&port_value->_base), profile);
  FbleReleaseValue(heap, FbleWrapUnpackedValue(&port_value->_base));
  return put;
}

// FbleNewListValue -- see documentation in value.h
FbleValue FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue* args)
{
  FbleValue unit = FbleNewStructValue(heap, 0);
  FbleValue tail = FbleNewUnionValue(heap, 1, unit);
  FbleReleaseValue(heap, unit);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue arg = args[argc - i - 1];
    FbleValue cons = FbleNewStructValue(heap, 2, arg, tail);
    FbleReleaseValue(heap, tail);

    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
  }
  return tail;
}

// FbleNewLiteralValue -- see documentation in value.h
FbleValue FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args)
{
  FbleValue unit = FbleNewStructValue(heap, 0);
  FbleValue tail = FbleNewUnionValue(heap, 1, unit);
  FbleReleaseValue(heap, unit);
  for (size_t i = 0; i < argc; ++i) {
    size_t letter = args[argc - i - 1];
    FbleValue arg = FbleNewUnionValue(heap, letter, unit);
    FbleValue cons = FbleNewStructValue(heap, 2, arg, tail);
    FbleReleaseValue(heap, arg);
    FbleReleaseValue(heap, tail);

    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
  }
  return tail;
}

// FbleNewRefValue -- see documentation in value.h
FbleValue FbleNewRefValue(FbleValueHeap* heap)
{
  FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
  rv->_base.tag = FBLE_REF_VALUE;
  rv->value = FbleNullValue;
  return FbleWrapUnpackedValue(&rv->_base);
}

// FbleAssignRefValue -- see documentation in value.h
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue ref, FbleValue value)
{
  // Unwrap any accumulated layers of references on the value and make sure we
  // aren't forming a vacuous value.
  FbleRefValue* unwrap = (FbleRefValue*)value.unpacked;
  while (FbleValueIsUnpacked(value) && value.unpacked->tag == FBLE_REF_VALUE && !FbleValueIsNull(unwrap->value)) {
    value = unwrap->value;
    unwrap = (FbleRefValue*)value.unpacked;
  }

  if (FbleValueIsUnpacked(value) && value.unpacked == ref.unpacked) {
    return false;
  }

  assert(FbleValueIsUnpacked(ref));
  FbleRefValue* rv = (FbleRefValue*)ref.unpacked;
  assert(rv->_base.tag == FBLE_REF_VALUE);
  rv->value = value;
  FbleValueAddRef(heap, ref, value);
  return true;
}

// FbleStrictValue -- see documentation in value.h
FbleValue FbleStrictValue(FbleValue value)
{
  while (FbleValueIsUnpacked(value) && value.unpacked != NULL && value.unpacked->tag == FBLE_REF_VALUE) {
    FbleRefValue* ref = (FbleRefValue*)value.unpacked;
    value = ref->value;
  }
  return value;
}
