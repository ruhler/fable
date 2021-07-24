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

// PACKED --
//   Test whether a value is packed into an FbleValue* pointer.
//
// IMPORTANT: Some fble values are packed directly in the FbleValue* pointer
// to save space. An FbleValue* only points to an FbleValue if the least
// significant bit of the pointer is 0.
#define PACKED(x) (((intptr_t)x) & 1)

// ValueTag --
//   A tag used to distinguish among different kinds of FbleValue.
typedef enum {
  STRUCT_VALUE,
  UNION_VALUE,
  FUNC_VALUE,
  LINK_VALUE,
  PORT_VALUE,
  REF_VALUE,
} ValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial layout as
//   FbleValue. The tag can be used to determine what kind of value this is to
//   get access to additional fields of the value by first casting to that
//   specific type of value.
struct FbleValue {
  ValueTag tag;
};

// StructValue --
//   STRUCT_VALUE
//
// Represents a struct value.
//
// Packings:
// * Unit is packed as 1.
typedef struct {
  FbleValue _base;
  size_t fieldc;
  FbleValue* fields[];
} StructValue;

// UnionValue --
//   UNION_VALUE
//
// Represents a union value.
//
// Packings:
// * If the argument is Unit, we pack the union as {tag, 1}.
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} UnionValue;

// FuncValue -- FUNC_VALUE
//
// Fields:
//   executable - The code for the function.
//   profile_base_id - An offset to use for profile blocks referenced from this
//                     function.
//   statics - static variables captured by the function.
//             Size is executable->statics
//
// Note: Function values are used for both pure functions and processes. We
// don't distinguish between the two at runtime, except that
// executable->args == 0 suggests this is for a process instead of a function.
typedef struct {
  FbleValue _base;
  FbleExecutable* executable;
  size_t profile_base_id;
  FbleValue* statics[];
} FuncValue;

// Values --
//   A non-circular singly linked list of values.
typedef struct Values {
  FbleValue* value;
  struct Values* next;
} Values;

// LinkValue -- LINK_VALUE
//   Holds the list of values on a link. Values are added to the tail and taken
//   from the head. If there are no values on the list, both head and tail are
//   set to NULL.
typedef struct {
  FbleValue _base;
  Values* head;
  Values* tail;
} LinkValue;

// PortValue --
//   PORT_VALUE
//
// Use for input and output values linked to external IO.
//
// Fields:
//   data - a pointer to a value owned externally where data should be put to
//          and got from.
typedef struct {
  FbleValue _base;
  FbleValue** data;
} PortValue;

// RefValue --
//   REF_VALUE
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
  FbleValue _base;
  FbleValue* value;
} RefValue;

// NewValue --
//   Allocate a new value of the given type.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//
// Results:
//   The newly allocated value. The value does not have its tag initialized.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define NewValue(heap, T) ((T*) FbleNewHeapObject(heap, sizeof(T)))

// NewValueExtra --
//   Allocate a new value of the given type with some extra space.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//   size - the number of bytes of extra space to include in the allocated
//   object.
//
// Results:
//   The newly allocated value with extra space.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define NewValueExtra(heap, T, size) ((T*) FbleNewHeapObject(heap, sizeof(T) + size))

FbleValue* FbleGenericTypeValue = (FbleValue*)1;
static FbleValue* UnitValue = (FbleValue*)1;

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);

static FbleExecStatus GetRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void GetAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleExecStatus PutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void PutAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleExecStatus PartialPutRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void PartialPutAbortFunction(FbleValueHeap* heap, FbleStack* stack);

static FbleValue* NewGetValue(FbleValueHeap* heap, FbleValue* port, FbleBlockId profile);
static FbleValue* NewPutValue(FbleValueHeap* heap, FbleValue* link, FbleBlockId profile);


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
  if (!PACKED(value)) {
    FbleRetainHeapObject(heap, value);
  }
}

// FbleReleaseValue -- see documentation in fble-value.h
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
{
  if (!PACKED(value) && value != NULL) {
    FbleReleaseHeapObject(heap, value);
  }
}

// FbleValueAddRef -- see documentation in fble-value.h
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  if (!PACKED(src) && !PACKED(dst)) {
    FbleHeapObjectAddRef(heap, src, dst);
  }
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
    case STRUCT_VALUE: return;
    case UNION_VALUE: return;

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      FbleFreeExecutable(v->executable);
      return;
    }

    case LINK_VALUE: {
      LinkValue* v = (LinkValue*)value;
      Values* curr = v->head;
      while (curr != NULL) {
        Values* tmp = curr;
        curr = curr->next;
        FbleFree(tmp);
      }
      return;
    }

    case PORT_VALUE: return;
    case REF_VALUE: return;
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
  if (!PACKED(value) && value != NULL) {
    callback->callback(callback, value);
  }
}

// Refs --
//   The 'refs' function for values. See documentation in heap.h
static void Refs(FbleHeapCallback* callback, FbleValue* value)
{
  switch (value->tag) {
    case STRUCT_VALUE: {
      StructValue* sv = (StructValue*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        Ref(callback, sv->fields[i]);
      }
      break;
    }

    case UNION_VALUE: {
      UnionValue* uv = (UnionValue*)value;
      Ref(callback, uv->arg);
      break;
    }

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      for (size_t i = 0; i < v->executable->statics; ++i) {
        Ref(callback, v->statics[i]);
      }
      break;
    }

    case LINK_VALUE: {
      LinkValue* v = (LinkValue*)value;
      for (Values* elem = v->head; elem != NULL; elem = elem->next) {
        Ref(callback, elem->value);
      }
      break;
    }

    case PORT_VALUE: {
      break;
    }

    case REF_VALUE: {
      RefValue* v = (RefValue*)value;
      Ref(callback, v->value);
      break;
    }
  }
}

// FbleNewStructValue -- see documentation in fble-value.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, ...)
{
  if (argc == 0) {
    return UnitValue;
  }

  StructValue* value = NewValueExtra(heap, StructValue, sizeof(FbleValue*) * argc);
  value->_base.tag = STRUCT_VALUE;
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

// FbleNewStructValue_ -- see documentation in fble-value.h
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  if (argc == 0) {
    return UnitValue;
  }

  StructValue* value = NewValueExtra(heap, StructValue, sizeof(FbleValue*) * argc);
  value->_base.tag = STRUCT_VALUE;
  value->fieldc = argc;

  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[i];
    value->fields[i] = arg;
    if (arg != NULL) {
      FbleValueAddRef(heap, &value->_base, arg);
    }
  }

  return &value->_base;
}

// FbleStructValueAccess -- see documentation in fble-value.h
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field)
{
  object = FbleStrictValue(object);
  assert(object != NULL && object->tag == STRUCT_VALUE);
  StructValue* value = (StructValue*)object;
  assert(field < value->fieldc);
  return value->fields[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg)
{
  if (arg == UnitValue) {
    intptr_t packed = (tag << 1) | 1;
    return (FbleValue*)packed;
  }

  UnionValue* union_value = NewValue(heap, UnionValue);
  union_value->_base.tag = UNION_VALUE;
  union_value->tag = tag;
  union_value->arg = arg;
  FbleValueAddRef(heap, &union_value->_base, arg);
  return &union_value->_base;
}
// FbleNewEnumValue -- see documentation in fble-value.h
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag)
{
  intptr_t packed = (tag << 1) | 1;
  return (FbleValue*)packed;
}

// FbleUnionValueTag -- see documentation in fble-value.h
size_t FbleUnionValueTag(FbleValue* object)
{
  object = FbleStrictValue(object);

  if (PACKED(object)) {
    size_t packed = (size_t)object;
    return packed >> 1;
  };

  assert(object != NULL && object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue* FbleUnionValueAccess(FbleValue* object)
{
  object = FbleStrictValue(object);

  if (PACKED(object)) {
    return UnitValue;
  };

  assert(object != NULL && object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return value->arg;
}

// FbleNewFuncValue -- see documentation in value.h
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id)
{
  FuncValue* v = NewValueExtra(heap, FuncValue, sizeof(FbleValue*) * executable->statics);
  v->_base.tag = FUNC_VALUE;
  v->profile_base_id = profile_base_id;
  v->executable = executable;
  v->executable->refcount++;
  return &v->_base;
}

// FbleFuncValueStatics -- see documentation in value.h
FbleValue** FbleFuncValueStatics(FbleValue* func)
{
  FuncValue* func_value = (FuncValue*)func;
  return func_value->statics;
}

// FbleFuncValueProfileBaseId -- see documentation in value.h
size_t FbleFuncValueProfileBaseId(FbleValue* func)
{
  FuncValue* func_value = (FuncValue*)func;
  return func_value->profile_base_id;
}

// FbleFuncValueExecutable -- see documentation in value.h
FbleExecutable* FbleFuncValueExecutable(FbleValue* func)
{
  FuncValue* func_value = (FuncValue*)func;
  return func_value->executable;
}

// FbleIsProcValue -- see documentation in fble-value.h
bool FbleIsProcValue(FbleValue* value)
{
  if (!PACKED(value)) {
    FuncValue* proc = (FuncValue*)value;
    return value->tag == FUNC_VALUE && proc->executable->args == 0;
  }
  return false;
}

// GetRunFunction --
//   FbleExecutable.run function for Get value.
//
// See documentation of FbleExecutable.run in execute.h.
static FbleExecStatus GetRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleValue* get_port = FbleFuncValueStatics(thread->stack->func)[0];
  if (get_port->tag == LINK_VALUE) {
    LinkValue* link = (LinkValue*)get_port;

    if (link->head == NULL) {
      // Blocked on get.
      return FBLE_EXEC_BLOCKED;
    }

    Values* head = link->head;
    link->head = link->head->next;
    if (link->head == NULL) {
      link->tail = NULL;
    }

    FbleRetainValue(heap, head->value);
    FbleThreadReturn(heap, thread, head->value);
    FbleFree(head);
    return FBLE_EXEC_FINISHED;
  }

  assert(get_port->tag == PORT_VALUE);
  PortValue* port = (PortValue*)get_port;
  if (*port->data == NULL) {
    // Blocked on get.
    return FBLE_EXEC_BLOCKED;
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
  FbleValue** statics = FbleFuncValueStatics(thread->stack->func);
  FbleValue* put_port = statics[0];
  FbleValue* arg = statics[1];
  if (put_port->tag == LINK_VALUE) {
    LinkValue* link = (LinkValue*)put_port;

    Values* tail = FbleAlloc(Values);
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

  PortValue* port = (PortValue*)put_port;
  if (*port->data != NULL) {
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
  *(stack->result) = NULL;
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

  FbleValue* put = FbleNewFuncValue(heap, &executable, FbleFuncValueProfileBaseId(thread->stack->func) + 1);
  FbleValue** statics = FbleFuncValueStatics(put);

  FbleValue* link = FbleFuncValueStatics(thread->stack->func)[0];
  FbleValue* arg = thread->stack->locals[0];
  statics[0] = link;
  FbleValueAddRef(heap, put, link);
  statics[1] = arg;
  FbleValueAddRef(heap, put, arg);

  FbleReleaseValue(heap, thread->stack->locals[0]);
  FbleThreadReturn(heap, thread, put);
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
static FbleValue* NewGetValue(FbleValueHeap* heap, FbleValue* port, FbleBlockId profile)
{
  assert(port->tag == LINK_VALUE || port->tag == PORT_VALUE);

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

  FbleValue* get = FbleNewFuncValue(heap, &executable, profile);
  FbleValue** statics = FbleFuncValueStatics(get);
  statics[0] = port;
  FbleValueAddRef(heap, get, port);
  return get;
}

// FbleNewInputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewInputPortValue(FbleValueHeap* heap, FbleValue** data, FbleBlockId profile)
{
  PortValue* get_port = NewValue(heap, PortValue);
  get_port->_base.tag = PORT_VALUE;
  get_port->data = data;

  FbleValue* get = NewGetValue(heap, &get_port->_base, profile);
  FbleReleaseValue(heap, &get_port->_base);
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
static FbleValue* NewPutValue(FbleValueHeap* heap, FbleValue* link, FbleBlockId profile)
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

  FbleValue* put = FbleNewFuncValue(heap, &executable, profile);
  FbleValue** statics = FbleFuncValueStatics(put);
  statics[0] = link;
  FbleValueAddRef(heap, put, link);
  return put;
}

// FbleNewLinkValue -- see documentation in value.h
void FbleNewLinkValue(FbleValueHeap* heap, FbleBlockId profile, FbleValue** get, FbleValue** put)
{
  LinkValue* link = NewValue(heap, LinkValue);
  link->_base.tag = LINK_VALUE;
  link->head = NULL;
  link->tail = NULL;

  *get = NewGetValue(heap, &link->_base, profile);
  *put = NewPutValue(heap, &link->_base, profile + 1);
  FbleReleaseValue(heap, &link->_base);
}

// FbleNewOutputPortValue -- see documentation in fble-value.h
FbleValue* FbleNewOutputPortValue(FbleValueHeap* heap, FbleValue** data, FbleBlockId profile)
{
  PortValue* port_value = NewValue(heap, PortValue);
  port_value->_base.tag = PORT_VALUE;
  port_value->data = data;
  FbleValue* put = NewPutValue(heap, &port_value->_base, profile);
  FbleReleaseValue(heap, &port_value->_base);
  return put;
}

// FbleNewListValue -- see documentation in value.h
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  FbleValue* unit = FbleNewStructValue(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, unit);
  FbleReleaseValue(heap, unit);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[argc - i - 1];
    FbleValue* cons = FbleNewStructValue(heap, 2, arg, tail);
    FbleReleaseValue(heap, tail);

    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
  }
  return tail;
}

// FbleNewLiteralValue -- see documentation in value.h
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args)
{
  FbleValue* unit = FbleNewStructValue(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, unit);
  FbleReleaseValue(heap, unit);
  for (size_t i = 0; i < argc; ++i) {
    size_t letter = args[argc - i - 1];
    FbleValue* arg = FbleNewUnionValue(heap, letter, unit);
    FbleValue* cons = FbleNewStructValue(heap, 2, arg, tail);
    FbleReleaseValue(heap, arg);
    FbleReleaseValue(heap, tail);

    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
  }
  return tail;
}

// FbleNewRefValue -- see documentation in value.h
FbleValue* FbleNewRefValue(FbleValueHeap* heap)
{
  RefValue* rv = NewValue(heap, RefValue);
  rv->_base.tag = REF_VALUE;
  rv->value = NULL;
  return &rv->_base;
}

// FbleAssignRefValue -- see documentation in value.h
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue* ref, FbleValue* value)
{
  // Unwrap any accumulated layers of references on the value and make sure we
  // aren't forming a vacuous value.
  RefValue* unwrap = (RefValue*)value;
  while (!PACKED(value) && value->tag == REF_VALUE && unwrap->value != NULL) {
    value = unwrap->value;
    unwrap = (RefValue*)value;
  }

  if (value == ref) {
    return false;
  }

  RefValue* rv = (RefValue*)ref;
  assert(rv->_base.tag == REF_VALUE);
  rv->value = value;
  FbleValueAddRef(heap, ref, value);
  return true;
}

// FbleStrictValue -- see documentation in value.h
FbleValue* FbleStrictValue(FbleValue* value)
{
  while (!PACKED(value) && value != NULL && value->tag == REF_VALUE) {
    RefValue* ref = (RefValue*)value;
    value = ref->value;
  }
  return value;
}
