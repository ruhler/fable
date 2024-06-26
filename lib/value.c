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
// Reference values and native values are GC allocated up front, they are
// never stack allocated.
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
// all stack frames until we encounter a recursive function recursive call.
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

/**
 * @struct[List] Circular, doubly linked list of values.
 *  @field[List*][next] The next element in the list.
 *  @field[List*][prev] The previous element in the list.
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

typedef struct Frame Frame;

/** Different kinds of FbleValue. */
typedef enum {
  STRUCT_VALUE,
  UNION_VALUE,
  FUNC_VALUE,
  REF_VALUE,
  NATIVE_VALUE,
} ValueTag;

/**
 * @struct[StackHeader] Object header for values allocated to the stack.
 *  @field[FbleValue*][gc]
 *   The equivalent GC value allocated for this value, or NULL if this value
 *   has not been GC allocated yet.
 *  @field[Frame*][frame] The frame this value is allocated to.
 */
typedef struct {
  FbleValue* gc;
  Frame* frame;
} StackHeader;

/**
 * @struct[GcHeader] Object header for GC allocated values.
 *  @field[List][list] A list of values this value belongs to.
 *  @field[uint64_t][gen] Generation this object is allocated in.
 */
typedef struct {
  List list;
  uint64_t gen;
} GcHeader;

// Where an object is allocated.
typedef enum {
  GC_ALLOC,
  STACK_ALLOC
} AllocLoc;

// Value header depending on a value's AllocLoc.
typedef union {
  GcHeader gc;
  StackHeader stack;
} Header;

/**
 * @struct[FbleValue] Base class for fble values.
 *  A tagged union of value types. All values have the same initial layout as
 *  FbleValue. The tag can be used to determine what kind of value this is to
 *  get access to additional fields of the value by first casting to that
 *  specific type of value.
 *
 *  IMPORTANT: Some fble values are packed directly in the FbleValue* pointer
 *  to save space. An FbleValue* only points to an FbleValue if the least
 *  significant bit of the pointer is 0.
 *
 *  @field[Header][h] The object header.
 *  @field[ValueTag][tag] The kind of value.
 *  @field[AllocLoc][loc] Where the object is allocated.
 */
struct FbleValue {
  // Note: Header.gc.list must be the first thing in FbleValue so we can map
  // from GC list pointers back to FbleValue. 
  Header h;
  ValueTag tag;
  AllocLoc loc;
};

/**
 * @struct[StructValue] STRUCT_VALUE
 *  An fble struct value.
 *
 *  Struct values may be packed. If so, read in order from the least
 *  significant bits:
 *  '0' - to indicate it is a struct value instead of a union value.
 *  'num_args' - unary encoded number of arguments in the struct.
 *    e.g. "0" for 0 args, "10" for 1 arg, "110" for 2 args, and so on.
 *  'args' in order of argument arg0, arg1, etc.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[size_t][fieldc] Number of fields.
 *  @field[FbleValue**][fields] Field values.
 */
typedef struct {
  FbleValue _base;
  size_t fieldc;
  FbleValue* fields[];
} StructValue;

/**
 * @struct[UnionValue] UNION_VALUE
 *  An fble union value.
 *
 *  Union values may be packed. If so, read in order from the least
 *  significant bit:
 *  '1' - to indicate it is a union value instead of a struct value.
 *  'tag' - the tag, using a unary encoding terminated with 0.
 *    e.g. "0" for 0, "10" for 1, "110" for 2, and so on.
 *  'arg' - the argument.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[size_t][tag] Union tag value.
 *  @field[FbleValue*][arg] Union argument value.
 */
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} UnionValue;

/**
 * @struct[FuncValue] FUNC_VALUE
 *  An fble function value.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[FbleFunction][function] Function information.
 *  @field[FbleValue**][statics] Storage location for static variables.
 */
typedef struct {
  FbleValue _base;
  FbleFunction function;
  FbleValue* statics[];
} FuncValue;

/**
 * @struct[RefValue] A reference value.
 *  An implementation-specific value introduced to support recursive values
 *  and not yet computed values.
 *
 *  A ref value holds a reference to another value. All values must be
 *  dereferenced before being otherwise accessed in case they are ref values.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[FbleValue*][value] The referenced value, or NULL.
 */
typedef struct {
  FbleValue _base;
  FbleValue* value;
} RefValue;

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
 * @func[NewValue] Allocates a new value of the given type.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on
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
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on
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
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
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

static void* StackAlloc(FbleValueHeap* heap, size_t size);
static FbleValue* NewValueRaw(FbleValueHeap* heap, ValueTag tag, size_t size);
static FbleValue* NewGcValueRaw(FbleValueHeap* heap, Frame* frame, ValueTag tag, size_t size);
static void FreeGcValue(FbleValue* value);
static FbleValue* GcRealloc(FbleValueHeap* heap, FbleValue* value);

static bool IsPacked(FbleValue* value);
static bool IsAlloced(FbleValue* value);
static size_t PackedValueLength(intptr_t data);
static FbleValue* StrictValue(FbleValue* value);

// We allocate memory for the stack in 1MB chunks.
#define CHUNK_SIZE (1024 * 1024)

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
 * @struct[FbleValueHeap] The runtime stack and heap.
 *  @field[void*][stack] The base of the stack.
 *  @field[Frame*][top]
 *   The top frame of the stack. New values are allocated here.
 *  @field[Gc][gc] Info about currently running GC.
 *  @field[Chunk*][chunks]
 *   Chunks of allocated stack memory not currently in use.
 */
struct FbleValueHeap {
  void* stack;
  Frame* top;
  Gc gc;
  Chunk* chunks;
};

static void MarkRef(Gc* gc, FbleValue* src, FbleValue* dst);
static void MarkRefs(Gc* gc, FbleValue* value);
static void IncrGc(FbleValueHeap* heap);
static void PushFrame(FbleValueHeap* heap, bool merge);
static void CompactFrame(FbleValueHeap* heap, bool merge, size_t n, FbleValue** save);

static FbleValue* TailCallSentinel = (FbleValue*)0x2;

/**
 * @struct[TailCallData] Side buffer to store tail call data.
 *  @field[size_t][capacity] Number of allocated slots in func_and_args.
 *  @field[size_t][argc] Number of args, not including func.
 *  @field[FbleValue**][func_and_args] Tail call function followed by args.
 */
typedef struct {
  size_t capacity;
  size_t argc;
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
  got->h.gc.list.prev->next = got->h.gc.list.next;
  got->h.gc.list.next->prev = got->h.gc.list.prev;
  Clear(&got->h.gc.list);
  return got;
}

/**
 * @func[MoveTo] Moves an value from one list to another.
 *  @arg[List*][dst] The list to move the value to.
 *  @arg[FbleValue*][value] The value to move.
 *  @sideeffects
 *   Moves the value from its current list to @a[dst].
 */
static void MoveTo(List* dst, FbleValue* value)
{
  value->h.gc.list.prev->next = value->h.gc.list.next;
  value->h.gc.list.next->prev = value->h.gc.list.prev;
  value->h.gc.list.next = dst->next;
  value->h.gc.list.prev = dst;
  dst->next->prev = &value->h.gc.list;
  dst->next = &value->h.gc.list;
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
 * @func[StackAlloc] Allocates memory on the stack.
 *  @arg[Heap*][heap] The heap to allocate the memory to.
 *  @arg[size_t][size] The number of bytes to allocate.
 *  @returns[void*] Pointer to the allocated memory.
 *  @sideeffects
 *   Allocates a new stack chunk if needed.
 */
static void* StackAlloc(FbleValueHeap* heap, size_t size)
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
 *  @arg[FbleValueHeap*][heap] The heap to allocate to.
 *  @arg[ValueTag][tag] The tag of the new value.
 *  @arg[size_t][size] The number of bytes to allocate for the value.
 *  @returns[FbleValue*] The newly allocated value.
 *  @sideeffects
 *   Allocates a value to the top frame of the heap.
 */
static FbleValue* NewValueRaw(FbleValueHeap* heap, ValueTag tag, size_t size)
{
  FbleValue* value = StackAlloc(heap, size);
  value->h.stack.gc = NULL;
  value->h.stack.frame = heap->top;
  value->tag = tag;
  value->loc = STACK_ALLOC;
  return value;
}

/**
 * @func[NewGcValueRaw] Allocates a new value on the heap.
 *  @arg[FbleValueHeap*][heap] The heap to allocate on.
 *  @arg[Frame*][frame] The frame to allocate to.
 *  @arg[ValueTag][tag] The tag of the new value.
 *  @arg[size_t][size] The number of bytes to allocate for the value.
 *  @returns[FbleValue*] The newly allocated value.
 *  @sideeffects
 *   Allocates a value to the given frame of the heap.
 */
static FbleValue* NewGcValueRaw(FbleValueHeap* heap, Frame* frame, ValueTag tag, size_t size)
{
  IncrGc(heap);

  FbleValue* value = (FbleValue*)FbleAllocRaw(size);
  value->tag = tag;
  value->loc = GC_ALLOC;
  value->h.gc.gen = frame->gen;

  Clear(&value->h.gc.list);
  MoveTo(&frame->alloced, value);
  return value;
}

/**
 * @func[FreeGcValue] Frees a GC allocated value.
 *  @arg[FbleValue*][value] The value to free. May be NULL.
 *  @sideeffects
 *   @i Frees any resources outside of the heap that this value holds on to.
 *   @i Behavior is undefined if the value is not GC allocated.
 */
static void FreeGcValue(FbleValue* value)
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
 * @func[GcRealloc] Reallocate a value onto the heap.
 *  @arg[FbleValueHeap*][heap] The heap to allocate to.
 *  @arg[FbleValue*][value] The value to allocate.
 *  @returns[FbleValue*] A gc allocated value equivalent to @a[value].
 *  @sideeffects
 *   @i Allocates GC values on the heap.
 *   @i Records mapping from original value to GC value on the stack.
 */
static FbleValue* GcRealloc(FbleValueHeap* heap, FbleValue* value)
{
  // Packed values and NULL need not be allocated at all.
  if (!IsAlloced(value)) {
    return value;
  }

  // If the value is already a GC value, there's nothing to do.
  if (value->loc == GC_ALLOC) {
    return value;
  }

  // If the value has already been GC allocated, return the associated GC
  // allocated value.
  if (value->h.stack.gc != NULL) {
    return value->h.stack.gc;
  }

  switch (value->tag) {
    case STRUCT_VALUE: {
      StructValue* sv = (StructValue*)value;
      StructValue* nv = NewGcValueExtra(heap, value->h.stack.frame, StructValue, STRUCT_VALUE, sv->fieldc);
      value->h.stack.gc = &nv->_base;
      
      nv->fieldc = sv->fieldc;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        nv->fields[i] = GcRealloc(heap, sv->fields[i]);
      }
      return &nv->_base;
    }

    case UNION_VALUE: {
      UnionValue* uv = (UnionValue*)value;
      UnionValue* nv = NewGcValue(heap, value->h.stack.frame, UnionValue, UNION_VALUE);
      value->h.stack.gc = &nv->_base;
      nv->tag = uv->tag;
      nv->arg = GcRealloc(heap, uv->arg);
      return &nv->_base;
    }

    case FUNC_VALUE: {
      FuncValue* fv = (FuncValue*)value;
      FuncValue* nv = NewGcValueExtra(heap, value->h.stack.frame, FuncValue, FUNC_VALUE, fv->function.executable.num_statics);
      value->h.stack.gc = &nv->_base;
      memcpy(&nv->function.executable, &fv->function.executable, sizeof(FbleExecutable));
      nv->function.profile_block_id = fv->function.profile_block_id;
      nv->function.statics = nv->statics;
      for (size_t i = 0; i < nv->function.executable.num_statics; ++i) {
        nv->statics[i] = GcRealloc(heap, fv->statics[i]);
      }
      return &nv->_base;
    }

    case REF_VALUE: {
      FbleUnreachable("ref value should already be GC allocated.");
      return NULL;
    }

    case NATIVE_VALUE: {
      FbleUnreachable("native value should already be GC allocated.");
      return NULL;
    }
  }

  FbleUnreachable("should never get here");
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
 * @func[PackedValueLength] Computes bits needed for a packed value.
 *  Compute the number of bits needed for the value packed into the least
 *  significat bits of @a[data]. @a[data] should not include the pack marker.
 *
 *  @arg[intptr_t][data]
 *   Raw data bits for a packed value without the pack marker.
 *
 *  @returns[size_t]
 *   The number of bits of the data used to describe that value.
 *
 *  @sideeffects
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
  if (IsAlloced(dst)
      && dst->h.gc.gen >= gc->min_gen
      && dst->h.gc.gen != gc->gen) {
    MoveTo(&gc->marked, dst);
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
  switch (value->tag) {
    case STRUCT_VALUE: {
      StructValue* sv = (StructValue*)value;
      for (size_t i = 0; i < sv->fieldc; ++i) {
        MarkRef(gc, value, sv->fields[i]);
      }
      break;
    }

    case UNION_VALUE: {
      UnionValue* uv = (UnionValue*)value;
      MarkRef(gc, value, uv->arg);
      break;
    }

    case FUNC_VALUE: {
      FuncValue* v = (FuncValue*)value;
      for (size_t i = 0; i < v->function.executable.num_statics; ++i) {
        MarkRef(gc, value, v->statics[i]);
      }
      break;
    }

    case REF_VALUE: {
      RefValue* v = (RefValue*)value;
      if (v->value != NULL) {
        MarkRef(gc, value, v->value);
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
  Gc* gc = &heap->gc;

  // Free a couple objects on the free list.
  FreeGcValue(Get(&gc->free));
  FreeGcValue(Get(&gc->free));

  // Traverse an object on the heap.
  FbleValue* marked = Get(&gc->marked);
  if (marked != NULL) {
    marked->h.gc.gen = gc->gen;
    MarkRefs(gc, marked);

    if (gc->interrupted) {
      // GC was interrupted during pop frame or compact, so this object should
      // be moved to 'unmarked'.
      MoveTo(&gc->frame->unmarked, marked);
    } else {
      MoveTo(&gc->frame->alloced, marked);
    }
    return;
  }

  // Anything left unmarked is unreachable.
  MoveAllTo(&gc->free, &gc->unmarked);

  // Resurrect anything that needs saving due to interrupted GC.
  for (size_t i = 0; gc->save.xs[i] != NULL; ++i) {
    MoveTo(&gc->frame->marked, gc->save.xs[i]);
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
  FbleValueHeap* heap = FbleAlloc(FbleValueHeap);

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

  for (FbleValue* value = Get(&values); value != NULL; value = Get(&values)) {
    FreeGcValue(value);
  }
  FbleFree(heap);
}

// See documentation of FblePushFrame in fble-value.h
// If merge is true, reuse the same allocation space as the caller frame.
static void PushFrame(FbleValueHeap* heap, bool merge)
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
  PushFrame(heap, false);
}

// See documentation in fble-value.h
FbleValue* FblePopFrame(FbleValueHeap* heap, FbleValue* value)
{
  Frame* top = heap->top;
  if (top->merges > 0) {
    top->merges--;
    return value;
  }

  value = GcRealloc(heap, value);

  heap->top = heap->top->caller;

  MoveAllTo(&heap->top->unmarked, &top->unmarked);
  MoveAllTo(&heap->top->unmarked, &top->marked);
  MoveAllTo(&heap->top->unmarked, &top->alloced);

  if (IsAlloced(value) && value->h.gc.gen >= top->min_gen) {
    heap->top->max_gen = top->max_gen;
    MoveTo(&heap->top->marked, value);
  }

  if (heap->gc.frame == top) {
    // We are popping the frame currently being GC'd.
    heap->gc.interrupted = true;
    heap->gc.frame = heap->top;

    // If the value we are returning is currently undergoing GC, keep it there
    // until GC has a chance to finish.
    if (IsAlloced(value)
        && value->h.gc.gen >= heap->gc.min_gen
        && value->h.gc.gen != heap->gc.gen
        && value->h.gc.gen < heap->gc.max_gen) {
      MoveTo(&heap->gc.marked, value);
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
 *  @arg[FbleValueHeap*][heap] The heap.
 *  @arg[bool][merge] If true, skip compaction.
 *  @arg[size_t][n] The number of save values.
 *  @arg[FbleValue**][save] Values to save.
 *
 *  @sideeffects
 *   Compacts the heap frame. Updates pointers in the save array to their new
 *   values. You must not reference any values allocated to the frame after
 *   this call except for the updated values in the save array.
 */
static void CompactFrame(FbleValueHeap* heap, bool merge, size_t n, FbleValue** save)
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
    if (IsAlloced(save[i]) && save[i]->h.gc.gen >= heap->top->min_gen) {
      MoveTo(&heap->top->marked, save[i]);
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
      if (IsAlloced(save[i])
          && save[i]->h.gc.gen >= heap->gc.min_gen
          && save[i]->h.gc.gen != heap->gc.gen
          && save[i]->h.gc.gen < heap->gc.max_gen) {
        MoveTo(&heap->gc.marked, save[i]);
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
    gTailCallData.func_and_args = FbleReAllocArray(FbleValue*, gTailCallData.func_and_args, capacity);
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

    CompactFrame(heap, func->tag != REF_VALUE, 1 + argc, gTailCallData.func_and_args);

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
    } else if (num_unused > 0 && result != NULL) {
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

#ifndef __WIN32
  // Restore the stack limit to what it was before.
  if (setrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }
#endif // __WIN32

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

  PushFrame(heap, function->tag != REF_VALUE);
  FbleValue* result = executable->run(heap, profile, func, args);

  if (result == TailCallSentinel) {
    EnsureTailCallArgsSpace(gTailCallData.argc + num_unused);
    memcpy(gTailCallData.func_and_args + 1 + gTailCallData.argc, unused, num_unused * sizeof(FbleValue*));
    gTailCallData.argc += num_unused;
    result = TailCall(heap, profile);
  } else if (num_unused > 0 && result != NULL) {
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
  memcpy(&v->function.executable, executable, sizeof(FbleExecutable));
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
  RefValue* rv = NewGcValue(heap, heap->top, RefValue, REF_VALUE);
  rv->value = NULL;
  return &rv->_base;
}

// See documentation in fble-value.h.
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue* ref, FbleValue* value)
{
  assert(ref->h.gc.gen >= heap->top->min_gen
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
  rv->value = GcRealloc(heap, value);
  return true;
}

// See documentation in fble-value.h
FbleValue* FbleNewNativeValue(FbleValueHeap* heap,
    void* data, void (*on_free)(void* data))
{
  NativeValue* value = NewGcValue(heap, heap->top, NativeValue, NATIVE_VALUE);
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

// See documentation in fble-value.h
void FbleValueFullGc(FbleValueHeap* heap)
{
  while (!(heap->gc.next == NULL
      && IsEmpty(&heap->gc.frame->marked)
      && IsEmpty(&heap->gc.frame->unmarked))) {
    IncrGc(heap);
  }
}
