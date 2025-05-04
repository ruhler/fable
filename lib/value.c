/**
 * @file value.c
 *  FbleValue routines.
 *
 *  Including the implementations of:
 *
 *  @i All the various types of Fble values.
 *  @i Memory management: value packing and garbage collection.
 *  @i The execution stack and function calls.
 */

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stddef.h>   // for offsetof
#include <stdlib.h>   // for NULL
#include <string.h>   // for memcpy

#ifndef __WIN32
#include <sys/resource.h>   // for getrlimit, setrlimit
#endif // __WIN32

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-function.h>  // for FbleFunction, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#include "unreachable.h"    // for FbleUnreachable

// Notes on Memory Management
// ==========================
//
// Value Packing
// -------------
// Values are either 'packed' or 'alloced'.
//
// Packed values are stored (packed into) in a single machine word. They are
// passed around by value. We try to use packed values wherever we can.
//
// Allocated values are allocated in memory. They are passed around by
// reference.
//
// Ref Values
// ----------
// Ref values are packed specially as {id, 'b10}, with id 0 reserved for 
// use as a generic undefined value.
//
// Stack Allocation
// ----------------
// An allocated value is owned by a particular frame of the stack.
//
// When a value is first allocated, it is allocated to the (managed) stack. We
// say the value is 'stack allocated'.
//
// If a stack allocated value is returned from the stack frame that owns it to
// the caller frame, we re-allocate the value on the heap before returning it.
// The value is now and forever more 'GC allocated'.
//
// Native values are GC allocated up front, they are never stack allocated.
//
// Stack Frame Merging
// -------------------
// To reduce the number of short lived GC allocated objects, we 'merge'
// together adjacent stack frames. For example, a sequence of calls
// A->B->C->D->E->F->G might get merged into just a couple of stack frames
// (A,B,C,D)->(E,F,G).
//
// We can merge as many stack frames as we want, so long as we don't incur
// more than constant memory overhead from doing so. In practice we merge 
// stack frames as long as we haven't allocated too many bytes on the frame so
// far.
//
// Garbage Collection
// ------------------
// Once an object is GC allocated, it becomes subject to garbage collection. A
// GC allocated object is associated with the stack frame that owns it. When a
// stack frame returns, it transfers ownership of all GC allocated objects
// associated with it to the caller stack frame.
//
// Garbage collection is incremental. Any time we GC allocate a new object, we
// do a small amount of GC work.
//
// Garbage collection operates one stack frame at a time. Each stack frame
// keeps track of a set of marked/unmarked objects with the invariant that
// 'unmarked' objects are reachable if and only if they are reachable from a
// 'marked' object. Garbage collection traverses all the marked objects,
// moving unmarked objects to marked objects, and marked objects to the
// frame's allocated objects until there are no more marked objects. At that
// point anything left in 'unmarked' is unreachable and can be reclaimed.
//
// The idea is, the only time we can create garbage is when we return from (or
// compact) a stack frame. At that point any object allocated on the stack
// frame that isn't reachable from the returned value is garbage. We add the
// returned value to the set of marked objects and everything else allocated
// on the frame to the set of unmarked objects.
//
// We collect garbage from the oldest frame of the stack first, then work our
// way to younger frames of the stack. This gives us a chance to batch
// together objects from younger frames of the stack as those stack frames
// return, while working on GC for the older frames of the stack.
//
// A GC allocated object belongs to a singly linked list of objects and is
// tagged with a generation id ('gen'). The generation id is used to keep
// track of which frame and alloced/marked/unmarked list the object currently
// belongs to.
//
// Frame Compaction
// ----------------
// Tail recursive calls result in frame compaction. This is similar to
// returning from a stack frame, but needs some special handling to keep track
// of objects properly.
//
// Interrupted GC
// --------------
// If a stack frame returns or is compacted while garbage collection is
// happening on that frame, we say GC is interrupted. We let GC finish its
// work and give responsibility for transferring returned objects to the
// caller stack frame to GC when it finishes.

const static uintptr_t ONE = 1;
const static uintptr_t PACKED_OFFSET_WIDTH = (sizeof(FbleValue*) == 8) ? 6 : 5;
const static uintptr_t PACKED_OFFSET_MASK = (ONE << PACKED_OFFSET_WIDTH) - 1;

/**
 * @struct[List] Circular, doubly linked list of values.
 *  @field[List*][next] The next element in the list.
 *  @field[List*][prev] The previous element in the list.
 */
typedef struct List {
  struct List* next;
  struct List* prev;
} List;

// Forward reference to GcAllocatedValue.
typedef struct GcAllocatedValue GcAllocatedValue;

static void Clear(List* list);
static bool IsEmpty(List* list);
static GcAllocatedValue* Get(List* list);
static void MoveTo(List* dst, GcAllocatedValue* value);
static void MoveAllTo(List* dst, List* src);

typedef struct Frame Frame;

/** Different kinds of FbleValue. */
typedef enum {
  STRUCT_VALUE = 0,
  UNION_VALUE = 1,
  FUNC_VALUE = 2,
  NATIVE_VALUE = 3,
} ValueTag;

// uint32_t FbleValue.data field is the tag of a union and the number of
// fields of a struct.

// uint32_t FbleValue.flags field is {traversing, is_gc_alloc, value_tag}. The
// traversing bit is used to limit recursion in RefAssigns. The is_gc_alloc
// bit is used to indicate the value is gc allocated rather than stack
// allocated. The value_tag bits hold the ValueTag of the value.
const uint32_t FbleValueFlagTagBits = 0x3;
const uint32_t FbleValueFlagIsGcAllocBit = 0x4;
const uint32_t FbleValueFlagTraversingBit = 0x8;

/**
 * @struct[NativeValue] NATIVE_VALUE
 *  GC tracked native allocation.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[void*][data] User data.
 *  @field[void (*)(void*)][on_free] Destructor for user data.
 */
typedef struct {
  FbleValue _base;
  void* data;
  void (*on_free)(void* data);
} NativeValue;

/**
 * @struct[StackAllocatedValue] An FbleValue allocated on the stack.
 *  The stack allocated value header is either a pointer to the frame the
 *  object was allocated to, or, after the object has been gc reallocated, a
 *  pointer to the gc reallocated value. If the least significant bit of
 *  gcframe is 1, the object hasn't been gc allocated yet and gcframe XORED
 *  with 1 is the Frame* pointer. Otherwise the object has been gc allocated
 *  and gc is the FbleValue* gc allocated object.
 *
 *  @field[intptr_t][gcframe]
 *   The Frame* or gc allocated FbleValue* pointer, depending on whether the
 *   least significant bit is set.
 *  @field[Frame*][frame] The frame this value is allocated to, XORED with 1.
 *  @field[FbleValue][value] The contents of the value.
 */
typedef struct {
  uintptr_t gcframe;
  FbleValue value;
} StackAllocatedValue;

/**
 * @struct[GcAllocatedValue] An FbleValue allocated on the heap.
 *  @field[List][list] A list of values this value belongs to.
 *  @field[uint64_t][gen] Generation this object is allocated in.
 *  @field[FbleValue][value] The contents of the value.
 */
struct GcAllocatedValue {
  List list;
  uint64_t gen;
  FbleValue value;
};

/**
 * @func[NewValue] Allocates a new value of the given type.
 *  @arg[ValueHeap*][heap] The heap to allocate the value on
 *  @arg[<type>][T] The type of the value
 *  @arg[ValueTag*][tag] The tag of the value
 *
 *  @returns[T*]
 *   The newly allocated value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
#define NewValue(heap, T, tag) ((T*) NewValueRaw(heap, tag, sizeof(T)))

/**
 * @func[NewValueExtra]
 * @ Allocates a new value with some extra space to the stack.
 *  @arg[ValueHeap*][heap] The heap to allocate the value on
 *  @arg[<type>][T] The type of the value
 *  @arg[ValueTag][tag] The tag of the value
 *  @arg[size_t][count]
 *   The number of FbleValue* worth of extra space to include in the allocated
 *   object.
 *
 *  @returns[T*]
 *   The newly allocated value with extra space.
 *
 *  @sideeffects
 *   Allocates a value on the stack.
 */
#define NewValueExtra(heap, T, tag, count) ((T*) NewValueRaw(heap, tag, sizeof(T) + count * sizeof(FbleValue*)))

/**
 * @func[NewGcValue] Allocates a new value of the given type on the heap.
 *  @arg[ValueHeap*][heap] The heap to allocate the value on.
 *  @arg[Frame*][frame] The frame to allocate the value on.
 *  @arg[type][T] The type of the value.
 *  @arg[ValueTag][tag] The tag of the value.
 *  @returns[T*] The newly allocated value.
 *
 *  @sideeffects
 *   Allocates a GC value on the heap.
 */
#define NewGcValue(heap, frame, T, tag) ((T*) NewGcValueRaw(heap, frame, tag, sizeof(T)))

/**
 * @func[NewGcValueExtra]
 * @ Allocates a new value with some extra space to the heap.
 *  @arg[FbleTypeHeap*][heap] The heap to allocate on.
 *  @arg[Frame*][frame] The frame to allocate the value on
 *  @arg[<type>][T] The type of the value
 *  @arg[ValueTag][tag] The tag of the value
 *  @arg[size_t][count]
 *   The number of FbleValue* worth of extra space to include in
 *   the allocated object.
 *
 *  @returns[T*]
 *   The newly allocated value with extra space.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
#define NewGcValueExtra(heap, frame, T, tag, count) ((T*) NewGcValueRaw(heap, frame, tag, sizeof(T) + count * sizeof(FbleValue*)))

// We allocate memory for the stack in 1MB chunks.
#define CHUNK_SIZE (1024 * 1024)

// How many bytes we can allocate on a frame before we should stop merging
// frames. Chosen fairly arbitrarily.
#define MERGE_LIMIT (4 * 1024)

/**
 * @struct[Chunk] A chunk of allocated stack space.
 *  @field[Chunk*][next] The next chunk in the list.
 */
typedef struct Chunk {
  struct Chunk* next;
} Chunk;

/**
 * @struct[Frame] A stack frame.
 *  @field[Frame*][caller] The caller's stack frame.
 *  @field[size_t][merges]
 *   The number of frames that have been merged into this frame.  As an
 *   optimization, we avoid pushing and popping new frames for each function
 *   call. This keeps track of how many calls we've done without pushing a new
 *   frame.
 *  @field[uint64_t][min_gen]
 *   Objects allocated before entering this stack frame have generation less
 *   than min_gen.
 *  @field[uint64_t][gen]
 *   Objects allocated before the most recent compaction on the frame have
 *   generation less than gen.
 *  @field[uint64_t][max_gen]
 *   Objects in marked,unmarked have generation less than max_gen.
 *  @field[List][unmarked]
 *   Potential garbage GC objects on the frame not yet seen in traversal.
 *   These are either from objects allocated to callee frames that have since
 *   returned or objects allocated on this frame prior to compaction.
 *  @field[List][marked]
 *   Potential garbage GC objects on the frame seen in traversal but not
 *   processed yet. These are either from objects allocated to callee frames
 *   that have since returned or objects allocated on this frame prior to
 *   compaction.
 *  @field[List][alloced] Other GC objects allocated to this frame.
 *  @field[intptr_t][top]
 *   The top of the frame on the stack. This points to the callee frame or new
 *   stack allocations if this is the top frame on the stack.
 *  @field[intptr_t][max] The max bounds of allocated memory for this frame.
 *  @field[Chunk*][chunks]
 *   Additional chunks of memory allocated for the stack for use by this and
 *   callee frames.
 */
struct Frame {
  struct Frame* caller;
  size_t merges;

  uint64_t min_gen;
  uint64_t gen;
  uint64_t max_gen;

  List unmarked;
  List marked;
  List alloced;

  intptr_t top;
  intptr_t max;

  Chunk* chunks;
};

static void MoveToAlloced(Frame* frame, GcAllocatedValue* value);
static void MoveToMarked(Frame* frame, GcAllocatedValue* value);
static void MoveToUnmarked(Frame* frame, GcAllocatedValue* value);

/**
 * @struct[Gc] Information about the current set of objects undergoing GC.
 *  @field[uint64_t][gen]
 *   The generation to move objects to when they survive GC. Guaranteed to be
 *   distinct from the generation of any object currently in GC.
 *  @field[uint64_t][min_gen]
 *   An object is currently undergoing GC if its generation is in the interval
 *   [min_gen, max_gen), but not equal to gen.
 *  @field[uint64_t][max_gen]
 *   An object is currently undergoing GC if its generation is in the interval
 *   [min_gen, max_gen), but not equal to gen.
 *  @field[Frame*][frame] The frame that GC is currently running on.
 *  @field[Frame*][next]
 *   The next frame to run garbage collection on. This is the frame closest to
 *   the base of the stack with some potential garbage objects to GC. NULL to
 *   indicate that no frames have potential garbage objects to GC.
 *  @field[List][marked]
 *   Marked objects being traversed in the current GC cycle. These belong to
 *   gc->frame.
 *  @field[List][unmarked]
 *   Unmarked objects being traversed in the current GC cycle. These belong to
 *   gc->frame.
 *  @field[bool][interrupted]
 *   True if the frame GC was working on was popped or compacted during GC.
 *   If this is the case, we'll move reachable objects to 'unmarked' instead
 *   of 'alloced'. See also 'save' field.
 *  @field[FbleValueV][save]
 *   List of objects to resurrect at the end of GC if it was interrupted.
 *   save.size is the capacity of the array, which is expanded as needed. The
 *   The list of values in save.xs is NULL terminated.
 *  @field[List][free] A list of garbage objects to be freed.
 */
typedef struct {
  uint64_t gen;

  uint64_t min_gen;
  uint64_t max_gen;

  Frame* frame;
  Frame* next;

  List marked;
  List unmarked;

  bool interrupted;
  FbleValueV save;

  List free;
} Gc;

/**
 * @struct[ValueHeap] The full FbleValueHeap.
 *  @field[FbleValueHeap][_base]
 *   The publically exposed parts of the value heap.
 *  @field[size_t][tail_call_capacity]
 *   Number of allocated slots in tail_call_buffer.
 *  @field[void*][stack] The base of the stack.
 *  @field[Frame*][top]
 *   The top frame of the stack. New values are allocated here.
 *  @field[Gc][gc] Info about currently running GC.
 *  @field[Chunk*][chunks]
 *   Chunks of allocated stack memory not currently in use.
 *  @field[uintptr_t][ref_id] The next available ref_id.
 */
typedef struct {
  FbleValueHeap _base;
  size_t tail_call_capacity;

  void* stack;
  Frame* top;
  Gc gc;
  Chunk* chunks;
  uintptr_t ref_id;
} ValueHeap;

static StackAllocatedValue* StackAllocatedValueOf(FbleValue* value);
static GcAllocatedValue* GcAllocatedValueOf(FbleValue* value);

static void* StackAlloc(ValueHeap* heap, size_t size);
static FbleValue* NewValueRaw(ValueHeap* heap, ValueTag tag, size_t size);
static FbleValue* NewGcValueRaw(ValueHeap* heap, Frame* frame, ValueTag tag, size_t size);
static void FreeGcValue(GcAllocatedValue* value);
static FbleValue* GcRealloc(ValueHeap* heap, FbleValue* value);

static FbleValue* RefValue(uintptr_t id);
static bool IsRefValue(FbleValue* value);
static uintptr_t RefValueId(FbleValue* value);
static void RefAssign(ValueHeap* heap, uintptr_t refs, FbleValue** values, FbleValue** r);
static void RefsAssign(ValueHeap* heap, uintptr_t refs, FbleValue** values, FbleValue* x);

static bool IsPacked(FbleValue* value);
static bool IsAlloced(FbleValue* value);


static void MarkRef(Gc* gc, FbleValue* src, FbleValue* dst);
static void MarkRefs(Gc* gc, FbleValue* value);
static void IncrGc(ValueHeap* heap);
static void PushFrame(ValueHeap* heap, bool merge);
static void CompactFrame(ValueHeap* heap, bool merge, size_t n, FbleValue** save);

static void EnsureTailCallArgsSpace(ValueHeap* heap, size_t max_call_args);

static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* PartialApply(ValueHeap* heap, FbleFuncValue* function, size_t argc, FbleValue** args);

static FbleValue* TailCall(ValueHeap* heap, FbleProfileThread* profile);
static FbleValue* Eval(ValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);

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
 *  @returns[GcAllocatedValue*] A value from the list. NULL if the list is empty.
 *  @sideeffects Removes the value from the list.
 */
static GcAllocatedValue* Get(List* list)
{
  if (list->next == list) {
    return NULL;
  }

  GcAllocatedValue* got = (GcAllocatedValue*)list->next;
  got->list.prev->next = got->list.next;
  got->list.next->prev = got->list.prev;
  Clear(&got->list);
  return got;
}

/**
 * @func[MoveTo] Moves a value from one list to another.
 *  @arg[List*][dst] The list to move the value to.
 *  @arg[FbleValue*][value] The value to move.
 *  @sideeffects
 *   Moves the value from its current list to @a[dst].
 */
static void MoveTo(List* dst, GcAllocatedValue* value)
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

static void MoveToAlloced(Frame* frame, GcAllocatedValue* value)
{
  assert(value->gen == frame->gen);
  MoveTo(&frame->alloced, value);
}

static void MoveToMarked(Frame* frame, GcAllocatedValue* value)
{
  assert(value->gen != frame->gen);
  assert(value->gen >= frame->min_gen);
  assert(value->gen < frame->max_gen);
  MoveTo(&frame->marked, value);
}

static void MoveToUnmarked(Frame* frame, GcAllocatedValue* value)
{
  assert(value->gen != frame->gen);
  assert(value->gen >= frame->min_gen);
  assert(value->gen < frame->max_gen);
  MoveTo(&frame->unmarked, value);
}

/**
 * @func[StackAllocatedValueOf] Gets the StackAllocatedValue for this value.
 *  @arg[FbleValue*][value] The value to get the StackAllocatedValue* of.
 *  @returns[StackAllocatedValue*] The StackAllocatedValue* for this value.
 *  @sideeffects
 *   Behavior is undefined if the given value is not stack allocated.
 */
static StackAllocatedValue* StackAllocatedValueOf(FbleValue* value)
{
  intptr_t ptr = (intptr_t)value;
  ptr -= offsetof(StackAllocatedValue, value);
  return (StackAllocatedValue*)ptr;
}

/**
 * @func[GcAllocatedValueOf] Gets the GcAllocatedValue for this value.
 *  @arg[FbleValue*][value] The value to get the GcAllocatedValue* of.
 *  @returns[GcAllocatedValue*] The GcAllocatedValue* for this value.
 *  @sideeffects
 *   Behavior is undefined if the given value is not stack allocated.
 */
static GcAllocatedValue* GcAllocatedValueOf(FbleValue* value)
{
  intptr_t ptr = (intptr_t)value;
  ptr -= offsetof(GcAllocatedValue, value);
  return (GcAllocatedValue*)ptr;
}

/**
 * @func[StackAlloc] Allocates memory on the stack.
 *  @arg[ValueHeap*][heap] The heap to allocate the memory to.
 *  @arg[size_t][size] The number of bytes to allocate.
 *  @returns[void*] Pointer to the allocated memory.
 *  @sideeffects
 *   Allocates a new stack chunk if needed.
 */
static void* StackAlloc(ValueHeap* heap, size_t size)
{
  Frame* frame = heap->top;
  if (frame->max < frame->top + size) {
    Chunk* chunk = heap->chunks;
    if (chunk == NULL) {
      chunk = (Chunk*)FbleAllocRaw(CHUNK_SIZE);
    } else {
      heap->chunks = chunk->next;
    }

    chunk->next = frame->chunks;
    frame->chunks = chunk;
    frame->top = size + (intptr_t)(chunk + 1);
    frame->max = CHUNK_SIZE + (intptr_t)chunk;
  }

  void* result = (void*)frame->top;
  frame->top += size;
  return result;
}

/**
 * @func[NewValueRaw] Allocates a new value on the stack.
 *  @arg[ValueHeap*][heap] The heap to allocate to.
 *  @arg[ValueTag][tag] The tag of the new value.
 *  @arg[size_t][size] The number of bytes to allocate for the value.
 *  @returns[FbleValue*] The newly allocated value.
 *  @sideeffects Allocates a value to the top frame of the heap.
 */
static FbleValue* NewValueRaw(ValueHeap* heap, ValueTag tag, size_t size)
{
  StackAllocatedValue* value = StackAlloc(heap, size + offsetof(StackAllocatedValue, value));
  value->gcframe = ((uintptr_t)heap->top) ^ ONE;
  value->value.flags = tag;
  return &value->value;
}

/**
 * @func[NewGcValueRaw] Allocates a new value on the heap.
 *  @arg[ValueHeap*][heap] The heap to allocate on.
 *  @arg[Frame*][frame] The frame to allocate to.
 *  @arg[ValueTag][tag] The tag of the new value.
 *  @arg[size_t][size] The number of bytes to allocate for the value.
 *  @returns[FbleValue*] The newly allocated value.
 *  @sideeffects
 *   Allocates a value to the given frame of the heap.
 */
static FbleValue* NewGcValueRaw(ValueHeap* heap, Frame* frame, ValueTag tag, size_t size)
{
  IncrGc(heap);

  GcAllocatedValue* value = (GcAllocatedValue*)FbleAllocRaw(size + offsetof(GcAllocatedValue, value));
  value->value.flags = tag | FbleValueFlagIsGcAllocBit;
  value->gen = frame->gen;

  Clear(&value->list);
  MoveToAlloced(frame, value);
  return &value->value;
}

/**
 * @func[FreeGcValue] Frees a GC allocated value.
 *  @arg[GcAllocatedValue*][value] The value to free. May be NULL.
 *  @sideeffects
 *   Frees any resources outside of the heap that this value holds on to.
 */
static void FreeGcValue(GcAllocatedValue* value)
{
  if (value != NULL) {
    if ((value->value.flags & FbleValueFlagTagBits) == NATIVE_VALUE) {
      NativeValue* v = (NativeValue*)&value->value;
      if (v->on_free != NULL) {
        v->on_free(v->data);
      }
    }
    FbleFree(value);
  }
}

/**
 * @func[GcRealloc] Reallocate a value onto the heap.
 *  @arg[ValueHeap*][heap] The heap to allocate to.
 *  @arg[FbleValue*][value] The value to allocate.
 *  @returns[FbleValue*] A gc allocated value equivalent to @a[value].
 *  @sideeffects
 *   @i Allocates GC values on the heap.
 *   @i Records mapping from original value to GC value on the stack.
 */
static FbleValue* GcRealloc(ValueHeap* heap, FbleValue* value)
{
  // Packed values and NULL need not be allocated at all.
  if (!IsAlloced(value)) {
    return value;
  }

  // If the value is already a GC value, there's nothing to do.
  if (value->flags & FbleValueFlagIsGcAllocBit) {
    return value;
  }

  // If the value has already been GC allocated, return the associated GC
  // allocated value.
  StackAllocatedValue* svalue = StackAllocatedValueOf(value);
  if ((svalue->gcframe & ONE) == 0) {
    return (FbleValue*)svalue->gcframe;
  }

  Frame* frame = (Frame*)(svalue->gcframe ^ ONE);
  switch ((ValueTag)(value->flags & FbleValueFlagTagBits)) {
    case STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      FbleStructValue* nv = NewGcValueExtra(heap, frame, FbleStructValue, STRUCT_VALUE, value->data);
      nv->_base.data = sv->_base.data;
      svalue->gcframe = (uintptr_t)&nv->_base;
      
      nv->_base.data = sv->_base.data;
      for (size_t i = 0; i < sv->_base.data; ++i) {
        nv->fields[i] = GcRealloc(heap, sv->fields[i]);
      }
      return &nv->_base;
    }

    case UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)value;
      FbleUnionValue* nv = NewGcValue(heap, frame, FbleUnionValue, UNION_VALUE);
      svalue->gcframe = (uintptr_t)&nv->_base;
      nv->_base.data = uv->_base.data;
      nv->arg = GcRealloc(heap, uv->arg);
      return &nv->_base;
    }

    case FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)value;
      FbleFuncValue* nv = NewGcValueExtra(heap, frame, FbleFuncValue, FUNC_VALUE, fv->function.executable.num_statics);
      svalue->gcframe = (uintptr_t)&nv->_base;
      memcpy(&nv->function.executable, &fv->function.executable, sizeof(FbleExecutable));
      nv->function.profile_block_id = fv->function.profile_block_id;
      nv->function.statics = nv->statics;
      for (size_t i = 0; i < nv->function.executable.num_statics; ++i) {
        nv->statics[i] = GcRealloc(heap, fv->statics[i]);
      }
      return &nv->_base;
    }

    case NATIVE_VALUE: {
      FbleUnreachable("native value should already be GC allocated.");
      return NULL;
    }
  }

  FbleUnreachable("should never get here");
}

/**
 * @func[RefValue] Construct a reference value.
 *  @arg[uintptr_t][id] The id of the value.
 *  @returns[FbleValue*] The constructed reference value.
 */
static FbleValue* RefValue(uintptr_t id)
{
  uintptr_t data = (id << 2) | 0x2;
  return (FbleValue*)data;
}

/**
 * @func[IsRefValue] Check if a value is a reference value.
 *  @arg[FbleValue*][value] The value to check.
 *  @returns[bool] True if it is a reference value, false otherwise.
 */
static bool IsRefValue(FbleValue* value)
{
  uintptr_t data = (uintptr_t)value;
  return (data & 0x3) == 0x2;
}

/**
 * @func[RefValueId] Gets the id of a reference value.
 *  @arg[FbleValue*][value] The reference value to get the id of.
 *  @returns[uintptr_t] The id of the reference value.
 *  @sideeffects
 *   Behavior is undefined if the value is not a reference value.
 */
static uintptr_t RefValueId(FbleValue* value)
{
  uintptr_t data = (uintptr_t)value;
  return data >> 2;
}

/**
 * @func[RefAssign] Update a reference value assignment.
 *  @arg[ValueHeap*][heap] The value heap.
 *  @arg[uintptr_t][refs] The id of the first of the refs.
 *  @arg[FbleValue**][values] The values to be assigned.
 *  @arg[FbleValue**][x] Pointer to value to check assignment on.
 *  @sideeffects
 *   @i Updates refs assignments to the given reference as appropriate.
 *   @item
 *    Behavior is undefined if the number of values does not match the number
 *    of currently allocated ref ids >= refs.
 */
static void RefAssign(ValueHeap* heap, uintptr_t refs, FbleValue** values, FbleValue** r)
{
  FbleValue* x = *r;

  // See if this is one of the ref values for us to substitute in.
  if (IsRefValue(x)) {
    uintptr_t id = RefValueId(x);
    if (id >= refs) {
      *r = values[id - refs];
      return;
    }
  }

  // Do substitution inside this value.
  RefsAssign(heap, refs, values, x);
}

/**
 * @func[RefsAssign] Perform ref value assignments on x.
 *  @arg[ValueHeap*][heap] The value heap.
 *  @arg[uintptr_t][refs] The id of the first of the refs.
 *  @arg[FbleValue**][values] The values to be assigned.
 *  @arg[FbleValue*][x] The value to do the assignments in.
 *  @sideeffects
 *   Replaces all references to ref values in x with their corresponding
 *   values.
 */
static void RefsAssign(ValueHeap* heap, uintptr_t refs, FbleValue** values, FbleValue* x)
{
  // Nothing to do for packed values or NULL.
  if (!IsAlloced(x)) {
    return;
  }

  // Nothing to do for values currently being traversed.
  if (x->flags & FbleValueFlagTraversingBit) {
    return;
  }

  assert((x->flags & FbleValueFlagIsGcAllocBit) && "I think this is expected?");

  GcAllocatedValue* gx = GcAllocatedValueOf(x);

  // Avoid traversing objects from older generations.
  if ((x->flags & FbleValueFlagIsGcAllocBit) && gx->gen < heap->top->gen) {
    return;
  }

  x->flags ^= FbleValueFlagTraversingBit;
  switch ((ValueTag)(x->flags & FbleValueFlagTagBits)) {
    case STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)x;
      for (size_t i = 0; i < x->data; ++i) {
        RefAssign(heap, refs, values, sv->fields + i);
      }
      break;
    }

    case UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)x;
      RefAssign(heap, refs, values, &uv->arg);
      break;
    }

    case FUNC_VALUE: {
      FbleFuncValue* fv = (FbleFuncValue*)x;
      for (size_t i = 0; i < fv->function.executable.num_statics; ++i) {
        RefAssign(heap, refs, values, fv->statics + i);
      }
      break;
    }

    case NATIVE_VALUE: {
      // Nothing to do.
      break;
    }
  }
  x->flags ^= FbleValueFlagTraversingBit;
}

/**
 * @func[IsPacked] Tests whether a value is packed into an FbleValue* pointer.
 *  Not including RefValues.
 *
 *  @arg[FbleValue*][value] The value to test.
 *  @returns[bool] True if the value is a packed value, false otherwise.
 */
static bool IsPacked(FbleValue* value)
{
  return (((uintptr_t)value) & 0x1) == 0x1;
}

/**
 * @func[IsAlloced] Tests whether a value is unpacked and not NULL.
 *  @arg[FbleValue*][value] The value to test.
 *  @returns[bool] True if the value is a non-NULL unpacked value.
 */
static bool IsAlloced(FbleValue* value)
{
  return !IsPacked(value) && !IsRefValue(value) && value != NULL;
}

/**
 * @func[MarkRef] Marks a GC value referenced from another value.
 *  @arg[Gc*][gc] The current Gc.
 *  @arg[FbleValue*][src] The source of the reference.
 *  @arg[FbleValue*][dst] The target of the reference. Must not be NULL.
 *
 *  @sideeffects
 *   If value is non-NULL, moves the object to the heap->marked list.
 */
static void MarkRef(Gc* gc, FbleValue* src, FbleValue* dst)
{
  GcAllocatedValue* gdst = GcAllocatedValueOf(dst);
  if (IsAlloced(dst)
      && gdst->gen >= gc->min_gen
      && gdst->gen != gc->gen) {
    assert(gdst->gen < gc->max_gen);
    MoveTo(&gc->marked, gdst);
  }
}

/**
 * @func[MarkRefs] Marks references from a GC allocated value.
 *  @arg[Gc*][gc] The current gc.
 *  @arg[FbleValue*][value] The value whose references to traverse
 *  @sideeffects
 *   Calls MarkRef for each object referenced by obj.
 */
static void MarkRefs(Gc* gc, FbleValue* value)
{
  switch ((ValueTag)(value->flags & FbleValueFlagTagBits)) {
    case STRUCT_VALUE: {
      FbleStructValue* sv = (FbleStructValue*)value;
      for (size_t i = 0; i < value->data; ++i) {
        MarkRef(gc, value, sv->fields[i]);
      }
      break;
    }

    case UNION_VALUE: {
      FbleUnionValue* uv = (FbleUnionValue*)value;
      MarkRef(gc, value, uv->arg);
      break;
    }

    case FUNC_VALUE: {
      FbleFuncValue* v = (FbleFuncValue*)value;
      for (size_t i = 0; i < v->function.executable.num_statics; ++i) {
        MarkRef(gc, value, v->statics[i]);
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
 *  @arg[ValueHeap*][heap] The heap to GC on.
 *  @sideeffects
 *   Performs a constant amount of GC work on the heap.
 */
static void IncrGc(ValueHeap* heap)
{
  Gc* gc = &heap->gc;

  // Free a couple objects on the free list.
  FreeGcValue(Get(&gc->free));
  FreeGcValue(Get(&gc->free));

  // Traverse an object on the heap.
  GcAllocatedValue* marked = Get(&gc->marked);
  if (marked != NULL) {
    marked->gen = gc->gen;
    MarkRefs(gc, &marked->value);

    if (gc->interrupted) {
      // GC was interrupted during pop frame or compact, so this object should
      // be moved to 'unmarked'.
      MoveToUnmarked(gc->frame, marked);
    } else {
      MoveToAlloced(gc->frame, marked);
    }
    return;
  }

  // Anything left unmarked is unreachable.
  MoveAllTo(&gc->free, &gc->unmarked);

  // Resurrect anything that needs saving due to interrupted GC.
  for (size_t i = 0; gc->save.xs[i] != NULL; ++i) {
    MoveToMarked(gc->frame, GcAllocatedValueOf(gc->save.xs[i]));
  }

  // Set up next gc
  if (gc->next != NULL) {
    gc->frame = gc->next;
    if (gc->frame == heap->top) {
      gc->next = NULL;
    } else {
      gc->next = ((Frame*)gc->frame->top) - 1;
    }

    gc->min_gen = gc->frame->min_gen;
    gc->gen = gc->frame->gen;
    gc->max_gen = gc->frame->max_gen;
    MoveAllTo(&gc->marked, &gc->frame->marked);
    MoveAllTo(&gc->unmarked, &gc->frame->unmarked);
    gc->interrupted = false;
    gc->save.xs[0] = NULL;
  }
}

// See documentation in fble-value.h.
FbleValueHeap* FbleNewValueHeap()
{
  // If this fails, add support for whatever crazy architecture you are trying
  // to use. 32 and 64 bit architectures should be supported.
  assert((ONE << PACKED_OFFSET_WIDTH) == 8 * sizeof(FbleValue*));

  ValueHeap* heap = FbleAlloc(ValueHeap);

  heap->tail_call_capacity = 1;
  heap->_base.tail_call_sentinel = (FbleValue*)0x2;
  heap->_base.tail_call_buffer = FbleAllocArray(FbleValue*, heap->tail_call_capacity);
  heap->_base.tail_call_argc = 0;

  heap->stack = FbleAllocRaw(CHUNK_SIZE);

  heap->top = (Frame*)heap->stack;

  heap->top->caller = NULL;
  heap->top->merges = 0;
  heap->top->min_gen = 0;
  heap->top->gen = 0;
  heap->top->max_gen = 1;
  Clear(&heap->top->unmarked);
  Clear(&heap->top->marked);
  Clear(&heap->top->alloced);
  heap->top->top = (intptr_t)(heap->top + 1);
  heap->top->max = CHUNK_SIZE + (intptr_t)heap->stack;
  heap->chunks = NULL;

  heap->gc.min_gen = heap->top->min_gen;
  heap->gc.gen = heap->top->gen;
  heap->gc.max_gen = heap->top->max_gen;
  heap->gc.frame = heap->top;
  heap->gc.next = NULL;
  Clear(&heap->gc.marked);
  Clear(&heap->gc.unmarked);
  Clear(&heap->gc.free);
  heap->gc.interrupted = false;
  heap->gc.save.size = 2;
  heap->gc.save.xs = FbleAllocArray(FbleValue*, heap->gc.save.size);
  heap->gc.save.xs[0] = NULL;

  heap->chunks = NULL;
  heap->ref_id = 1;

  return &heap->_base;
}

// See documentation in fble-value.h.
void FbleFreeValueHeap(FbleValueHeap* heap_)
{
  ValueHeap* heap = (ValueHeap*)heap_;

  List values;
  Clear(&values);
  for (Frame* frame = heap->top; frame != NULL; frame = frame->caller) {
    MoveAllTo(&values, &frame->unmarked);
    MoveAllTo(&values, &frame->marked);
    MoveAllTo(&values, &frame->alloced);

    for (Chunk* chunk = frame->chunks; chunk != NULL; chunk = frame->chunks) {
      frame->chunks = chunk->next;
      FbleFree(chunk);
    }
  }
  MoveAllTo(&values, &heap->gc.free);
  MoveAllTo(&values, &heap->gc.marked);
  MoveAllTo(&values, &heap->gc.unmarked);
  FbleFree(heap->gc.save.xs);

  FbleFree(heap->stack);
  for (Chunk* chunk = heap->chunks; chunk != NULL; chunk = heap->chunks) {
    heap->chunks = chunk->next;
    FbleFree(chunk);
  }

  for (GcAllocatedValue* value = Get(&values); value != NULL; value = Get(&values)) {
    FreeGcValue(value);
  }

  FbleFree(heap->_base.tail_call_buffer);

  FbleFree(heap);
}

// See documentation of FblePushFrame in fble-value.h
// If merge is true, reuse the same allocation space as the caller frame.
static void PushFrame(ValueHeap* heap, bool merge)
{
  if (merge) {
    heap->top->merges++;
    return;
  }

  Frame* callee = (Frame*)StackAlloc(heap, sizeof(Frame));
  callee->caller = heap->top;
  Clear(&callee->unmarked);
  Clear(&callee->marked);
  Clear(&callee->alloced);
  callee->merges = 0;
  callee->min_gen = heap->top->max_gen;
  callee->gen = heap->top->max_gen;
  callee->max_gen = callee->gen + 1;
  callee->top = (intptr_t)(callee + 1);
  callee->max = heap->top->max;
  callee->chunks = NULL;

  // If a program runs for a really long time (like over 100 years), it's
  // possible we could overflow the GC gen value and GC would break. Hopefully
  // that never happens.
  assert(callee->gen > 0 && "GC gen overflow!");
  
  heap->top = callee;
}

// See documentation in fble-value.h
void FblePushFrame(FbleValueHeap* heap)
{
  PushFrame((ValueHeap*)heap, false);
}

// See documentation in fble-value.h
FbleValue* FblePopFrame(FbleValueHeap* heap_, FbleValue* value)
{
  ValueHeap* heap = (ValueHeap*)heap_;

  Frame* top = heap->top;
  if (top->merges > 0) {
    top->merges--;
    return value;
  }

  value = GcRealloc(heap, value);

  heap->top = heap->top->caller;

  heap->top->max_gen = top->max_gen;
  MoveAllTo(&heap->top->unmarked, &top->unmarked);
  MoveAllTo(&heap->top->unmarked, &top->marked);
  MoveAllTo(&heap->top->unmarked, &top->alloced);

  GcAllocatedValue* gvalue = GcAllocatedValueOf(value);
  if (IsAlloced(value) && gvalue->gen >= top->min_gen) {
    MoveToMarked(heap->top, gvalue);
  }

  if (heap->gc.frame == top) {
    // We are popping the frame currently being GC'd.
    heap->gc.interrupted = true;
    heap->gc.frame = heap->top;

    // If the value we are returning is currently undergoing GC, keep it there
    // until GC has a chance to finish.
    if (IsAlloced(value)
        && gvalue->gen >= heap->gc.min_gen
        && gvalue->gen != heap->gc.gen
        && gvalue->gen < heap->gc.max_gen) {
      MoveTo(&heap->gc.marked, gvalue);
      heap->gc.save.xs[0] = value;
      heap->gc.save.xs[1] = NULL;
    } else {
      heap->gc.save.xs[0] = NULL;
    }
  }

  while (top->chunks != NULL) {
    Chunk* chunk = top->chunks;
    top->chunks = chunk->next;
    chunk->next = heap->chunks;
    heap->chunks = chunk;
  }

  if (heap->gc.next == NULL || heap->gc.next == top) {
    heap->gc.next = heap->top;
  }

  return value;
}

/**
 * @func[CompactFrame] Compacts the top heap frame.
 *  Values allocated on the frame are freed except for those reachable from
 *  the given list of saved values.
 *
 *  @arg[ValueHeap*][heap] The heap.
 *  @arg[bool][merge] If true, skip compaction.
 *  @arg[size_t][n] The number of save values.
 *  @arg[FbleValue**][save] Values to save.
 *
 *  @sideeffects
 *   Compacts the heap frame. Updates pointers in the save array to their new
 *   values. You must not reference any values allocated to the frame after
 *   this call except for the updated values in the save array.
 */
static void CompactFrame(ValueHeap* heap, bool merge, size_t n, FbleValue** save)
{
  if (merge) {
    return;
  }

  if (heap->top->merges > 0) {
    // We can't compact in place because some of the frame is shared with the
    // caller. Push a fresh frame so we can compact next time around.
    heap->top->merges--;
    PushFrame(heap, false);
    return;
  }

  for (size_t i = 0; i < n; ++i) {
    save[i] = GcRealloc(heap, save[i]);
  }

  heap->top->gen = heap->top->max_gen;
  heap->top->max_gen = heap->top->gen + 1;

  // If a program runs for a really long time (like over 100 years), it's
  // possible we could overflow the GC gen value and GC would break. Hopefully
  // that never happens.
  assert(heap->top->max_gen > 0 && "GC gen overflow!");

  heap->top->top = (intptr_t)(heap->top + 1);
  heap->top->max = heap->top->caller->max;
  while (heap->top->chunks != NULL) {
    Chunk* chunk = heap->top->chunks;
    heap->top->chunks = chunk->next;
    chunk->next = heap->chunks;
    heap->chunks = chunk;
  }

  MoveAllTo(&heap->top->unmarked, &heap->top->marked);
  MoveAllTo(&heap->top->unmarked, &heap->top->alloced);

  for (size_t i = 0; i < n; ++i) {
    GcAllocatedValue* gsave = GcAllocatedValueOf(save[i]);
    if (IsAlloced(save[i]) && gsave->gen >= heap->top->min_gen) {
      MoveToMarked(heap->top, gsave);
    }
  }

  if (heap->gc.frame == heap->top) {
    // We are compacting the frame currently being GC'd.
    heap->gc.interrupted = true;

    // If any values we are saving are currently undergoing GC, keep them
    // there until GC has a chance to finish.
    if (heap->gc.save.size < n + 1) {
      heap->gc.save.size = n + 1;
      heap->gc.save.xs = FbleReAllocArray(FbleValue*, heap->gc.save.xs, n + 1);
    }

    size_t s = 0;
    for (size_t i = 0; i < n; ++i) {
      GcAllocatedValue* gsave = GcAllocatedValueOf(save[i]);
      if (IsAlloced(save[i])
          && gsave->gen >= heap->gc.min_gen
          && gsave->gen != heap->gc.gen
          && gsave->gen < heap->gc.max_gen) {
        MoveTo(&heap->gc.marked, gsave);
        heap->gc.save.xs[s] = save[i];
        s++;
      }
    }
    heap->gc.save.xs[s] = NULL;
  }

  if (heap->gc.next == NULL) {
    heap->gc.next = heap->top;
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
  uintptr_t header_length = (argc == 0) ? 0 : ((argc - 1) * PACKED_OFFSET_WIDTH);
  uintptr_t length = 0;
  uintptr_t header = 0;  // Struct header listing offsets for the fields.
  uintptr_t data = 0;    // Field data following the struct header.

  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[i];
    if (IsPacked(arg)) {
      uintptr_t adata = (uintptr_t)arg;
      adata >>= 1;
      uintptr_t alength = adata & PACKED_OFFSET_MASK;
      adata >>= PACKED_OFFSET_WIDTH;
      data |= (adata << length);
      length += alength;
      header |= (length << (i * PACKED_OFFSET_WIDTH));
    } else {
      length += 8 * sizeof(FbleValue*);
      break;
    }
  }

  length += header_length;

  if (length + PACKED_OFFSET_WIDTH + 1 <= 8 * sizeof(FbleValue*)) {
    header &= ((ONE << header_length) - 1); // Drop last field offset from header.
    data <<= header_length;
    data |= header;
    data <<= PACKED_OFFSET_WIDTH;
    data |= length;
    data <<= 1;
    data |= 1;
    return (FbleValue*)data;
  }

  FbleStructValue* value = NewValueExtra((ValueHeap*)heap, FbleStructValue, STRUCT_VALUE, argc);
  value->_base.data = argc;

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
FbleValue* FbleStructValueField(FbleValue* object, size_t fieldc, size_t field)
{
  if (object == NULL || IsRefValue(object)) {
    return NULL;
  }

  if (IsPacked(object)) {
    uintptr_t data = (uintptr_t)object;
    data >>= 1;

    uintptr_t length = data & PACKED_OFFSET_MASK;
    data >>= PACKED_OFFSET_WIDTH;

    uintptr_t header_length = (fieldc == 0) ? 0 : (PACKED_OFFSET_WIDTH * (fieldc - 1));
    uintptr_t offset = (field == 0) ? 0 : ((data >> (PACKED_OFFSET_WIDTH * (field - 1))) & PACKED_OFFSET_MASK);
    uintptr_t end = (field + 1 == fieldc) ? (length - header_length) : ((data >> (PACKED_OFFSET_WIDTH * field)) & PACKED_OFFSET_MASK);
    data >>= header_length;

    length = end - offset;
    data >>= offset;
    data &= (ONE << length) - 1;

    data <<= PACKED_OFFSET_WIDTH;
    data |= length;
    data <<= 1;
    data |= 1;
    return (FbleValue*)data;
  }

  assert((object->flags & FbleValueFlagTagBits) == STRUCT_VALUE);
  FbleStructValue* value = (FbleStructValue*)object;
  assert(field < value->_base.data);
  return value->fields[field];
}

// See documentation in fble-value.h.
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tagwidth, size_t tag, FbleValue* arg)
{
  if (IsPacked(arg)) {
    uintptr_t data = (uintptr_t)arg;
    data >>= 1;

    uintptr_t length = data & PACKED_OFFSET_MASK;
    data >>= PACKED_OFFSET_WIDTH;

    length += tagwidth;
    if (length + PACKED_OFFSET_WIDTH + 1 <= 8 * sizeof(FbleValue*)) {
      data <<= tagwidth;
      data |= (uintptr_t)tag;
      data <<= PACKED_OFFSET_WIDTH;
      data |= length;
      data <<= 1;
      data |= 1;
      return (FbleValue*)data;
    }
  }

  FbleUnionValue* union_value = NewValue((ValueHeap*)heap, FbleUnionValue, UNION_VALUE);
  union_value->_base.data = tag;
  union_value->arg = arg;
  return &union_value->_base;
}
// See documentation in fble-value.h.
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tagwidth, size_t tag)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewUnionValue(heap, tagwidth, tag, unit);
  return result;
}

// See documentation in fble-value.h.
size_t FbleUnionValueTag(FbleValue* object, size_t tagwidth)
{
  if (object == NULL || IsRefValue(object)) {
    return (size_t)(-1);
  }

  if (IsPacked(object)) {
    uintptr_t data = (uintptr_t)object;
    data >>= (1 + PACKED_OFFSET_WIDTH);
    data &= (ONE << tagwidth) - 1;
    return data;
  }

  assert((object->flags & FbleValueFlagTagBits) == UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->_base.data;
}

// See documentation in fble-value.h.
FbleValue* FbleUnionValueArg(FbleValue* object, size_t tagwidth)
{
  if (object == NULL || IsRefValue(object)) {
    return NULL;
  }

  if (IsPacked(object)) {
    uintptr_t data = (uintptr_t)object;
    data >>= 1;

    uintptr_t length = data & PACKED_OFFSET_MASK;
    length -= tagwidth;

    data >>= (PACKED_OFFSET_WIDTH + tagwidth);

    data <<= PACKED_OFFSET_WIDTH;
    data |= length;
    data <<= 1;
    data |= 1;
    return (FbleValue*)data;
  }

  assert((object->flags & FbleValueFlagTagBits) == UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return value->arg;
}

// See documentation in fble-value.h.
FbleValue* FbleUnionValueField(FbleValue* object, size_t tagwidth, size_t field)
{
  if (object == NULL || IsRefValue(object)) {
    return NULL;
  }

  if (IsPacked(object)) {
    uintptr_t data = (uintptr_t)object;
    data >>= 1;

    uintptr_t length = data & PACKED_OFFSET_MASK;
    length -= tagwidth;

    data >>= PACKED_OFFSET_WIDTH;

    uintptr_t tag = data & ((ONE << tagwidth) - 1);
    if (tag != field) {
      return FbleWrongUnionTag;
    }

    data >>= tagwidth;
    data <<= PACKED_OFFSET_WIDTH;
    data |= length;
    data <<= 1;
    data |= 1;
    return (FbleValue*)data;
  }

  assert((object->flags & FbleValueFlagTagBits) == UNION_VALUE);
  FbleUnionValue* value = (FbleUnionValue*)object;
  return (value->_base.data == field) ? value->arg : FbleWrongUnionTag;
}

/**
 * @func[EnsureTailCallArgsSpace] Makes sure enough space is allocated.
 *  Resizes heap->tail_call_buffer as needed to have sufficient space,
 *  assuming the maximum number of args to any call or tail call in the
 *  program is not greater than @a[max_call_args].
 *
 *  @arg[ValueHeap*][heap] The heap to update.
 *  @arg[size_t][max_call_args] The max number of args to any call.
 *  @sideeffects
 *   Resizes the tail call buffer as needed. The args array may be
 *   reallocated; the previous value should not be referenced after this call.
 */
static void EnsureTailCallArgsSpace(ValueHeap* heap, size_t max_call_args)
{
  // We need space for double max_call_args. In the worst case:
  // 1 for the function.
  // max_call_args worth of args provided by the tail call.
  // max_call_args - 1 worth of 'unused' args left over from a call.
  size_t required = 2 * max_call_args;

  if (heap->tail_call_capacity < required) {
    heap->tail_call_capacity = required;
    heap->_base.tail_call_buffer = FbleReAllocArray(FbleValue*, heap->_base.tail_call_buffer, required);
  }
}

// FbleRunFunction for PartialApply executable.
// See documentation of FbleRunFunction in fble-value.h
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  size_t s = function->executable.num_statics;
  size_t a = function->executable.num_args;
  heap->tail_call_argc = a + s - 1;
  memcpy(heap->tail_call_buffer, function->statics, s * sizeof(FbleValue*));
  memcpy(heap->tail_call_buffer + s, args, a * sizeof(FbleValue*));
  return heap->tail_call_sentinel;
}

/**
 * @func[PartialApply] Partially applies a function.
 *  Creates a thunk with the function and arguments without applying the
 *  function yet.
 *
 *  @arg[ValueHeap*][heap] The value heap.
 *  @arg[FbleFuncValue*][function] The function to apply.
 *  @arg[size_t][argc] Number of args to pass.
 *  @arg[FbleValue**][args] Args to pass to the function.
 *
 *  @returns[FbleValue*] The allocated result.
 *
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
static FbleValue* PartialApply(ValueHeap* heap, FbleFuncValue* function, size_t argc, FbleValue** args)
{
  FbleExecutable exe = {
    .num_args = function->function.executable.num_args - argc,
    .num_statics = 1 + argc,
    .max_call_args = function->function.executable.num_args,
    .run = &PartialApplyImpl
  };

  FbleValue* statics[1 + argc];
  statics[0] = &function->_base;
  memcpy(statics + 1, args, argc * sizeof(FbleValue*));
  return FbleNewFuncValue(&heap->_base, &exe, function->function.profile_block_id, statics);
}

/**
 * @func[TailCall] Tail calls an fble function.
 *  Calls the function and args stored in heap->tail_call data.
 *
 *  @arg[ValueHeap*] heap
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
static FbleValue* TailCall(ValueHeap* heap, FbleProfileThread* profile)
{
  while (true) {
    FbleFuncValue* func = (FbleFuncValue*)heap->_base.tail_call_buffer[0];
    size_t argc = heap->_base.tail_call_argc;

    if (argc < func->function.executable.num_args) {
      FbleValue* partial = PartialApply(heap, (FbleFuncValue*)func, argc, heap->_base.tail_call_buffer + 1);
      return FblePopFrame(&heap->_base, partial);
    }

    if (profile) {
      FbleProfileReplaceBlock(profile, func->function.profile_block_id);
    }

    bool should_merge = heap->top->caller != NULL
      && heap->top->max == heap->top->caller->max
      && heap->top->top - heap->top->caller->top < MERGE_LIMIT;
    CompactFrame(heap, should_merge, 1 + argc, heap->_base.tail_call_buffer);

    func = (FbleFuncValue*)heap->_base.tail_call_buffer[0];
    argc = heap->_base.tail_call_argc;

    FbleValue* args[argc];
    memcpy(args, heap->_base.tail_call_buffer + 1, argc * sizeof(FbleValue*));

    FbleValue* result = func->function.executable.run(&heap->_base, profile, &func->function, args);

    size_t num_unused = argc - func->function.executable.num_args;
    FbleValue** unused = args + func->function.executable.num_args;

    if (result == heap->_base.tail_call_sentinel) {
      // Add the unused args to the end of the tail call args and make that
      // our new func and args.
      assert(heap->_base.tail_call_argc + num_unused < heap->tail_call_capacity);
      memcpy(heap->_base.tail_call_buffer + 1 + heap->_base.tail_call_argc, unused, num_unused * sizeof(FbleValue*));
      heap->_base.tail_call_argc += num_unused;
    } else if (num_unused > 0 && result != NULL) {
      // Do a tail call to the returned result with unused args applied.
      assert(num_unused < heap->tail_call_capacity);
      heap->_base.tail_call_argc = num_unused;
      heap->_base.tail_call_buffer[0] = result;
      memcpy(heap->_base.tail_call_buffer + 1, unused, num_unused * sizeof(FbleValue*));
    } else {
      return FblePopFrame(&heap->_base, result);
    }
  }

  FbleUnreachable("should never get here");
  return NULL;
}

/**
 * @func[Eval] Evaluates the given function.
 *  @arg[ValueHeap*][heap] The value heap.
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
static FbleValue* Eval(ValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile)
{
#ifndef __WIN32
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
#endif // __WIN32

  FbleProfileThread* profile_thread = FbleNewProfileThread(profile);
  FbleValue* result = FbleCall(&heap->_base, profile_thread, func, argc, args);
  FbleFreeProfileThread(profile_thread);

#ifndef __WIN32
  // Restore the stack limit to what it was before.
  if (setrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }
#endif // __WIN32

  return result;
}

// See documentation in fble-value.h
FbleValue* FbleCall(FbleValueHeap* heap_, FbleProfileThread* profile, FbleValue* function, size_t argc, FbleValue** args)
{
  ValueHeap* heap = (ValueHeap*)heap_;

  if (function == NULL || IsRefValue(function)) {
    FbleLoc loc = FbleNewLoc(__FILE__, __LINE__ + 1, 5);
    FbleReportError("called undefined function\n", loc);
    FbleFreeLoc(loc);
    return NULL;
  }
  FbleFunction* func = &((FbleFuncValue*)function)->function;
  FbleExecutable* executable = &func->executable;
  if (argc < executable->num_args) {
    return PartialApply(heap, (FbleFuncValue*)function, argc, args);
  }

  if (profile) {
    FbleProfileEnterBlock(profile, func->profile_block_id);
  }

  size_t num_unused = argc - executable->num_args;
  FbleValue** unused = args + executable->num_args;

  bool should_merge = heap->top->caller != NULL
    && heap->top->max == heap->top->caller->max
    && heap->top->top - heap->top->caller->top < MERGE_LIMIT;
  PushFrame(heap, should_merge);
  FbleValue* result = executable->run(&heap->_base, profile, func, args);

  if (result == heap->_base.tail_call_sentinel) {
    assert(heap->_base.tail_call_argc + num_unused < heap->tail_call_capacity);
    memcpy(heap->_base.tail_call_buffer + 1 + heap->_base.tail_call_argc, unused, num_unused * sizeof(FbleValue*));
    heap->_base.tail_call_argc += num_unused;
    result = TailCall(heap, profile);
  } else if (num_unused > 0 && result != NULL) {
    FbleValue* new_func = FblePopFrame(&heap->_base, result);
    result = FbleCall(&heap->_base, profile, new_func, num_unused, unused);
  } else {
    result = FblePopFrame(&heap->_base, result);
  }

  if (profile != NULL) {
    FbleProfileExitBlock(profile);
  }

  return result;
}

// See documentation in fble-value.h.
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, 0, NULL, profile);
}

// See documentation in fble-value.h.
FbleValue* FbleApply(FbleValueHeap* heap_, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile)
{
  ValueHeap* heap = (ValueHeap*)heap_;
  EnsureTailCallArgsSpace(heap, argc);
  return Eval(heap, func, argc, args, profile);
}

// See documentation in fble-value.h.
FbleValue* FbleNewFuncValue(FbleValueHeap* heap_, FbleExecutable* executable, size_t profile_block_id, FbleValue** statics)
{
  ValueHeap* heap = (ValueHeap*)heap_;
  EnsureTailCallArgsSpace(heap, executable->max_call_args);

  FbleFuncValue* v = NewValueExtra(heap, FbleFuncValue, FUNC_VALUE, executable->num_statics);
  v->function.profile_block_id = profile_block_id;
  memcpy(&v->function.executable, executable, sizeof(FbleExecutable));
  v->function.statics = v->statics;
  for (size_t i = 0; i < executable->num_statics; ++i) {
    v->statics[i] = statics[i];
  }
  return &v->_base;
}

// See documentation in fble-value.h.
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, 1, unit);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* arg = args[argc - i - 1];
    FbleValue* cons = FbleNewStructValue_(heap, 2, arg, tail);

    tail = FbleNewUnionValue(heap, 1, 0, cons);
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
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t tagwidth, size_t argc, size_t* args)
{
  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* tail = FbleNewUnionValue(heap, 1, 1, unit);
  for (size_t i = 0; i < argc; ++i) {
    size_t letter = args[argc - i - 1];
    FbleValue* arg = FbleNewUnionValue(heap, tagwidth, letter, unit);
    FbleValue* cons = FbleNewStructValue_(heap, 2, arg, tail);
    tail = FbleNewUnionValue(heap, 1, 0, cons);
  }
  return tail;
}

// See documentation in fble-value.h.
FbleValue* FbleDeclareRecursiveValues(FbleValueHeap* heap_, size_t n)
{
  ValueHeap* heap = (ValueHeap*)heap_;

  FbleValue* args[n];
  for (size_t i = 0; i < n; ++i) {
    args[i] = RefValue(heap->ref_id + i);
  }
  heap->ref_id += n;
  return FbleNewStructValue(heap_, n, args);
}

// See documentation in fble-value.h.
size_t FbleDefineRecursiveValues(FbleValueHeap* heap_, FbleValue* decl, FbleValue* defn)
{
  ValueHeap* heap = (ValueHeap*)heap_;
  assert(IsAlloced(decl) && "decl should have been alloced");
  assert((decl->flags & FbleValueFlagTagBits) == STRUCT_VALUE);
  FbleStructValue* sv = (FbleStructValue*)decl;
  size_t n = decl->data;
  FbleValue** refs = sv->fields;

  heap->ref_id -= n;
  uintptr_t refs_id = heap->ref_id;
  FbleValue* values[n];
  for (size_t i = 0; i < n; ++i) {
    // GcRealloc the values to make sure we don't end up with a GcAllocated
    // value pointing to a stack allocated value.
    values[i] = GcRealloc(heap, FbleStructValueField(defn, n, i));

    // Double check the ref values are as expected while we are at it.
    assert(IsRefValue(refs[i]));
    assert(RefValueId(refs[i]) == refs_id + i); 
  }

  // Eliminate any occurences of refs in the values array.
  for (size_t i = 0; i < n; ++i) {
    if (refs[i] == values[i]) {
      // vacuous value.
      return i+1;
    }

    for (size_t j = 0; j < n; ++j) {
      if (refs[i] == values[j]) {
        values[j] = values[i];
      }
    }
  }

  // Do assignments inside of all the values.
  for (size_t i = 0; i < n; ++i) {
    RefsAssign(heap, refs_id, values, values[i]);
  }

  // Write back the final assignments.
  for (size_t i = 0; i < n; ++i) {
    refs[i] = values[i];
  }
  return 0;
}

// See documentation in fble-value.h
FbleValue* FbleNewNativeValue(FbleValueHeap* heap_,
    void* data, void (*on_free)(void* data))
{
  ValueHeap* heap = (ValueHeap*)heap_;

  NativeValue* value = NewGcValue(heap, heap->top, NativeValue, NATIVE_VALUE);
  value->data = data;
  value->on_free = on_free;
  return &value->_base;
}

// See documentation in fble-value.h
void* FbleNativeValueData(FbleValue* value)
{
  assert((value->flags & FbleValueFlagTagBits) == NATIVE_VALUE);
  return ((NativeValue*)value)->data;
}

// See documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap_)
{
  ValueHeap* heap = (ValueHeap*)heap_;

  while (!(heap->gc.next == NULL
      && IsEmpty(&heap->gc.frame->marked)
      && IsEmpty(&heap->gc.frame->unmarked))) {
    IncrGc(heap);
  }
}
