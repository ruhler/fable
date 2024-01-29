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

#include "heap.h"           // for FbleHeap, etc.
#include "unreachable.h"    // for FbleUnreachable

/** Different kinds of FbleValue. */
typedef enum {
  STRUCT_VALUE,
  UNION_VALUE,
  FUNC_VALUE,
  REF_VALUE,
  NATIVE_VALUE,
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
} NativeValue;

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
#define NewValue(heap, T) ((T*) FbleNewHeapObject(heap, sizeof(T)))

/**
 * Allocates a new value with some extra space.
 *
 * @param heap  The heap to allocate the value on
 * @param T  The type of the value
 * @param size  The number of bytes of extra space to include in the allocated
 *   object.
 *
 * @returns
 *   The newly allocated value with extra space.
 *
 * @sideeffects
 *   Allocates a value that should be released when it is no longer needed.
 */
#define NewValueExtra(heap, T, size) ((T*) FbleNewHeapObject(heap, sizeof(T) + size))

// See documentation in fble-value.h
//
// Note: the packed value for a generic type matches the packed value of a
// zero-argument struct value, so that it can be packed along with union and
// struct values.
FbleValue* FbleGenericTypeValue = (FbleValue*)1;

static void AddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);
static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeap* heap, FbleValue* src, FbleValue* dst);
static void Refs(FbleHeap* heap, FbleValue* value);

static bool IsPacked(FbleValue* value);
static size_t PackedValueLength(intptr_t data);
static FbleValue* StrictValue(FbleValue* value);

// See documentation in fble-value.h.
FbleValueHeap* FbleNewValueHeap()
{
  return FbleNewHeap(
      (void (*)(FbleHeap*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);
}

// See documentation in fble-value.h.
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  FbleFreeHeap(heap);
}

// See documentation in fble-value.h.
void FbleRetainValue(FbleValueHeap* heap, FbleValue* value)
{
  if (!IsPacked(value) && value != NULL) {
    FbleRetainHeapObject(heap, value);
  }
}

// See documentation in fble-value.h.
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
{
  if (!IsPacked(value) && value != NULL) {
    FbleReleaseHeapObject(heap, value);
  }
}

// See documentation in fble-value.h.
void FbleReleaseValues(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  for (size_t i = 0; i < argc; ++i) {
    FbleReleaseValue(heap, args[i]);
  }
}

// See documentation in fble-value.h.
void FbleReleaseValues_(FbleValueHeap* heap, size_t argc, ...)
{
  va_list ap;
  va_start(ap, argc);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* value = va_arg(ap, FbleValue*);
    FbleReleaseValue(heap, value);
  }
  va_end(ap);
}

/**
 * @func[AddRef] Adds reference from one value to another.
 *  Notifies the value heap of a new reference from src to dst.
 *
 *  @arg[FbleValueHeap*][heap] The heap the values are allocated on.
 *  @arg[FbleValue*    ][src ] The source of the reference.
 *  @arg[FbleValue*    ][dst ]
 *   The destination of the reference. Must not be NULL.
 *
 *  @sideeffects
 *   Causes the dst value to be retained for at least as long as the src value.
 */
static void AddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  if (!IsPacked(src) && !IsPacked(dst)) {
    FbleHeapObjectAddRef(heap, src, dst);
  }
}

// See documentation in fble-value.h.
void FbleValueFullGc(FbleValueHeap* heap)
{
  FbleHeapFullGc(heap);
}

/**
 * The 'on_free' function for values.
 *
 * See documentation of on_free in heap.h.
 *
 * @param heap  The heap.
 * @param value  The value being freed.
 * 
 * @sideeffects
 *   Frees any resources outside of the heap that this value holds on to.
 */
static void OnFree(FbleValueHeap* heap, FbleValue* value)
{
  (void)heap;

  switch (value->tag) {
    case STRUCT_VALUE:
    case UNION_VALUE:
    case FUNC_VALUE:
    case REF_VALUE:
      return;

    case NATIVE_VALUE: {
      NativeValue* v = (NativeValue*)value;
      if (v->on_free != NULL) {
        v->on_free(v->data);
      }
      return;
    }
  }

  FbleUnreachable("Should not get here");
}

/**
 * @func[Ref] Helper function for implementing Refs.
 *  Calls FbleHeapObjectAddRef.
 *
 *  @arg[FbleHeap*][heap] The heap.
 *  @arg[FbleValue*][src] The source of the reference.
 *  @arg[FbleValue*][dst] The target of the reference. Must not be NULL.
 *
 *  @sideeffects
 *   If value is non-NULL, FbleHeapObjectAddRef is called for it.
 */
static void Ref(FbleHeap* heap, FbleValue* src, FbleValue* dst)
{
  if (!IsPacked(dst)) {
    FbleHeapObjectAddRef(heap, src, dst);
  }
}

/**
 * The 'refs' function for values.
 *
 * See documentation of refs in heap.h.
 *
 * @param heap  The heap.
 * @param value  The value whose references to traverse
 *   
 * @sideeffects
 * * Calls FbleHeapRef for each object referenced by obj.
 */
static void Refs(FbleHeap* heap, FbleValue* value)
{
  switch (value->tag) {
    case STRUCT_VALUE: {
      StructValue* sv = (StructValue*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        Ref(heap, value, sv->fields[i]);
      }
      break;
    }

    case UNION_VALUE: {
      UnionValue* uv = (UnionValue*)value;
      Ref(heap, value, uv->arg);
      break;
    }

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      for (size_t i = 0; i < v->function.executable.num_statics; ++i) {
        Ref(heap, value, v->statics[i]);
      }
      break;
    }

    case REF_VALUE: {
      RefValue* v = (RefValue*)value;
      if (v->value != NULL) {
        Ref(heap, value, v->value);
      }
      break;
    }

    case NATIVE_VALUE: {
      break;
    }
  }
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

  StructValue* value = NewValueExtra(heap, StructValue, sizeof(FbleValue*) * argc);
  value->_base.tag = STRUCT_VALUE;
  value->fieldc = argc;

  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[i];
    value->fields[i] = arg;
    AddRef(heap, &value->_base, arg);
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
  AddRef(heap, &union_value->_base, arg);
  return &union_value->_base;
}
// See documentation in fble-value.h.
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewUnionValue(heap, tag, unit);
  FbleReleaseValue(heap, unit);
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
  FuncValue* v = NewValueExtra(heap, FuncValue, sizeof(FbleValue*) * executable->num_statics);
  v->_base.tag = FUNC_VALUE;
  v->function.profile_block_id = profile_block_id;
  memcpy(&v->function, executable, sizeof(FbleExecutable));
  v->function.statics = v->statics;
  for (size_t i = 0; i < executable->num_statics; ++i) {
    v->statics[i] = statics[i];
    AddRef(heap, &v->_base, statics[i]);
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
  FbleReleaseValue(heap, unit);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[argc - i - 1];
    FbleValue* cons = FbleNewStructValue_(heap, 2, arg, tail);
    FbleReleaseValue(heap, tail);

    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
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
  FbleReleaseValue(heap, unit);
  for (size_t i = 0; i < argc; ++i) {
    size_t letter = args[argc - i - 1];
    FbleValue* arg = FbleNewUnionValue(heap, letter, unit);
    FbleValue* cons = FbleNewStructValue_(heap, 2, arg, tail);
    FbleReleaseValue(heap, arg);
    FbleReleaseValue(heap, tail);

    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
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
  AddRef(heap, ref, value);
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
  return &value->_base;
}

// See documentation in fble-value.h
void* FbleNativeValueData(FbleValue* value)
{
  value = StrictValue(value);
  assert(value->tag == NATIVE_VALUE);
  return ((NativeValue*)value)->data;
}
