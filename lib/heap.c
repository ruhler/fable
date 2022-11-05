// heap.c --
//   This file implements a mark-sweep based heap.

#include "heap.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>

// Space --
//   Which space the object belongs to.
//
// A and B are different spaces that are designated as the "to" space or the
// "from" space depending on the phase of GC we are on. They can be used to
// check which space an object is in, and instantly swap all objects in the
// "to" space with all objects in the "from" space.
//
// Root objects in the "to" root space have already been traversed.
// They live on heap->roots_to.
//
// Root objects in the "from" root space have yet to be traversed, and so far
// are otherwise unreachable aside from being roots.
// They live on heap->roots_from.
//
// Root objects in the PENDING root space have yet to be traversed, but are
// otherwise reachable even if they weren't roots.
// They live on heap->roots_from.
//
// Non-root objects in the "from" space have not yet been seen.
// They live on heap->from.
//
// Non-root objects in the PENDING space are reachable, but have not yet been
// traversed.
// They live on heap->pending.
//
// Non-root objects in the "to" space are reachable and have been traversed.
// They live on heap->to.
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

// ToObj --
//   Get the Obj* corresponding to a void* obj pointer.
//
// Inputs:
//   obj - the void* obj pointer.
//
// Results:
//   The corresponding Obj* pointer.
//
// Side effects:
//   None.
//
// Notes: defined as a macro instead of a function to improve performance.
#define ToObj(obj) (((Obj*)obj)-1)

// FbleHeap -- see documentation in heap.h.
//
// GC traverses objects in the "from" space, moving any of those objects that
// are reachable to the "to" space. Objects are "pending" when they have been
// identified as reachable, but have not yet been traversed.
//
// The Obj fields are dummy nodes for a list of objects.
//
// Fields:
//   refs - The refs callback to traverse reference from an object.
//   on_free - The on_free callback to indicate an object has been freed.
//
//   to - List of non-root objects in the "to" space.
//   from - List of non-root objects in the "from" space.
//   pending - List of non-root objects in the PENDING space.
//   roots_to - List of root objects in the "to" space.
//   roots_from - List of root objects in the "from" and PENDING space.
//   free - List of free objects.
//
//   to_space - Which space, A or B, is currently the "to" space.
//   from_space - Which space, A or B, is currently the "from" space.
struct FbleHeap {
  void (*refs)(FbleHeapCallback* callback, void* obj);
  void (*on_free)(FbleHeap* heap, void* obj);

  Obj* to;
  Obj* from;
  Obj* pending;
  Obj* roots_to;
  Obj* roots_from;
  Obj* free;

  Space to_space;
  Space from_space;
};

static void MarkRefs(FbleHeap* heap, Obj* obj);
static bool IncrGc(FbleHeap* heap);

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
  FbleHeap* heap;
} MarkRefsCallback;

static void MarkRef(MarkRefsCallback* this, void* obj_)
{
  Obj* obj = ToObj(obj_);
  FbleHeap* heap = this->heap;

  // Move "from" space objects to pending.
  if (obj->space == heap->from_space) {
    obj->space = PENDING;
    if (obj->refcount == 0) {
      // Non-root objects move to pending.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->pending->next;
      obj->prev = heap->pending;
      heap->pending->next->prev = obj;
      heap->pending->next = obj;
    }
  }
}

static void MarkRefs(FbleHeap* heap, Obj* obj)
{
  MarkRefsCallback callback = {
    ._base = { .callback = (void(*)(FbleHeapCallback*, void*))&MarkRef },
    .heap = heap
  };
  heap->refs(&callback._base, obj->obj);
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
bool IncrGc(FbleHeap* heap)
{
  // Free a couple of objects on the free list.
  // If we free less than one object, we won't be able to keep up with
  // allocations and the heap will grow unbounded. If we free exactly one
  // object here, we won't be able to get ahead of allocations; the heap will
  // never shrink. We can shrink the heap if we free just a little more than
  // one object here. Two seems like the easiest approximation to that.
  for (size_t i = 0; i < 2 && heap->free->next != heap->free; ++i) {
    Obj* obj = heap->free->next;
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    heap->on_free(heap, obj->obj);
    FbleFree(obj);
  }

  // Traverse some objects on the heap.
  // The more objects we traverse, the more time we spend doing GC but the
  // less time a garbage object spends waiting to be collected, which means
  // the less memory overhead. The less time we traverse, the less time doing
  // GC, but the greater the memory overhead. I think, technically, as long as
  // we traverse at least one object occasionally, we should be able to keep
  // up with allocations.
  //
  // Here we traverse exactly one object. We give priority to pending objects
  // in the hopes that roots will be dropped and can be collected this GC
  // cycle if we haven't traversed them yet.
  if (heap->pending->next != heap->pending) {
    Obj* obj = heap->pending->next;
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->next = heap->to->next;
    obj->prev = heap->to;
    obj->space = heap->to_space;
    heap->to->next->prev = obj;
    heap->to->next = obj;
    MarkRefs(heap, obj);
  } else if (heap->roots_from->next != heap->roots_from) {
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

// FbleNewHeap -- see documentation in heap.h
FbleHeap* FbleNewHeap(
    void (*refs)(FbleHeapCallback*, void*),
    void (*on_free)(FbleHeap*, void*))
{
  FbleHeap* heap = FbleAlloc(FbleHeap);
  heap->refs = refs;
  heap->on_free = on_free;

  heap->to = FbleAlloc(Obj);
  heap->from = FbleAlloc(Obj);
  heap->pending = FbleAlloc(Obj);
  heap->roots_to = FbleAlloc(Obj);
  heap->roots_from = FbleAlloc(Obj);
  heap->free = FbleAlloc(Obj);

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
  return heap;
}

// FbleFreeHeap -- see documentation in heap.h
void FbleFreeHeap(FbleHeap* heap)
{
  FbleHeapFullGc(heap);

  FbleFree(heap->to);
  FbleFree(heap->from);
  FbleFree(heap->pending);
  FbleFree(heap->roots_to);
  FbleFree(heap->roots_from);
  FbleFree(heap->free);
  FbleFree(heap);
}

// FbleNewHeapObject -- see documentation in heap.h
void* FbleNewHeapObject(FbleHeap* heap, size_t size)
{
  IncrGc(heap);

  // Objects are allocated as roots.
  Obj* obj = FbleAllocExtra(Obj, size);
  obj->next = heap->roots_to->next;
  obj->prev = heap->roots_to;
  obj->space = heap->to_space;
  obj->refcount = 1;
  heap->roots_to->next->prev = obj;
  heap->roots_to->next = obj;
  return obj->obj;
}

// FbleRetainHeapObject -- see documentation in heap.h
void FbleRetainHeapObject(FbleHeap* heap, void* obj_)
{
  Obj* obj = ToObj(obj_);
  if (obj->refcount++ == 0) {
    // This is a new root.
    if (obj->space == heap->to_space) {
      // We have already traversed the object, so move it to roots_to.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->roots_to->next;
      obj->prev = heap->roots_to;
      heap->roots_to->next->prev = obj;
      heap->roots_to->next = obj;
    } else {
      // We haven't traversed the object yet, so move it to roots_from.
      // If the object was PENDING, we keep it marked as PENDING.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->roots_from->next;
      obj->prev = heap->roots_from;
      heap->roots_from->next->prev = obj;
      heap->roots_from->next = obj;
    }
  }
}

// FbleReleaseHeapObject -- see documentation in heap.h
void FbleReleaseHeapObject(FbleHeap* heap, void* obj_)
{
  Obj* obj = ToObj(obj_);
  assert(obj->refcount > 0);
  if (--obj->refcount == 0) {
    // This is no longer a root.
    if (obj->space == heap->to_space) {
      // We have already traversed this object. Move it to the "to" space.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->to->next;
      obj->prev = heap->to;
      heap->to->next->prev = obj;
      heap->to->next = obj;
    } else if (obj->space == PENDING) {
      // We haven't traversed the object yet, but it's reachable from some
      // other root that we have already traversed. Move it to pending.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->pending->next;
      obj->prev = heap->pending;
      heap->pending->next->prev = obj;
      heap->pending->next = obj;
    } else {
      // We haven't traversed the object yet, but it isn't reachable from
      // anything else we have traversed so far. Move it to "from" space.
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      obj->next = heap->from->next;
      obj->prev = heap->from;
      heap->from->next->prev = obj;
      heap->from->next = obj;
    }
  }
}

// FbleHeapObjectAddRef -- see documentation in heap.h
void FbleHeapObjectAddRef(FbleHeap* heap, void* src_, void* dst_)
{
  assert(dst_ != NULL);

  Obj* src = ToObj(src_);
  Obj* dst = ToObj(dst_);

  // Mark dst as pending if we have already traversed the src, but haven't yet
  // seen the dst this round of gc.
  if (src->space == heap->to_space && dst->space == heap->from_space) {
    // If dst is a root, we just updating its PENDING state. Otherwise we need
    // to move it to the list of pending objects.
    dst->space = PENDING;
    if (dst->refcount == 0) {
      dst->prev->next = dst->next;
      dst->next->prev = dst->prev;
      dst->next = heap->pending->next;
      dst->prev = heap->pending;
      heap->pending->next->prev = dst;
      heap->pending->next = dst;
    }
  }
}

// FbleHeapFullGc -- see documentation in heap.h
void FbleHeapFullGc(FbleHeap* heap)
{
  // Finish the GC in progress.
  while (!IncrGc(heap));

  // Do repeated rounds of full GC for as long as we are able to free any
  // objects. It's not enough to run a single additional round of full GC in
  // case any of the objects freed have on_free functions that release other
  // objects.
  bool done;
  do {
    // Do a round of full GC to catch any references that were just removed.
    while (!IncrGc(heap));

    // Clean up all the free objects.
    done = heap->free->next == heap->free;
    while (heap->free->next != heap->free) {
      Obj* obj = heap->free->next;
      obj->prev->next = obj->next;
      obj->next->prev = obj->prev;
      heap->on_free(heap, obj->obj);
      FbleFree(obj);
    }
  } while (!done);
}
