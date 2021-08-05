// value.c
//   This file implements routines associated with fble values.

#include "value.h"

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL

#include "fble-alloc.h"   // for FbleAlloc, FbleFree, etc.
#include "fble-vector.h"  // for FbleVectorInit, etc.

#include "heap.h"
#include "execute.h"

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
  DATA_TYPE_VALUE,
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

// DataTypeValue --
//   DATA_TYPE_VALUE
//
// Represents a struct or union type value.
//
// Fields:
//   tag_size - the number of bits required for the tag, 0 if this
//              represents a struct type instead of a union type.
//   fieldc - the number of fields.
//   fields - the type of each field.
typedef struct {
  FbleValue _base;
  size_t tag_size;
  size_t fieldc;
  FbleValue* fields[];
} DataTypeValue;

// StructValue --
//   STRUCT_VALUE
//
// Represents a struct value.
//
// Packings:
//   Struct values may be packed. If so, read in order from the least
//   significant bits:
//   '0' - to indicate it is a struct value instead of a union value.
//   <num_args> - unary encoded number of arguments in the struct.
//     e.g. "0" for 0 args, "10" for 1 arg, "110" for 2 args, and so on.
//   <args> in order of argument arg0, arg1, etc.
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
//   Union values may be packed. If so, read in order from the least
//   significant bit:
//   '1' - to indicate it is a union value instead of a struct value.
//   <tag> - the tag, using a unary encoding terminated with 0.
//     e.g. "0" for 0, "10" for 1, "110" for 2, and so on.
//   <arg> - the argument.
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

// FbleGenericTypeValue -- see documentation in fble-value.h
//
// Note: the packed value for a generic type matches the packed value of a
// zero-argument struct value, so that it can be packed along with union and
// struct values.
FbleValue* FbleGenericTypeValue = (FbleValue*)1;

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);

static size_t PackedValueLength(intptr_t data);

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
    case DATA_TYPE_VALUE: return;
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
    case DATA_TYPE_VALUE: {
      DataTypeValue* t = (DataTypeValue*)value;
      for (size_t i = 0; i < t->fieldc; ++i) {
        Ref(callback, t->fields[i]);
      }
      break;
    }

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

// PackedValueLength --
//   Compute the number of bits needed for the value packed into the least
//   significat bits of 'data'. 'data' should not include the pack marker.
//
// Inputs:
//   Raw data bits for a packed value without the pack marker.
//
// Output:
//   The number of bits of the data used to describe that value.
static size_t PackedValueLength(intptr_t data)
{
  size_t len = 0;
  if ((data & 0x1) == 0) {
    // Struct value
    data >>= 1; len++;     // struct value marker
    size_t argc = 0;
    while (data & 0x1) {
      data >>= 1; len++;   // unary encoding of number of fields.
      argc++;
    }
    data >>= 1; len++;   // number of fields terminator.

    for (size_t i = 0; i < argc; ++i) {
      size_t arglen = PackedValueLength(data);
      data >>= arglen; len += arglen;
    }
    return len;
  } else {
    // Union value
    data >>= 1; len++;      // union value marker
    while (data & 0x1) {
      data >>= 1; len++;    // unary encoding of tag.
    };
    data >>= 1; len++;      // tag terminator.
    return len + PackedValueLength(data);
  }
}

// FbleNewStructValue -- see documentation in fble-value.h
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, ...)
{
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, argc);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  return FbleNewStructValue_(heap, argc, args);
}

// FbleNewStructValue_ -- see documentation in fble-value.h
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  // Try packing optimistically.
  intptr_t data = 0;
  size_t num_bits = 0;
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[argc-i-1];
    if (PACKED(arg)) {
      intptr_t argdata = (intptr_t)arg;
      argdata >>= 1;   // skip past pack marker.
      size_t arglen = PackedValueLength(argdata);
      num_bits += arglen;
      intptr_t mask = ((intptr_t)1 << arglen) - 1;
      data = (data << arglen) | (mask & argdata);
    } else {
      num_bits += 8 * sizeof(FbleValue*);
      break;
    }
  }

  if (num_bits + argc + 1 < 8 * sizeof(FbleValue*)) {
    // We have enough space to pack the struct value.
    data <<= 1;   // num args '0' terminator.
    for (size_t i = 0; i < argc; ++i) {
      data = (data << 1) | 1;     // unary encoding of number of args.
    }
    data = (data << 2) | 1;       // struct value and pack marks.
    return (FbleValue*)data;
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

  if (PACKED(object)) {
    intptr_t data = (intptr_t)object;

    // Skip past the pack bit and the bit saying this is a struct value.
    data >>= 2;

    // Skip past the number of args.
    while (data & 0x1) {
      data >>= 1;
    }
    data >>= 1;

    // Skip past any args before the field we want.
    for (size_t i = 0; i < field; ++i) {
      data >>= PackedValueLength(data);
    }

    // Pack the result.
    data = (data << 1) | 1;
    return (FbleValue*)data;
  }

  assert(object != NULL && object->tag == STRUCT_VALUE);
  StructValue* value = (StructValue*)object;
  assert(field < value->fieldc);
  return value->fields[field];
}

// FbleNewUnionValue -- see documentation in fble-value.h
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg)
{
  if (PACKED(arg)) {
    intptr_t data = (intptr_t)arg;
    data >>= 1;

    if (PackedValueLength(data) + tag + 1 < 8*sizeof(FbleValue*)) {
      data <<= 1;   // tag '0' terminator.
      for (size_t i = 0; i < tag; ++i) {
        data = (data << 1) | 1; // unary encoded tag
      }
      data = (data << 2) | 3;   // union value and packed markers.
      return (FbleValue*)data;
    }
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
  FbleValue* unit = FbleNewStructValue(heap, 0);
  FbleValue* result = FbleNewUnionValue(heap, tag, unit);
  FbleReleaseValue(heap, unit);
  return result;
}

// FbleUnionValueTag -- see documentation in fble-value.h
size_t FbleUnionValueTag(FbleValue* object)
{
  object = FbleStrictValue(object);

  if (PACKED(object)) {
    intptr_t data = (intptr_t)object;

    // Skip past the pack bit and the bit saying this is a union value.
    data >>= 2;

    // Read the tag.
    size_t tag = 0;
    while (data & 0x1) {
      tag++;
      data >>= 1;
    }
    return tag;
  }

  assert(object != NULL && object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return value->tag;
}

// FbleUnionValueAccess -- see documentation in fble-value.h
FbleValue* FbleUnionValueAccess(FbleValue* object)
{
  object = FbleStrictValue(object);

  if (PACKED(object)) {
    intptr_t data = (intptr_t)object;

    // Skip past the pack bit and the bit saying this is a union value.
    data >>= 2;

    // Skip passed the tag.
    while (data & 0x1) {
      data >>= 1;
    }
    data >>= 1;

    // Pack the result
    data = (data << 1) | 1;  // packed marker
    return (FbleValue*)data;
  }

  assert(object != NULL && object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return value->arg;
}

// FbleNewDataTypeValue -- see documentation in value.h
FbleValue* FbleNewDataTypeValue(FbleValueHeap* heap, FbleDataTypeTag kind, size_t fieldc, FbleValue** fields)
{
  size_t tag_size = 0;
  switch (kind) {
    case FBLE_STRUCT_DATATYPE:
      tag_size = 0;
      break;

    case FBLE_UNION_DATATYPE:
      tag_size = 1;
      while ((1 << tag_size) <= fieldc) {
        tag_size++;
      }
      break;
  }

  DataTypeValue* value = NewValueExtra(heap, DataTypeValue, sizeof(FbleValue*) * fieldc);
  value->_base.tag = DATA_TYPE_VALUE;
  value->tag_size = tag_size;
  value->fieldc = fieldc;
  for (size_t i = 0; i < fieldc; ++i) {
    value->fields[i] = fields[i];
    if (fields[i] != NULL) {
      FbleValueAddRef(heap, &value->_base, fields[i]);
    }
  }

  return &value->_base;
}

// FbleNewFuncValue -- see documentation in value.h
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id, FbleValue** statics)
{
  FuncValue* v = NewValueExtra(heap, FuncValue, sizeof(FbleValue*) * executable->statics);
  v->_base.tag = FUNC_VALUE;
  v->profile_base_id = profile_base_id;
  v->executable = executable;
  v->executable->refcount++;
  for (size_t i = 0; i < executable->statics; ++i) {
    v->statics[i] = statics[i];
    FbleValueAddRef(heap, &v->_base, statics[i]);
  }
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

  FbleValue* link = FbleFuncValueStatics(thread->stack->func)[0];
  FbleValue* arg = thread->stack->locals[0];
  FbleValue* statics[2] = { link, arg };
  FbleValue* put = FbleNewFuncValue(heap, &executable, FbleFuncValueProfileBaseId(thread->stack->func) + 1, statics);

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

  FbleValue* statics[1] = { port };
  return FbleNewFuncValue(heap, &executable, profile, statics);
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

  FbleValue* statics[1] = { link };
  return FbleNewFuncValue(heap, &executable, profile, statics);
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
