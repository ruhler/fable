// ms-heap.c --
//   This file implements a mark-sweep based heap.

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool

#include "fble-alloc.h"
#include "fble-heap.h"

// Space --
//   Which space the object belongs to.
//
// A and B are different spaces that are designated as the "to" space or the
// "from" space depending on the phase of GC we are on. They can be used to
// check which space an object is in, and instantly swap all objects in the
// "to" space with all objects in the "from" space.
//
// Root objects in the "from" root space have yet to be traversed.
// Root objects in the "to" root space have already been traversed.
// Root objects are not allowed to be in the PENDING space.
// Non-root objects in the "from" space have not yet been seen.
// Non-root objects in the PENDING space are reachable, but have not yet been
//    traversed.
// Non-root objects in the "to" space are reachable and have been traversed.
typedef enum {
  A,
  B,
  PENDING,
} Space;

// Obj --
//   An object allocated on the heap.
//
// Fields:
//   prev - previous entry in doubly linked list of objects.
//   next - next entry in doubly linked list of objects.
//   space - which space the object currently belongs to.
//   refcount - the number of external (non-cyclic) references to this object.
//              Objects with refcount greater than 0 are roots.
//   obj - the obj pointer visible to the user.
typedef struct Obj {
  struct Obj* prev;
  struct Obj* next;
  Space space;
  size_t refcount;

  char obj[];
} Obj;

// Heap --
//   The FbleHeap for the mark sweek heap.
//
// GC traverses objects in the "from" space, moving any of those objects that
// are reachable to the "to" space. Objects are "pending" when they have been
// identified as reachable, but have not yet been traversed.
//
// The Obj fields are dummy nodes for a list of objects.
//
// Fields:
//   to - List of non-root objects in the "to" space.
//   from - List of non-root objects in the "from" space.
//   pending - List of non-root objects in the PENDING space.
//   roots_to - List of root objects in the "to" space.
//   roots_from - List of root objects in the "from" space.
//   free - List of free objects.
//
//   to_space - Which space, A or B, is currently the "to" space.
//   from_space - Which space, A or B, is currently the "from" space.
typedef struct {
  FbleHeap _base;

  Obj* to;
  Obj* from;
  Obj* pending;
  Obj* roots_to;
  Obj* roots_from;
  Obj* free;

  Space to_space;
  Space from_space;
} Heap;

static Obj* ToObj(void* obj_);
static void MarkRefs(Heap* heap, Obj* obj);
static bool IncrGc(Heap* heap);
static void FullGc(Heap* heap);

// ToObj --
//   Get the Obj* corresponding to a void* obj pointer.
//
// Inputs:
//   obj_ - the void* obj pointer.
//
// Results:
//   The corresponding Obj* pointer.
//
// Side effects:
//   None.
static Obj* ToObj(void* obj_)
{
  Obj* obj = ((Obj*)obj_) - 1;
  assert(obj->obj == obj_);
  return obj;
}

// MarkRefs --
//   Visit the references from the given object for the purposes of marking.
//
// Inputs:
//   heap - the heap
//   obj - the object to visit.
//
// Side effects:
//   Marks the visited references, moving them to the pending space if
//   appropriate.
typedef struct {
  FbleHeapCallback _base;
  Heap* heap;
} MarkRefsCallback;

static void MarkRef(MarkRefsCallback* this, void* obj_)
{
  Obj* obj = ToObj(obj_);
  Heap* heap = this->heap;

  // Move non-root "from" space objects to pending.
  if (obj->refcount == 0 && obj->space == heap->from_space) {
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = heap->pending->next;
    obj->prev = heap->pending;
    obj->space = PENDING;
    heap->pending->next->prev = obj;
    heap->pending->next = obj;
  }
}

static void MarkRefs(Heap* heap, Obj* obj)
{
  MarkRefsCallback callback = {
    ._base = { .callback = (void(*)(FbleHeapCallback*, void*))&MarkRef },
    .heap = heap
  };
  heap->_base.refs(&callback._base, obj->obj);
}

// IncrGc --
//   Do an incremental amount of GC work.
//
// Inputs:
//   heap - the heap to do GC on.
//
// Result: true if this completed a round of GC. False otherwise.
//
// Side effects:
//   Does some GC work, which may involve moving objects around, traversing
//   objects, freeing objects, etc.
bool IncrGc(Heap* heap)
{
  FbleArena* arena = heap->_base.arena;

  // Free a couple of objects on the free list.
  // Assuming we do incremental GC for each new allocation, it's good to free
  // two objects so that we free at a faster rate than we allocate.
  for (size_t i = 0; i < 2 && heap->free->next != heap->free; ++i) {
    Obj* obj = heap->free->next;
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    heap->_base.on_free(&heap->_base, obj->obj);
    FbleFree(arena, obj);
  }

  // Traverse some roots.
  // Assuming we do incremental GC for each new allocation, we should traverse
  // 3 objects per allocation. That way fewer than N new objects will be
  // allocated in the time it takes us to traverse a heap with N objects (in
  // addition to whatever objects are allocated while traversing that heap).
  for (size_t i = 0; i < 3 && heap->roots_from->next != heap->roots_from; ++i) {
    Obj* obj = heap->roots_from->next;
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = heap->roots_to->next;
    obj->prev = heap->roots_to;
    obj->space = heap->to_space;
    heap->roots_to->next->prev = obj;
    heap->roots_to->next = obj;
    MarkRefs(heap, obj);
  }

  // Traverse some pending objects.
  // Assuming we do incremental GC for each new allocation, we should traverse
  // 3 objects per allocation. That way fewer than N new objects will be
  // allocated in the time it takes us to traverse a heap with N objects (in
  // addition to whatever objects are allocated while traversing that heap).
  for (size_t i = 0; i < 3 && heap->pending->next != heap->pending; ++i) {
    Obj* obj = heap->pending->next;
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = heap->to->next;
    obj->prev = heap->to;
    obj->space = heap->to_space;
    heap->to->next->prev = obj;
    heap->to->next = obj;
    MarkRefs(heap, obj);
  }

  // If we are done with gc, start a new GC by swapping from and to spaces.
  if (heap->roots_from->next == heap->roots_from
      && heap->pending->next == heap->pending) {
    // Anything still in the "from" space is unreachable. Move it to the free
    // space.
    if (heap->from->next != heap->from) {
      heap->free->next->prev = heap->from->prev;
      heap->from->prev->next = heap->free->next;
      heap->free->next = heap->from->next;
      heap->free->next->prev = heap->free;
    }
    heap->from->next = heap->from;
    heap->from->prev = heap->from;

    // Move the "to" space to the "from" space.
    if (heap->to->next != heap->to) {
      heap->to->next->prev = heap->from;
      heap->to->prev->next = heap->from;
      heap->from->next = heap->to->next;
      heap->from->prev = heap->to->prev;
    }
    heap->to->next = heap->to;
    heap->to->prev = heap->to;

    // Move the roots to the roots_from space.
    if (heap->roots_to->next != heap->roots_to) {
      heap->roots_to->next->prev = heap->roots_from;
      heap->roots_to->prev->next = heap->roots_from;
      heap->roots_from->next = heap->roots_to->next;
      heap->roots_from->prev = heap->roots_to->prev;
    }
    heap->roots_to->next = heap->roots_to;
    heap->roots_to->prev = heap->roots_to;

    Space tmp = heap->to_space;
    heap->to_space = heap->from_space;
    heap->from_space = tmp;

    // GC finished. Yay!
    return true;
  }

  // GC hasn't finished yet.
  return false;
}

// FullGc --
//   Do a full GC, collecting all objects that are unreachable at the time of
//   this call.
//
// Inputs:
//   heap - the heap to do GC on.
//
// Side effects:
//   Does full GC, freeing all unreachable objects.
void FullGc(Heap* heap)
{
  // Finish the GC in progress.
  while (!IncrGc(heap));

  // Do another round of full GC to catch any references that were removed
  // during the previos GC.
  while (!IncrGc(heap));

  // Clean up all the free objects.
  while (heap->free->next != heap->free) {
    Obj* obj = heap->free->next;
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    heap->_base.on_free(&heap->_base, obj->obj);
    FbleFree(heap->_base.arena, obj);
  }
}

// New -- see documentation for FbleHeap.new in fble-heap.h
static void* New(FbleHeap* heap_, size_t size)
{
  Heap* heap = (Heap*)heap_;
  IncrGc(heap);

  // Objects are allocated as roots.
  Obj* obj = FbleAllocExtra(heap->_base.arena, Obj, size);
  obj->next = heap->roots_to->next;
  obj->prev = heap->roots_to;
  obj->space = heap->to_space;
  obj->refcount = 1;
  heap->roots_to->next->prev = obj;
  heap->roots_to->next = obj;
  return obj->obj;
}

// Retain -- see documentation for FbleHeap.retain in fble-heap.h
static void Retain(FbleHeap* heap_, void* obj_)
{
  Heap* heap = (Heap*)heap_;
  Obj* obj = ToObj(obj_);
  if (obj->refcount++ == 0) {
    // This is a new root. Move it to roots_from.
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = heap->roots_from->next;
    obj->prev = heap->roots_from;
    obj->space = heap->from_space;
    heap->roots_from->next->prev = obj;
    heap->roots_from->next = obj;
  }
}

// Release -- see documentation for FbleHeap.release in fble-heap.h
static void Release(FbleHeap* heap_, void* obj_)
{
  Heap* heap = (Heap*)heap_;
  Obj* obj = ToObj(obj_);
  if (--obj->refcount == 0) {
    // This is no longer a root.
    if (obj->space == heap->from_space) {
      // The object may be reachable from some other root that we have already
      // traversed. Conservatively assume that's the case and mark this object
      // as pending.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->pending->next;
      obj->prev = heap->pending;
      obj->space = PENDING;
      heap->pending->next->prev = obj;
      heap->pending->next = obj;
    } else {
      // We've already traversed this object. Move it to the to space.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->to->next;
      obj->prev = heap->to;
      obj->space = heap->to_space;
      heap->to->next->prev = obj;
      heap->to->next = obj;
    }
  }
}

// AddRef -- see documentation for FbleHeap.add_ref in fble-heap.h
static void AddRef(FbleHeap* heap_, void* src_, void* dst_)
{
  Heap* heap = (Heap*)heap_;
  Obj* src = ToObj(src_);
  Obj* dst = ToObj(dst_);

  // Mark dst as pending if the following conditions hold:
  // * we have already traversed the src node.
  // * we have not already traversed the dst node.
  // * the dst node is not a root.
  // Because this means dst is now reachable by root when it may not otherwise
  // have been.
  if (src->space == heap->to_space
      && dst->space == heap->from_space
      && dst->refcount == 0) {
      dst->prev->next = dst->next;
      dst->next->prev = dst->prev;
      dst->next = heap->pending->next;
      dst->prev = heap->pending;
      dst->space = PENDING;
      heap->pending->next->prev = dst;
      heap->pending->next = dst;
  }
}

// DelRef -- see documentation for FbleHeap.del_ref in fble-heap.h
static void DelRef(FbleHeap* heap, void* src_, void* dst_)
{
  // Nothing to do. Ideally we arrange for this function to never be called.
}

// FbleNewMarkSweepHeap -- see documentation in fble-heap.h
FbleHeap* FbleNewMarkSweepHeap(
    FbleArena* arena, 
    void (*refs)(FbleHeapCallback*, void*),
    void (*on_free)(FbleHeap*, void*))
{
  Heap* heap = FbleAlloc(arena, Heap);
  heap->_base.arena = arena;
  heap->_base.refs = refs;
  heap->_base.on_free = on_free;

  heap->_base.new = &New;
  heap->_base.retain = &Retain;
  heap->_base.release = &Release;
  heap->_base.add_ref = &AddRef;
  heap->_base.del_ref = &DelRef;

  heap->to = FbleAlloc(arena, Obj);
  heap->from = FbleAlloc(arena, Obj);
  heap->pending = FbleAlloc(arena, Obj);
  heap->roots_to = FbleAlloc(arena, Obj);
  heap->roots_from = FbleAlloc(arena, Obj);
  heap->free = FbleAlloc(arena, Obj);

  heap->to->prev = heap->to;
  heap->to->next = heap->to;
  heap->from->prev = heap->from;
  heap->from->next = heap->from;
  heap->pending->prev = heap->pending;
  heap->pending->next = heap->pending;
  heap->roots_to->prev = heap->roots_to;
  heap->roots_to->next = heap->roots_to;
  heap->roots_from->prev = heap->roots_from;
  heap->roots_from->next = heap->roots_from;
  heap->free->prev = heap->free;
  heap->free->next = heap->free;
  heap->to_space = A;
  heap->from_space = B;
  return &heap->_base;
}

// FbleFreeMarkSweepHeap -- see documentation in fble-heap.h
void FbleFreeMarkSweepHeap(FbleHeap* heap_)
{
  Heap* heap = (Heap*)heap_;
  FullGc(heap);

  FbleArena* arena = heap->_base.arena;

  FbleFree(arena, heap->to);
  FbleFree(arena, heap->from);
  FbleFree(arena, heap->pending);
  FbleFree(arena, heap->roots_to);
  FbleFree(arena, heap->roots_from);
  FbleFree(arena, heap->free);
  FbleFree(arena, heap);
}
