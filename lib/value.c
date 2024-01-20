/**
 * @file value.c
 *  FbleValue routines.
 */

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL
#include <string.h>   // for memcpy

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-function.h>  // for FbleFreeExecutable
#include <fble/fble-value.h>     // for FbleValue, etc.

#include "unreachable.h"    // for FbleUnreachable

typedef struct Frame Frame;

struct Frame {
  intptr_t top;           /**< Next FbleValue to allocate on this stack frame. */
  FbleValue* natives;     /**< Singly linked list of native values on this frame. */
  Frame* caller;          /**< The caller's frame. */
  Frame* alternate;       /**< An alternate frame. */
};

struct FbleValueHeap {
  Frame* stack;
};

/** Different kinds of FbleValue. */
typedef enum {
  STRUCT_VALUE,
  UNION_VALUE,
  FUNC_VALUE,
  REF_VALUE,
  NATIVE_VALUE,
  TRAVERSED_VALUE,
} ValueTag;

/**
 * Base class for fble values.
 *
 * A tagged union of value types. All values have the same initial layout as
 * FbleValue. The tag can be used to determine what kind of value this is to
 * get access to additional fields of the value by first casting to that
 * specific type of value.
 *
 * IMPORTANT: Some fble values are packed directly in the FbleValue* pointer
 * to save space. An FbleValue* only points to an FbleValue if the least
 * significant bit of the pointer is 0.
 */
struct FbleValue {
  ValueTag tag;   /**< The kind of value. */
};

/**
 * STRUCT_VALUE: An fble struct value.
 *
 * Packings:
 *   Struct values may be packed. If so, read in order from the least
 *   significant bits:
 *   '0' - to indicate it is a struct value instead of a union value.
 *   'num_args' - unary encoded number of arguments in the struct.
 *     e.g. "0" for 0 args, "10" for 1 arg, "110" for 2 args, and so on.
 *   'args' in order of argument arg0, arg1, etc.
 */
typedef struct {
  FbleValue _base;        /**< FbleValue base class. */
  size_t fieldc;          /**< Number of fields. */
  FbleValue* fields[];    /**< Field values. */
} StructValue;

/**
 * UNION_VALUE: An fble union value.
 *
 * Packings:
 *   Union values may be packed. If so, read in order from the least
 *   significant bit:
 *   '1' - to indicate it is a union value instead of a struct value.
 *   'tag' - the tag, using a unary encoding terminated with 0.
 *     e.g. "0" for 0, "10" for 1, "110" for 2, and so on.
 *   'arg' - the argument.
 */
typedef struct {
  FbleValue _base;    /**< FbleValue base class. */
  size_t tag;         /**< Union tag value. */
  FbleValue* arg;     /**< Union argument value. */
} UnionValue;

/**
 * FUNC_VALUE: An fble function value.
 */
typedef struct {
  FbleValue _base;              /**< FbleValue base class. */
  FbleFunction function;        /**< Function information. */
  FbleValue* statics[];         /**< Storage location for static variables. */
} FuncValue;

/**
 * REF_VALUE: A reference value.
 *
 * An implementation-specific value introduced to support recursive values and
 * not yet computed values.
 *
 * A ref value holds a reference to another value. All values must be
 * dereferenced before being otherwise accessed in case they are ref
 * values.
 */
typedef struct {
  FbleValue _base;      /**< FbleValue base class. */
  FbleValue* value;     /**< The referenced value, or NULL. */
} RefValue;

/**
 * NATIVE_VALUE: GC tracked native allocation.
 */
typedef struct {
  FbleValue _base;              /**< FbleValue base class. */
  void* data;                   /**< User data. */               
  void (*on_free)(void* data);  /**< Destructor for user data. */
  FbleValue* tail;              /**< Singly linked list of native values in this frame. */
} NativeValue;

/**
 * TRAVERSED_VALUE: Marker used during traversal.
 */
typedef struct {
  FbleValue _base;             /**< FbleValue base class. */
  FbleValue* dest;             /**< Where the value was copied to. */
} TraversedValue;

/**
 * Allocates a new value of the given type.
 *
 * @param heap  The heap to allocate the value on
 * @param T  The type of the value
 *
 * @returns
 *   The newly allocated value. The value does not have its tag initialized.
 *
 * @sideeffects
 *   Allocates a value that should be released when it is no longer needed.
 */
#define NewValue(heap, T) ((T*) NewValueRaw(heap, sizeof(T)))

/**
 * Allocates a new value with some extra space.
 *
 * @param heap  The heap to allocate the value on
 * @param T  The type of the value
 * @param count  The number of FbleValue* worth of extra space to include in
 *   the allocated object.
 *
 * @returns
 *   The newly allocated value with extra space.
 *
 * @sideeffects
 *   Allocates a value that should be released when it is no longer needed.
 */
#define NewValueExtra(heap, T, count) ((T*) NewValueRaw(heap, sizeof(T) + count * sizeof(FbleValue*)))

// See documentation in fble-value.h
//
// Note: the packed value for a generic type matches the packed value of a
// zero-argument struct value, so that it can be packed along with union and
// struct values.
FbleValue* FbleGenericTypeValue = (FbleValue*)1;

static void* NewValueRaw(FbleValueHeap* heap, size_t size);
static void FreeNatives(FbleValue* natives);
static FbleValue* Traverse(FbleValueHeap* heap, FbleValue* base, FbleValue* top, FbleValue* value);

static bool IsPacked(FbleValue* value);
static size_t PackedValueLength(intptr_t data);
static FbleValue* StrictValue(FbleValue* value);

// See documentation in fble-value.h.
FbleValueHeap* FbleNewValueHeap()
{
  // TODO: Come up with a better way to size these initial allocations.
  // For now, 256MB each should fit in the 1GB available on my pi.
  size_t stack_size = 256 * 1024 * 1024;
  Frame* caller = FbleAllocExtra(Frame, stack_size);
  caller->top = (intptr_t)(caller + 1);
  caller->natives = NULL;
  caller->caller = NULL;
  caller->alternate = NULL;

  Frame* alternate = FbleAllocExtra(Frame, stack_size);
  alternate->top = (intptr_t)(alternate + 1);
  alternate->natives = NULL;
  alternate->caller = NULL;
  alternate->alternate = NULL;

  Frame* stack = FbleAllocExtra(Frame, stack_size);
  stack->top = (intptr_t)(stack + 1);
  stack->natives = NULL;
  stack->caller = caller;
  stack->alternate = alternate;
  
  FbleValueHeap* heap = FbleAlloc(FbleValueHeap);
  heap->stack = stack;

  return heap;
}

// See documentation in fble-value.h.
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  Frame* caller = heap->stack->caller;
  assert(caller->natives == NULL && "Mismatched FblePushFrame/FblePopFrame");
  assert(caller->caller == NULL && "Mismatched FblePushFrame/FblePopFrame");
  assert(caller->alternate == NULL && "Mismatched FblePushFrame/FblePopFrame");
  FbleFree(caller);

  Frame* alternate = heap->stack->alternate;
  assert(alternate->natives == NULL && "Mismatched FblePushFrame/FblePopFrame");
  assert(alternate->caller == NULL && "Mismatched FblePushFrame/FblePopFrame");
  assert(alternate->alternate == NULL && "Mismatched FblePushFrame/FblePopFrame");
  FbleFree(alternate);

  Frame* stack = heap->stack;
  FreeNatives(stack->natives);
  FbleFree(stack);

  FbleFree(heap);
}

// See documentation in fble-value.h
void FblePushFrame(FbleValueHeap* heap)
{
  Frame* stack = heap->stack;

  Frame* new = (Frame*)stack->alternate->top;
  new->top = (intptr_t)(new + 1);
  new->natives = NULL;
  new->caller = stack;
  new->alternate = stack->caller;

  heap->stack = new;
}

// See documentation in fble-value.h
FbleValue* FblePopFrame(FbleValueHeap* heap, FbleValue* value)
{
  Frame* popped = heap->stack;
  heap->stack = popped->caller;

  FbleValue* base = (FbleValue*)(popped + 1);
  FbleValue* top = (FbleValue*)popped->top;
  FbleValue* nv = Traverse(heap, base, top, value);

  FreeNatives(popped->natives);
  return nv;
}

// See documentation in fble-value.h
void FbleCompactFrame(FbleValueHeap* heap, size_t n, FbleValue** save)
{
  Frame* popped = heap->stack;

  FbleValue* base = (FbleValue*)(popped + 1);
  FbleValue* top = (FbleValue*)popped->top;

  Frame* new = (Frame*)popped->alternate->top;
  new->top = (intptr_t)(new + 1);
  new->natives = NULL;
  new->caller = popped->caller;
  new->alternate = popped;

  heap->stack = new;

  for (size_t i = 0; i < n; ++i) {
    save[i] = Traverse(heap, base, top, save[i]);
  }
  FreeNatives(popped->natives);
}

static void* NewValueRaw(FbleValueHeap* heap, size_t size)
{
  Frame* stack = heap->stack;
  intptr_t value = stack->top;
  stack->top += size;
  return (void*)value;
}

static void FreeNatives(FbleValue* natives)
{
  while (natives != NULL) {
    NativeValue* native = (NativeValue*)natives;
    if (native->_base.tag == NATIVE_VALUE) {
      if (native->on_free != NULL) {
        native->on_free(native->data);
      }
    }
    natives = native->tail;
  }
}

/**
 * @func[Traverse] Copies a value from to the current' frame.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][base] The base of the frame we are popping from.
 *  @arg[FbleValue*][top] The top of the frame we are popping from.
 *  @arg[FbleValue*][value] The value on the popped frame to copy.
 *  @returns[FbleValue*] The new address for value.
 *  @sideeffects
 *   Copies value and anything it depends on from the popped frame to the
 *   current frame.
 */
static FbleValue* Traverse(FbleValueHeap* heap, FbleValue* base, FbleValue* top, FbleValue* value)
{
  if (IsPacked(value) || value < base || value >= top) {
    return value;
  }

  switch (value->tag) {
    case STRUCT_VALUE: {
      StructValue* v = (StructValue*)value;
      StructValue* nv = NewValueExtra(heap, StructValue, v->fieldc);
      nv->_base.tag = STRUCT_VALUE;
      nv->fieldc = v->fieldc;

      TraversedValue* t = (TraversedValue*)v;
      t->_base.tag = TRAVERSED_VALUE;
      t->dest = &nv->_base;

      for (size_t i = 0; i < nv->fieldc; ++i) {
        nv->fields[i] = Traverse(heap, base, top, v->fields[i]);
      }

      return &nv->_base;
    }

    case UNION_VALUE: {
      UnionValue* v = (UnionValue*)value;
      UnionValue* nv = NewValue(heap, UnionValue);
      nv->_base.tag = UNION_VALUE;
      nv->tag = v->tag;

      TraversedValue* t = (TraversedValue*)v;
      t->_base.tag = TRAVERSED_VALUE;
      t->dest = &nv->_base;

      nv->arg = Traverse(heap, base, top, v->arg);
      return &nv->_base;
    }

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      size_t num_statics = v->function.executable.num_statics;
      FuncValue* nv = NewValueExtra(heap, FuncValue, num_statics);
      nv->_base.tag = FUNC_VALUE;
      nv->function.executable = v->function.executable;
      nv->function.profile_block_id = v->function.profile_block_id;
      nv->function.statics = nv->statics;

      TraversedValue* t = (TraversedValue*)v;
      t->_base.tag = TRAVERSED_VALUE;
      t->dest = &nv->_base;

      for (size_t i = 0; i < num_statics; ++i) {
        nv->statics[i] = Traverse(heap, base, top, v->statics[i]);
      }

      return &nv->_base;
    }

    case REF_VALUE: {
      RefValue* v = (RefValue*)value;
      RefValue* nv = NewValue(heap, RefValue);
      nv->_base.tag = REF_VALUE;

      FbleValue* ref_value = v->value;

      TraversedValue* t = (TraversedValue*)v;
      t->_base.tag = TRAVERSED_VALUE;
      t->dest = &nv->_base;

      nv->value = Traverse(heap, base, top, ref_value);
      return &nv->_base;
    }

    case NATIVE_VALUE: {
      NativeValue* v = (NativeValue*)value;
      NativeValue* nv = NewValue(heap, NativeValue);

      nv->_base.tag = NATIVE_VALUE;
      nv->data = v->data;
      nv->on_free = v->on_free;
      nv->tail = heap->stack->natives;
      heap->stack->natives = &nv->_base;

      TraversedValue* t = (TraversedValue*)v;
      t->_base.tag = TRAVERSED_VALUE;
      t->dest = &nv->_base;

      return &nv->_base;
    }

    case TRAVERSED_VALUE: {
      TraversedValue* v = (TraversedValue*)value;
      return v->dest;
    }
  }

  FbleUnreachable("should never get here");
  return NULL;
}

/**
 * @func[IsPacked] Tests whether a value is packed into an FbleValue* pointer.
 *  @arg[FbleValue*][value] The value to test.
 *  @returns[bool] True if the value is a packaged value, false otherwise.
 */
static bool IsPacked(FbleValue* value)
{
  return (((intptr_t)value) & 1);
}

/**
 * Computes bits needed for a packed value.
 *
 * Compute the number of bits needed for the value packed into the least
 * significat bits of 'data'. 'data' should not include the pack marker.
 *
 * @param data  Raw data bits for a packed value without the pack marker.
 *
 * @returns
 *   The number of bits of the data used to describe that value.
 *
 * @sideeffects
 *   None.
 */
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

/**
 * @func[StrictValue] Removes layers of refs from a value.
 *  Gets the strict value associated with the given value, which will either
 *  be the value itself, or the dereferenced value if the value is a
 *  reference.
 *
 *  @arg[FbleValue*][value] The value to get the strict version of.
 *
 *  @returns FbleValue*
 *   The value with all layers of reference indirection removed. NULL if the
 *   value is a reference that has no value yet.
 *
 *  @sideeffects
 *   None.
 */
static FbleValue* StrictValue(FbleValue* value)
{
  while (!IsPacked(value) && value != NULL && value->tag == REF_VALUE) {
    RefValue* ref = (RefValue*)value;
    value = ref->value;
  }
  return value;
}

// See documentation in fble-value.h.
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  // Try packing optimistically.
  intptr_t data = 0;
  size_t num_bits = 0;
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[argc-i-1];
    if (IsPacked(arg)) {
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

  StructValue* value = NewValueExtra(heap, StructValue, argc);
  value->_base.tag = STRUCT_VALUE;
  value->fieldc = argc;

  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[i];
    value->fields[i] = arg;
  }

  return &value->_base;
}

// See documentation in fble-value.h.
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, ...)
{
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, argc);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  return FbleNewStructValue(heap, argc, args);
}

// See documentation in fble-value.h.
FbleValue* FbleStructValueField(FbleValue* object, size_t field)
{
  object = StrictValue(object);

  if (object == NULL) {
    return NULL;
  }

  if (IsPacked(object)) {
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

  assert(object->tag == STRUCT_VALUE);
  StructValue* value = (StructValue*)object;
  assert(field < value->fieldc);
  return value->fields[field];
}

// See documentation in fble-value.h.
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg)
{
  if (IsPacked(arg)) {
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
  return &union_value->_base;
}
// See documentation in fble-value.h.
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewUnionValue(heap, tag, unit);
  return result;
}

// See documentation in fble-value.h.
size_t FbleUnionValueTag(FbleValue* object)
{
  object = StrictValue(object);

  if (object == NULL) {
    return (size_t)(-1);
  }

  if (IsPacked(object)) {
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

  assert(object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return value->tag;
}

// See documentation in fble-value.h.
FbleValue* FbleUnionValueArg(FbleValue* object)
{
  object = StrictValue(object);

  if (object == NULL) {
    return NULL;
  }

  if (IsPacked(object)) {
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

  assert(object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return value->arg;
}

// See documentation in fble-value.h.
FbleValue* FbleUnionValueField(FbleValue* object, size_t field)
{
  object = StrictValue(object);

  if (object == NULL) {
    return NULL;
  }

  if (IsPacked(object)) {
    intptr_t data = (intptr_t)object;

    // Skip past the pack bit and the bit saying this is a union value.
    data >>= 2;

    // Read the tag.
    size_t tag = 0;
    while (data & 0x1) {
      tag++;
      data >>= 1;
    }

    if (tag != field) {
      return FbleWrongUnionTag;
    }

    data >>= 1;

    // Pack the result
    data = (data << 1) | 1;  // packed marker
    return (FbleValue*)data;
  }

  assert(object->tag == UNION_VALUE);
  UnionValue* value = (UnionValue*)object;
  return (value->tag == field) ? value->arg : FbleWrongUnionTag;
}
// See documentation in fble-value.h.
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_id, FbleValue** statics)
{
  FuncValue* v = NewValueExtra(heap, FuncValue, executable->num_statics);
  v->_base.tag = FUNC_VALUE;
  v->function.profile_block_id = profile_block_id;
  memcpy(&v->function, executable, sizeof(FbleExecutable));
  v->function.statics = v->statics;
  for (size_t i = 0; i < executable->num_statics; ++i) {
    v->statics[i] = statics[i];
  }
  return &v->_base;
}

// See documentation in fble-value.h
FbleFunction* FbleFuncValueFunction(FbleValue* value)
{
  FuncValue* func = (FuncValue*)StrictValue(value);
  if (func == NULL) {
    return NULL;
  }
  assert(func->_base.tag == FUNC_VALUE);
  return &func->function;
}

// See documentation in fble-value.h.
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, unit);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[argc - i - 1];
    FbleValue* cons = FbleNewStructValue_(heap, 2, arg, tail);
    tail = FbleNewUnionValue(heap, 0, cons);
  }
  return tail;
}

// See documentation in fble-value.h.
FbleValue* FbleNewListValue_(FbleValueHeap* heap, size_t argc, ...)
{
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, argc);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  return FbleNewListValue(heap, argc, args);
}

// See documentation in fble-value.h.
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, unit);
  for (size_t i = 0; i < argc; ++i) {
    size_t letter = args[argc - i - 1];
    FbleValue* arg = FbleNewUnionValue(heap, letter, unit);
    FbleValue* cons = FbleNewStructValue_(heap, 2, arg, tail);
    tail = FbleNewUnionValue(heap, 0, cons);
  }
  return tail;
}

// See documentation in fble-value.h.
FbleValue* FbleNewRefValue(FbleValueHeap* heap)
{
  RefValue* rv = NewValue(heap, RefValue);
  rv->_base.tag = REF_VALUE;
  rv->value = NULL;
  return &rv->_base;
}

// See documentation in fble-value.h.
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue* ref, FbleValue* value)
{
  // Unwrap any accumulated layers of references on the value and make sure we
  // aren't forming a vacuous value.
  RefValue* unwrap = (RefValue*)value;
  while (!IsPacked(value) && value->tag == REF_VALUE && unwrap->value != NULL) {
    value = unwrap->value;
    unwrap = (RefValue*)value;
  }

  if (value == ref) {
    return false;
  }

  RefValue* rv = (RefValue*)ref;
  assert(rv->_base.tag == REF_VALUE);
  rv->value = value;
  return true;
}

// See documentation in fble-value.h
FbleValue* FbleNewNativeValue(FbleValueHeap* heap,
    void* data, void (*on_free)(void* data))
{
  NativeValue* value = NewValue(heap, NativeValue);
  value->_base.tag = NATIVE_VALUE;
  value->data = data;
  value->on_free = on_free;
  value->tail = heap->stack->natives;
  heap->stack->natives = &value->_base;
  return &value->_base;
}

// See documentation in fble-value.h
void* FbleNativeValueData(FbleValue* value)
{
  value = StrictValue(value);
  assert(value->tag == NATIVE_VALUE);
  return ((NativeValue*)value)->data;
}

// TODO: Remove me.
void FbleRetainValue(FbleValueHeap* heap, FbleValue* src) {}
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value) {}
void FbleReleaseValues(FbleValueHeap* heap, size_t argc, FbleValue** args) {}
void FbleReleaseValues_(FbleValueHeap* heap, size_t argc, ...) {}
void FbleValueFullGc(FbleValueHeap* heap) {}

