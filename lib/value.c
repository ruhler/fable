/**
 * @file value.c
 * FbleValue routines.
 */

#include "value.h"

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>   // for FbleAlloc, FbleFree, etc.
#include <fble/fble-execute.h>
#include <fble/fble-vector.h>  // for FbleVectorInit, etc.

#include "heap.h"
#include "unreachable.h"

/**
 * Tests whether a value is packed into an FbleValue* pointer.
 *
 * IMPORTANT: Some fble values are packed directly in the FbleValue* pointer
 * to save space. An FbleValue* only points to an FbleValue if the least
 * significant bit of the pointer is 0.
 *
 * @param x  The value to test.
 *
 * @returns
 *   True if the value is a packaged value, false otherwise.
 */
#define PACKED(x) (((intptr_t)x) & 1)

/** Different kinds of FbleValue. */
typedef enum {
  DATA_TYPE_VALUE,
  STRUCT_VALUE,
  UNION_VALUE,
  FUNC_VALUE,
  REF_VALUE,
} ValueTag;

/**
 * Base class for fble values.
 *
 * A tagged union of value types. All values have the same initial layout as
 * FbleValue. The tag can be used to determine what kind of value this is to
 * get access to additional fields of the value by first casting to that
 * specific type of value.
 */
struct FbleValue {
  ValueTag tag;   /**< The kind of value. */
};

/**
 * DATA_TYPE_VALUE: A struct or union type value.
 */
typedef struct {
  FbleValue _base;        /**< FbleValue base class. */

  /**
   * Number of bits required for the tag.
   * 0 if this represents a struct type instead of a union type.
   */
  size_t tag_size;        

  size_t fieldc;          /**< Number of fields. */
  FbleValue* fields[];    /**< The type of each field. */
} DataTypeValue;

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
  FbleExecutable* executable;   /**< The code for the function. */

  /** Offset to use for profile blocks referenced from this function. */
  size_t profile_block_offset;  

  /**
   * Static variables captured by the function.
   * Size is executable->num_statics
   */
  FbleValue* statics[];         
} FuncValue;

/** A non-circular singly linked list of values. */
typedef struct Values {
  FbleValue* value;       /**< An element of the list. */
  struct Values* next;    /**< The next elements in the list. */
} Values;

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

static void OnFree(FbleValueHeap* heap, FbleValue* value);
static void Ref(FbleHeapCallback* callback, FbleValue* value);
static void Refs(FbleHeapCallback* callback, FbleValue* value);

static size_t PackedValueLength(intptr_t data);

// See documentation in fble-value.h.
FbleValueHeap* FbleNewValueHeap()
{
  return FbleNewHeap(
      (void (*)(FbleHeapCallback*, void*))&Refs,
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
  if (!PACKED(value)) {
    FbleRetainHeapObject(heap, value);
  }
}

// See documentation in fble-value.h.
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value)
{
  if (!PACKED(value) && value != NULL) {
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

// See documentation in fble-value.h.
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  if (!PACKED(src) && !PACKED(dst)) {
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
    case DATA_TYPE_VALUE: return;
    case STRUCT_VALUE: return;
    case UNION_VALUE: return;

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      FbleFreeExecutable(v->executable);
      return;
    }

    case REF_VALUE: return;
  }

  FbleUnreachable("Should not get here");
}

/**
 * Helper function for implementing Refs.
 *
 * Calls the callback if the value is not NULL.
 *
 * @param callback  The refs callback.
 * @param value  The value to add.
 *
 * @sideeffects
 *   If value is non-NULL, the callback is called for it.
 */
static void Ref(FbleHeapCallback* callback, FbleValue* value)
{
  if (!PACKED(value) && value != NULL) {
    callback->callback(callback, value);
  }
}

/**
 * The 'refs' function for values.
 *
 * See documentation of refs in heap.h.
 *
 * @param callback  Callback to call for each object referenced by obj
 * @param value  The value whose references to traverse
 *   
 * @sideeffects
 * * Calls the callback function for each object referenced by obj. If the
 *   same object is referenced multiple times by obj, the callback is
 *   called once for each time the object is referenced by obj.
 */
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
      for (size_t i = 0; i < v->executable->num_statics; ++i) {
        Ref(callback, v->statics[i]);
      }
      break;
    }

    case REF_VALUE: {
      RefValue* v = (RefValue*)value;
      Ref(callback, v->value);
      break;
    }
  }
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

// See documentation in fble-value.h.
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, FbleValue** args)
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

// See documentation in fble-value.h.
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

// See documentation in fble-value.h.
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

// See documentation in value.h.
FbleValue* FbleNewDataTypeValue(FbleValueHeap* heap, FbleDataTypeTag kind, size_t fieldc, FbleValue** fields)
{
  size_t tag_size = 0;
  switch (kind) {
    case FBLE_STRUCT_DATATYPE:
      tag_size = 0;
      break;

    case FBLE_UNION_DATATYPE:
      tag_size = 1;
      while (((size_t)1 << tag_size) <= fieldc) {
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

// See documentation in fble-value.h.
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_offset, FbleValue** statics)
{
  FuncValue* v = NewValueExtra(heap, FuncValue, sizeof(FbleValue*) * executable->num_statics);
  v->_base.tag = FUNC_VALUE;
  v->profile_block_offset = profile_block_offset;
  v->executable = executable;
  v->executable->refcount++;
  for (size_t i = 0; i < executable->num_statics; ++i) {
    v->statics[i] = statics[i];
    FbleValueAddRef(heap, &v->_base, statics[i]);
  }
  return &v->_base;
}

// See documentation in fble-value.h.
FbleValue* FbleNewFuncValue_(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_offset, ...)
{
  size_t argc = executable->num_statics;
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, profile_block_offset);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  return FbleNewFuncValue(heap, executable, profile_block_offset, args);
}

// See documentation in fble-value.h.
FbleFuncInfo FbleFuncValueInfo(FbleValue* func)
{
  FuncValue* func_value = (FuncValue*)func;
  FbleFuncInfo info = {
    .executable = func_value->executable,
    .profile_block_offset = func_value->profile_block_offset,
    .statics = func_value->statics
  };
  return info;
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

// See documentation in fble-value.h.
FbleValue* FbleStrictValue(FbleValue* value)
{
  while (!PACKED(value) && value != NULL && value->tag == REF_VALUE) {
    RefValue* ref = (RefValue*)value;
    value = ref->value;
  }
  return value;
}
