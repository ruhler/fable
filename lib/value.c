/**
 * @file value.c
 *  FbleValue routines.
 */

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL
#include <string.h>   // for memcpy
#include <sys/time.h>       // for getrlimit
#include <sys/resource.h>   // for getrlimit


#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-function.h>  // for FbleFunction, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#include "unreachable.h"    // for FbleUnreachable

/**
 * Circular, doubly linked list of values.
 */
typedef struct List {
  struct List* next;
  struct List* prev;
} List;

static void Clear(List* list);
static bool IsEmpty(List* list);
static FbleValue* Get(List* list);
static void MoveTo(List* dst, FbleValue* value);
static void MoveAllTo(List* dst, List* src);

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
  List list;          /**< A list of values this value belongs to. */
  ValueTag tag;       /**< The kind of value. */
  size_t gen;         /**< Generation this object was allocated in. */
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
 * @param tag The tag of the value
 *
 * @returns
 *   The newly allocated value.
 *
 * @sideeffects
 *   Allocates a value that should be released when it is no longer needed.
 */
#define NewValue(heap, T, tag) ((T*) NewValueRaw(heap, tag, sizeof(T)))

/**
 * Allocates a new value with some extra space.
 *
 * @param heap  The heap to allocate the value on
 * @param T  The type of the value
 * @param tag The tag of the value
 * @param count  The number of FbleValue* worth of extra space to include in
 *   the allocated object.
 *
 * @returns[FbleValue*]
 *   The newly allocated value with extra space.
 *
 * @sideeffects
 *   Allocates a value that should be released when it is no longer needed.
 */
#define NewValueExtra(heap, T, tag, count) ((T*) NewValueRaw(heap, tag, sizeof(T) + count * sizeof(FbleValue*)))

static FbleValue* NewValueRaw(FbleValueHeap* heap, ValueTag tag, size_t size);
static void FreeValue(FbleValue* value);

static bool IsPacked(FbleValue* value);
static bool IsAlloced(FbleValue* value);
static size_t PackedValueLength(intptr_t data);
static FbleValue* StrictValue(FbleValue* value);

#define MAX_GEN ((size_t)-1)

typedef struct Frame {
  // Frames are allocated as needed and stored in a linear doubly linked list.
  // Frames that have been allocated but are unused are stored with all their
  // lists cleared, with min_gen set to MAX_GEN, and with merges set to 0.
  struct Frame* caller;
  struct Frame* callee;

  // The number of frames that have been merged into this frame.
  size_t merges;

  // Objects allocated before entering this stack frame have generation less
  // than min_gen. Objects allocated before the most recent compaction on the
  // frame have generation less than gen.
  // If a.min_gen < b.min_gen, then a is closer to the base of the stack than
  // b.
  size_t min_gen;
  size_t gen;

  // Potential garbage objects on the frame. These are either from objects
  // allocated to callee frames that have since returned or objects allocated
  // on this frame prior to compaction.
  List unmarked;    // Objects not yet seen in traversal.
  List marked;      // Objects seen in traversal but not processed yet.

  // Other objects allocated to this frame.
  List alloced;
} Frame;

struct FbleValueHeap {
  // The generation to allocate new objects to.
  size_t gen;

  // The top frame of the stack. New values are allocated here.
  Frame* top;

  // The frame currently running garbage collection on.
  Frame* gc;

  // The next frame to run garbage collection on.
  // This is the frame closest to the base of the stack with some potential
  // garbage objects to GC.
  // May be NULL to indicate that no frames have potential garbage objects to
  // GC.
  Frame* next_gc;

  // Objects being traversed in the current GC cycle. These belong to heap->gc
  // frame.
  List marked;
  List unmarked;

  // A list of garbage objects to be freed.
  List free;
};

static void MarkRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);
static void MarkRefs(FbleValueHeap* heap, FbleValue* value);
static void IncrGc(FbleValueHeap* heap);

static FbleValue* TailCallSentinel = (FbleValue*)0x2;

typedef struct {
  size_t capacity;
  size_t argc;      /** Number of args, not including func. */
  FbleValue** func_and_args;
} TailCallData;

static TailCallData gTailCallData = {
  .capacity = -1,
  .argc = -1,
  .func_and_args = NULL,
};

static void EnsureTailCallArgsSpace(size_t argc);

static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* PartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args);

static FbleValue* TailCall(FbleValueHeap* heap, FbleProfileThread* profile);
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);

/**
 * @func[Clear] Initialize a list to empty.
 *  @arg[List*][list] The list to initialize
 *  @sideeffects
 *   Makes the list empty, ignoring anything that was previously on the list.
 */
static void Clear(List* list)
{
  list->next = list;
  list->prev = list;
}

/**
 * @func[IsEmpty] Checks if a list is empty.
 *  @arg[List*][list] The list to check.
 *  @returns[bool] true if the list is empty.
 */
static bool IsEmpty(List* list)
{
  return list->next == list;
}

/**
 * @func[Get] Take a value off of a list.
 *  @arg[List*][list] The list to get a value from.
 *  @returns[FbleValue*] A value from the list. NULL if the list is empty.
 *  @sideeffects
 *   Removes the value from the list.
 */
static FbleValue* Get(List* list)
{
  if (list->next == list) {
    return NULL;
  }

  FbleValue* got = (FbleValue*)list->next;
  got->list.prev->next = got->list.next;
  got->list.next->prev = got->list.prev;
  Clear(&got->list);
  return got;
}

/**
 * @func[MoveTo] Moves an value from one list to another.
 *  @arg[List*][dst] The list to move the value to.
 *  @arg[FbleValue*][value] The value to move.
 *  @sideffects
 *   Moves the value from its current list to @a[dst].
 */
static void MoveTo(List* dst, FbleValue* value)
{
  value->list.prev->next = value->list.next;
  value->list.next->prev = value->list.prev;
  value->list.next = dst->next;
  value->list.prev = dst;
  dst->next->prev = &value->list;
  dst->next = &value->list;
}

/**
 * @func[MoveAllTo] Moves all values from one list to another.
 *  @arg[List*][dst] The list to move the values to.
 *  @arg[List*][src] The list to move the values from.
 *  @sideeffects
 *   Moves all values from @a[src] to @a[dst], leaving @a[src] empty.
 */
static void MoveAllTo(List* dst, List* src)
{
  if (!IsEmpty(src)) {
    dst->next->prev = src->prev;
    src->prev->next = dst->next;
    dst->next = src->next;
    dst->next->prev = dst;
    src->next = src;
    src->prev = src;
  }
}

/**
 * @func[NewValueRaw] Allocates a new value on the heap.
 *  @arg[FbleValueHeap*][heap] The heap to allocate to.
 *  @arg[ValueTag][tag] The tag of the new value.
 *  @arg[size_t][size] The number of bytes to allocate for the value.
 *  @returns[FbleValue*] The newly allocated value.
 *  @sideeffects
 *   Allocates a value to the top frame of the heap.
 */
static FbleValue* NewValueRaw(FbleValueHeap* heap, ValueTag tag, size_t size)
{
  IncrGc(heap);

  FbleValue* value = (FbleValue*)FbleAllocRaw(size);
  value->tag = tag;
  value->gen = heap->gen;

  Clear(&value->list);
  MoveTo(&heap->top->alloced, value);
  return value;
}

/**
 * @func[FreeValue] Frees a value.
 *  @arg[FbleValue*][value] The value to free. May be NULL.
 *  @sideeffects
 *   Frees any resources outside of the heap that this value holds on to.
 */
static void FreeValue(FbleValue* value)
{
  if (value != NULL) {
    if (value->tag == NATIVE_VALUE) {
      NativeValue* v = (NativeValue*)value;
      if (v->on_free != NULL) {
        v->on_free(v->data);
      }
    }
    FbleFree(value);
  }
}

/**
 * @func[IsPacked] Tests whether a value is packed into an FbleValue* pointer.
 *  @arg[FbleValue*][value] The value to test.
 *  @returns[bool] True if the value is a packed value, false otherwise.
 */
static bool IsPacked(FbleValue* value)
{
  return (((intptr_t)value) & 1);
}

/**
 * @func[IsAlloced] Tests whether a value is unpacked and not NULL.
 *  @arg[FbleValue*][value] The value to test.
 *  @returns[bool] True if the value is a non-NULL unpacked value.
 */
static bool IsAlloced(FbleValue* value)
{
  return !IsPacked(value) && value != NULL;
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
  while (IsAlloced(value) && value->tag == REF_VALUE) {
    RefValue* ref = (RefValue*)value;
    value = ref->value;
  }
  return value;
}

/**
 * @func[MarkRef] Marks a value referenced from another value.
 *  @arg[FbleHeap*][heap] The heap.
 *  @arg[FbleValue*][src] The source of the reference.
 *  @arg[FbleValue*][dst] The target of the reference. Must not be NULL.
 *
 *  @sideeffects
 *   If value is non-NULL, moves the object to the heap->marked list.
 */
static void MarkRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst)
{
  if (IsAlloced(dst)
      && dst->gen >= heap->gc->min_gen
      && dst->gen != heap->gc->gen) {
    MoveTo(&heap->marked, dst);
  }
}

/**
 * @func[MarkRefs] Marks references from a value.
 *  @arg[FbleValueHeap*][heap] The heap.
 *  @arg[FbleValue*][value] The value whose references to traverse
 *  @sideeffects
 *   Calls MarkRef for each object referenced by obj.
 */
static void MarkRefs(FbleValueHeap* heap, FbleValue* value)
{
  switch (value->tag) {
    case STRUCT_VALUE: {
      StructValue* sv = (StructValue*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        MarkRef(heap, value, sv->fields[i]);
      }
      break;
    }

    case UNION_VALUE: {
      UnionValue* uv = (UnionValue*)value;
      MarkRef(heap, value, uv->arg);
      break;
    }

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      for (size_t i = 0; i < v->function.executable.num_statics; ++i) {
        MarkRef(heap, value, v->statics[i]);
      }
      break;
    }

    case REF_VALUE: {
      RefValue* v = (RefValue*)value;
      if (v->value != NULL) {
        MarkRef(heap, value, v->value);
      }
      break;
    }

    case NATIVE_VALUE: {
      break;
    }
  }
}

/**
 * @func[IncrGc] Performs an incremental GC.
 *  @arg[FbleValueHeap*][heap] The heap to GC on.
 *  @sideeffects
 *   Performs a constant amount of GC work on the heap.
 */
static void IncrGc(FbleValueHeap* heap)
{
  // Free a couple objects on the free list.
  FreeValue(Get(&heap->free));
  FreeValue(Get(&heap->free));

  // Traverse an object on the heap.
  FbleValue* marked = Get(&heap->marked);
  if (marked != NULL) {
    marked->gen = heap->gc->gen;
    MarkRefs(heap, marked);
    MoveTo(&heap->gc->alloced, marked);
    return;
  }

  // Anything left unmarked is unreachable.
  MoveAllTo(&heap->free, &heap->unmarked);

  // Set up next gc
  if (heap->next_gc != NULL && heap->next_gc->min_gen <= heap->top->min_gen) {
    heap->gc = heap->next_gc;
    heap->next_gc = heap->gc->callee;

    MoveAllTo(&heap->marked, &heap->gc->marked);
    MoveAllTo(&heap->unmarked, &heap->gc->unmarked);
  }
}

// See documentation in fble-value.h.
FbleValueHeap* FbleNewValueHeap()
{
  FbleValueHeap* heap = FbleAlloc(FbleValueHeap);
  heap->gen = 0;

  heap->top = FbleAlloc(Frame);
  heap->top->caller = NULL;
  heap->top->callee = NULL;
  heap->top->merges = 0;
  heap->top->min_gen = heap->gen;
  heap->top->gen = heap->gen;
  Clear(&heap->top->unmarked);
  Clear(&heap->top->marked);
  Clear(&heap->top->alloced);

  heap->gc = heap->top;
  heap->next_gc = NULL;

  Clear(&heap->marked);
  Clear(&heap->unmarked);
  Clear(&heap->free);

  return heap;
}

// See documentation in fble-value.h.
void FbleFreeValueHeap(FbleValueHeap* heap)
{
  List values;
  Clear(&values);
  for (Frame* frame = heap->top; frame != NULL; frame = frame->caller) {
    MoveAllTo(&values, &frame->unmarked);
    MoveAllTo(&values, &frame->marked);
    MoveAllTo(&values, &frame->alloced);
  }
  MoveAllTo(&values, &heap->free);
  MoveAllTo(&values, &heap->marked);
  MoveAllTo(&values, &heap->unmarked);

  while (heap->top->callee != NULL) {
    Frame* callee = heap->top->callee;
    heap->top->callee = callee->callee;
    FbleFree(callee);
  }

  while (heap->top != NULL) {
    Frame* top = heap->top;
    heap->top = top->caller;
    FbleFree(top);
  }

  for (FbleValue* value = Get(&values); value != NULL; value = Get(&values)) {
    FreeValue(value);
  }
  FbleFree(heap);
}

// See documentation in fble-value.h
void FblePushFrame(FbleValueHeap* heap)
{
  if (heap->top->callee == NULL) {
    Frame* callee = FbleAlloc(Frame);
    callee->caller = heap->top;
    callee->callee = NULL;
    Clear(&callee->unmarked);
    Clear(&callee->marked);
    Clear(&callee->alloced);
    heap->top->callee = callee;
    callee->merges = 0;
  }

  heap->top = heap->top->callee;

  heap->gen++;
  heap->top->min_gen = heap->gen;
  heap->top->gen = heap->gen;
}

// See documentation in fble-value.h
FbleValue* FblePopFrame(FbleValueHeap* heap, FbleValue* value)
{
  Frame* top = heap->top;
  if (top->merges > 0) {
    top->merges--;
    return value;
  }

  heap->top = heap->top->caller;

  MoveAllTo(&heap->top->unmarked, &top->unmarked);
  MoveAllTo(&heap->top->unmarked, &top->marked);
  MoveAllTo(&heap->top->unmarked, &top->alloced);

  if (heap->gc == top) {
    // If GC is in progress on the frame we are popping, abandon that GC
    // because it's out of date now.
    MoveAllTo(&heap->top->unmarked, &heap->unmarked);
    MoveAllTo(&heap->top->unmarked, &heap->marked);
  }

  if (IsAlloced(value) && value->gen >= top->min_gen) {
    MoveTo(&heap->top->marked, value);
  }
  top->min_gen = MAX_GEN;

  if (heap->next_gc == NULL || heap->top->min_gen < heap->next_gc->min_gen) {
    heap->next_gc = heap->top;
  }

  return value;
}

// See documentation in fble-value.h
void FbleCompactFrame(FbleValueHeap* heap, size_t n, FbleValue** save)
{
  heap->gen++;
  heap->top->gen = heap->gen;

  MoveAllTo(&heap->top->unmarked, &heap->top->marked);
  MoveAllTo(&heap->top->unmarked, &heap->top->alloced);

  if (heap->gc == heap->top) {
    // If GC is in progress on the frame we are compacting we could run into
    // some trouble. Abandon the ongoing GC to avoid any potential problems.
    // TODO: Does this mean we may never get a chance to finish GC because we
    // keep restarting it before it can finish?
    MoveAllTo(&heap->top->unmarked, &heap->unmarked);
    MoveAllTo(&heap->top->unmarked, &heap->marked);
  }

  for (size_t i = 0; i < n; ++i) {
    if (IsAlloced(save[i]) && save[i]->gen >= heap->top->min_gen) {
      MoveTo(&heap->top->marked, save[i]);
    }
  }

  if (heap->next_gc == NULL || heap->top->min_gen < heap->next_gc->min_gen) {
    heap->next_gc = heap->top;
  }
}

// See documentation in fble-value.h
//
// Note: the packed value for a generic type matches the packed value of a
// zero-argument struct value, so that it can be packed along with union and
// struct values.
FbleValue* FbleGenericTypeValue = (FbleValue*)1;

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

  StructValue* value = NewValueExtra(heap, StructValue, STRUCT_VALUE, argc);
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

  UnionValue* union_value = NewValue(heap, UnionValue, UNION_VALUE);
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

/**
 * @func[EnsureTailCallArgsSpace] Makes sure enough space is allocated.
 *  Resizes gTailCallData.args as needed to have space for @a[argc] values.
 *
 *  @arg[size_t][argc] The number of arguments needed.
 *  @sideeffects
 *   Resizes gTailCallData.args as needed. The args array may be reallocated;
 *   the previous value should not be referenced after this call.
 */
static void EnsureTailCallArgsSpace(size_t argc)
{
  size_t capacity = argc + 1;
  if (capacity > gTailCallData.capacity) {
    gTailCallData.capacity = capacity;
    FbleValue** func_and_args = FbleAllocArray(FbleValue*, capacity);
    memcpy(func_and_args, gTailCallData.func_and_args, (1 + gTailCallData.argc) * sizeof(FbleValue*));
    FbleFree(gTailCallData.func_and_args);
    gTailCallData.func_and_args = func_and_args;
  }
}

// FbleRunFunction for PartialApply executable.
// See documentation of FbleRunFunction in fble-value.h
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  size_t s = function->executable.num_statics - 1;
  size_t a = function->executable.num_args;
  size_t argc = s + a;
  FbleValue* nargs[argc];
  memcpy(nargs, function->statics + 1, s * sizeof(FbleValue*));
  memcpy(nargs + s, args, a * sizeof(FbleValue*));
  return FbleCall(heap, profile, function->statics[0], argc, nargs);
}

/**
 * @func[PartialApply] Partially applies a function.
 *  Creates a thunk with the function and arguments without applying the
 *  function yet.
 *
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleFunction*][function] The function to apply.
 *  @arg[FbleValue*][func] The function value to apply.
 *  @arg[size_t][argc] Number of args to pass.
 *  @arg[FbleValue**][args] Args to pass to the function.
 *
 *  @returns[FbleValue*] The allocated result.
 *
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
static FbleValue* PartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args)
{
  FbleExecutable exe = {
    .num_args = function->executable.num_args - argc,
    .num_statics = 1 + argc,
    .run = &PartialApplyImpl
  };

  FbleValue* statics[1 + argc];
  statics[0] = func;
  memcpy(statics + 1, args, argc * sizeof(FbleValue*));
  return FbleNewFuncValue(heap, &exe, function->profile_block_id, statics);
}

/**
 * @func[TailCall] Tail calls an fble function.
 *  Calls the function and args stored in gTailCallData.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Enters a profiling block for the function being called.
 *   @i Executes the called function to completion, returning the result.
 *   @i Pops the current frame from the heap.
 */
static FbleValue* TailCall(FbleValueHeap* heap, FbleProfileThread* profile)
{
  while (true) {
    FbleValue* func = gTailCallData.func_and_args[0];
    FbleFunction* function = FbleFuncValueFunction(func);
    FbleExecutable* exe = &function->executable;
    size_t argc = gTailCallData.argc;

    if (argc < exe->num_args) {
      FbleValue* partial = PartialApply(heap, function, func, argc, gTailCallData.func_and_args + 1);
      return FblePopFrame(heap, partial);
    }

    if (profile) {
      FbleProfileReplaceBlock(profile, function->profile_block_id);
    }

    if (func->tag == REF_VALUE) {
      if (heap->top->merges == 0) {
        FbleCompactFrame(heap, 1 + argc, gTailCallData.func_and_args);
      } else {
        heap->top->merges--;
        FblePushFrame(heap);
      }
    }

    func = gTailCallData.func_and_args[0];
    function = FbleFuncValueFunction(func);
    exe = &function->executable;
    argc = gTailCallData.argc;

    FbleValue* args[argc];
    memcpy(args, gTailCallData.func_and_args + 1, argc * sizeof(FbleValue*));

    FbleValue* result = exe->run(heap, profile, function, args);

    size_t num_unused = argc - exe->num_args;
    FbleValue** unused = args + exe->num_args;

    if (result == TailCallSentinel) {
      // Add the unused args to the end of the tail call args and make that
      // our new func and args.
      EnsureTailCallArgsSpace(gTailCallData.argc + num_unused);
      memcpy(gTailCallData.func_and_args + 1 + gTailCallData.argc, unused, num_unused * sizeof(FbleValue*));
      gTailCallData.argc += num_unused;
    } else if (num_unused > 0) {
      return FblePopFrame(heap, FbleCall(heap, profile, result, num_unused, unused));
    } else {
      return FblePopFrame(heap, result);
    }
  }

  FbleUnreachable("should never get here");
  return NULL;
}

/**
 * @func[Eval] Evaluates the given function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][func] The function to evaluate.
 *  @arg[size_t][argc] Number of args to pass.
 *  @arg[FbleValue**][args] Args to pass to the function.
 *  @arg[FbleProfile*][profile]
 *   Profile to update with execution stats. Must not be NULL.
 *  
 *  @returns[FbleValue*] The computed value, or NULL on error.
 *  
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @i Prints a message to stderr in case of error.
 *   @i Updates profile based on the execution.
 *   @i Does not take ownership of the function or the args.
 */
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile)
{
  // The fble spec requires we don't put an arbitrarily low limit on the stack
  // size. Fix that here.
  struct rlimit original_stack_limit;
  if (getrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "getrlimit failed");
  }

  struct rlimit new_stack_limit = original_stack_limit;
  new_stack_limit.rlim_cur = new_stack_limit.rlim_max;
  if (setrlimit(RLIMIT_STACK, &new_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }

  // Set up gTailCallData.args.
  gTailCallData.capacity = 1;
  gTailCallData.argc = 0;
  gTailCallData.func_and_args = FbleAllocArray(FbleValue*, 1);

  FbleProfileThread* profile_thread = FbleNewProfileThread(profile);
  FbleValue* result = FbleCall(heap, profile_thread, func, argc, args);
  FbleFreeProfileThread(profile_thread);

  FbleFree(gTailCallData.func_and_args);
  gTailCallData.func_and_args = NULL;
  gTailCallData.capacity = -1;
  gTailCallData.argc = -1;

  // Restore the stack limit to what it was before.
  if (setrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }
  return result;
}

// See documentation in fble-value.h
FbleValue* FbleCall(FbleValueHeap* heap, FbleProfileThread* profile, FbleValue* function, size_t argc, FbleValue** args)
{
  FbleFunction* func = FbleFuncValueFunction(function);
  if (func == NULL) {
    FbleLoc loc = FbleNewLoc(__FILE__, __LINE__ + 1, 5);
    FbleReportError("called undefined function\n", loc);
    FbleFreeLoc(loc);
    return NULL;
  }

  FbleExecutable* executable = &func->executable;
  if (argc < executable->num_args) {
    return PartialApply(heap, func, function, argc, args);
  }

  if (profile) {
    FbleProfileEnterBlock(profile, func->profile_block_id);
  }

  size_t num_unused = argc - executable->num_args;
  FbleValue** unused = args + executable->num_args;

  if (function->tag == REF_VALUE) {
    heap->top->merges++;
  } else {
    FblePushFrame(heap);
  }
  FbleValue* result = executable->run(heap, profile, func, args);


  if (result == TailCallSentinel) {
    EnsureTailCallArgsSpace(gTailCallData.argc + num_unused);
    memcpy(gTailCallData.func_and_args + 1 + gTailCallData.argc, unused, num_unused * sizeof(FbleValue*));
    gTailCallData.argc += num_unused;
    result = TailCall(heap, profile);
  } else if (num_unused > 0) {
    FbleValue* new_func = FblePopFrame(heap, result);
    result = FbleCall(heap, profile, new_func, num_unused, unused);
  } else {
    result = FblePopFrame(heap, result);
  }

  if (profile != NULL) {
    FbleProfileExitBlock(profile);
  }

  return result;
}

// See documentation in fble-value.h
FbleValue* FbleTailCall(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args)
{
  EnsureTailCallArgsSpace(argc);

  gTailCallData.argc = argc;
  gTailCallData.func_and_args[0] = func;
  memcpy(gTailCallData.func_and_args + 1, args, argc * sizeof(FbleValue*));
  return TailCallSentinel;
}

// See documentation in fble-value.h.
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, 0, NULL, profile);
}

// See documentation in fble-value.h.
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile)
{
  return Eval(heap, func, argc, args, profile);
}

// See documentation in fble-value.h.
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_id, FbleValue** statics)
{
  FuncValue* v = NewValueExtra(heap, FuncValue, FUNC_VALUE, executable->num_statics);
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
  RefValue* rv = NewValue(heap, RefValue, REF_VALUE);
  rv->value = NULL;
  return &rv->_base;
}

// See documentation in fble-value.h.
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue* ref, FbleValue* value)
{
  assert((heap->top->callee == NULL || heap->top->callee->min_gen == MAX_GEN)
      && "FbleAssignRefValue must be called with ref on top of stack");

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
  NativeValue* value = NewValue(heap, NativeValue, NATIVE_VALUE);
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
