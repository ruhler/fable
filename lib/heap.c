/**
 * @file heap.c
 *  Mark-sweep based garbage collector.
 */

#include "heap.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>

/**
 * A list of objects.
 */
typedef struct ObjList {
  struct ObjList* prev;   /**< Previous entry in doubly linked list of objects. */
  struct ObjList* next;   /**< Next entry in doubly linked list of objects. */
} ObjList;

/**
 * A generation of allocated objects.
 */
typedef struct Gen {
  // Invariant: X.id < Y.id implies no objects in generation X reference any
  // objects in generation Y.
  size_t id;

  // The list of root objects in this generation.
  // Invariant: All objects in this generation are reachable from the first
  // root in the list of roots.
  ObjList roots;

  // The list of non-root objects in this generation.
  ObjList non_roots;

  // Singly linked list of generations.
  struct Gen* tail;
} Gen;

#define FROM_ID ((size_t)-2)
#define NEW_ID ((size_t)-1)

/**
 * An object allocated on the heap.
 */
typedef struct {
  ObjList list;       /**< List of objects this object belongs to. */
  Gen* gen;           /**< The generation the object currently belongs to. */

  /**
   * The number of external (non-cyclic) references to this object.
   * Objects with refcount greater than 0 are roots.
   */
  size_t refcount;

  char obj[];         /**< The object pointer visible to the user. */
} Obj;

/**
 * Gets the Obj* corresponding to a void* obj pointer.
 *
 * @param obj  The void* obj pointer.
 *
 * @returns
 *   The corresponding Obj* pointer.
 *
 * @sideeffects
 *   None.
 *
 * Notes: defined as a macro instead of a function to improve performance.
 */
#define ToObj(obj) (((Obj*)obj)-1)

/**
 * GC managed heap of objects.
 *
 * See documentation in heap.h.
 */
struct FbleHeap {
  /** The refs callback to traverse reference from an object. */
  void (*refs)(FbleHeap* heap, void* obj);

  /** The on_free callback to indicate an object has been freed. */
  void (*on_free)(FbleHeap* heap, void* obj);

  /**
   * List of older generation of objects.
   * These generations will not be traversed this GC cycle.
   * In order from youngest generation (largest id) to oldest generation
   * (smallest id).
   *
   * Generation 0 is an empty initial generation to ensure this is non-NULL.
   */
  Gen* older;

  /**
   * Temporary generation for objects that belong to the currently forming
   * 'to' generation, but not yet been traversed themselves. The id is to->id.
   */
  Gen* pending;

  /**
   * The generation of objects we are currently forming.
   * The id is (older->id + 1).
   */
  Gen* to;

  /**
   * The generations of objects being traversed this GC cycle.
   * This represents the union of all the generations being traversed this GC
   * cycle.
   *
   * For convenience, we move all objects to the roots and non_roots
   * fields of the first generation on the list.
   *
   * The rest of the generations on the list we keep around to maintain the
   * map from Gen* to id and to be able to clean up the generation after the
   * current GC traversal has completed.
   *
   * All generations in this list have id (-2).
   */
  Gen* from;

  /**
   * Objects traversed this GC cycle that are reachable from 'new' objects.
   *
   * This is to avoid collecting these objects at the end of the GC cycle if
   * they don't land in any of the new 'to' generations.
   *
   * The id is (-2).
   */
  Gen* from_new;

  /**
   * The generation where newly allocated objects will be placed.
   *
   * The id is (-1).
   */
  Gen* new;

  // The oldest generation we plan to traverse in the next GC cycle.
  // Borrowed, not owned. This could point to 'new', 'to', or one of the
  // 'older' generations.
  Gen* next;

  /**
   * List of free objects.
   *
   * These objects have been determined to be unreachable. They have not yet
   * had their on_free callbacks called.
   */
  ObjList free;
};

static void MoveToFront(ObjList* dest, Obj* obj);
static void MoveToBack(ObjList* dest, Obj* obj);
static void MoveAllToFront(ObjList* dest, ObjList* source);
static Gen* NewGen(size_t id);
static bool IncrGc(FbleHeap* heap);

/**
 * @func[MoveToFront] Move an object to the front of the given list.
 *  @arg[ObjList*][dest] Where to move the object to.
 *  @arg[Obj*][obj] The object to move.
 *  @sideffects Moves the object to the front of the given list.
 */
static void MoveToFront(ObjList* dest, Obj* obj)
{
  obj->list.prev->next = obj->list.next;
  obj->list.next->prev = obj->list.prev;
  obj->list.next = dest->next;
  obj->list.prev = dest;
  dest->next->prev = &obj->list;
  dest->next = &obj->list;
}

/**
 * @func[MoveToBack] Move an object to the back of the given list.
 *  @arg[ObjList*][dest] Where to move the object to.
 *  @arg[Obj*][obj] The object to move.
 *  @sideffects Moves the object to the back of the given list.
 */
static void MoveToBack(ObjList* dest, Obj* obj)
{
  obj->list.prev->next = obj->list.next;
  obj->list.next->prev = obj->list.prev;
  obj->list.prev = dest->prev;
  obj->list.next = dest;
  dest->prev->next = &obj->list;
  dest->prev = &obj->list;
}

/**
 * @func[MoveAllToFront] Move all objects in source to the front of dest.
 *  @arg[ObjList*][dest] Where to move the objects to.
 *  @arg[ObjList*][source] The list of objects to move.
 *  @sideffects Moves all objects from @a[source] to @a[dest].
 */
static void MoveAllToFront(ObjList* dest, ObjList* source)
{
  if (source->next != source) {
    dest->next->prev = source->prev;
    source->prev->next = dest->next;
    dest->next = source->next;
    dest->next->prev = dest;
    source->next = source;
    source->prev = source;
  }
}

/**
 * @func[NewGen] Allocates a new generation.
 *  @arg[size_t][id] The id to assing to the new generation.
 *  @returns[Gen*] Newly allocated, empty, initialized generation.
 *  @sideeffects
 *   Allocates a generation that should be freed with FbleFree when no longer
 *   needed.
 */
static Gen* NewGen(size_t id)
{
  Gen* gen = FbleAlloc(Gen);
  gen->id = id;
  gen->roots.next = &gen->roots;
  gen->roots.prev = &gen->roots;
  gen->non_roots.next = &gen->non_roots;
  gen->non_roots.prev = &gen->non_roots;
  gen->tail = NULL;
  return gen;
}

/**
 * @func[IncrGc] Does an incremental amount of GC work.
 *  @arg[FbleHeap*][heap] The heap to do GC on.
 *
 *  @returns[bool]
 *   true if this completed a round of GC. False otherwise.
 *
 *  @sideeffects
 *   Does some GC work, which may involve moving objects around, traversing
 *   objects, freeing objects, etc.
 */
static bool IncrGc(FbleHeap* heap)
{
  // Free a couple of objects on the free list.
  // If we free less than one object, we won't be able to keep up with
  // allocations and the heap will grow unbounded. If we free exactly one
  // object here, we won't be able to get ahead of allocations; the heap will
  // never shrink. We can shrink the heap if we free just a little more than
  // one object here. Two seems like the easiest approximation to that.
  for (size_t i = 0; i < 2 && heap->free.next != &heap->free; ++i) {
    Obj* obj = (Obj*)heap->free.next;
    obj->list.prev->next = obj->list.next;
    obj->list.next->prev = obj->list.prev;
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
  // Here we traverse exactly one object.

  // Pending Non-Root -> To Non-Root
  if (heap->pending->non_roots.next != &heap->pending->non_roots) {
    Obj* obj = (Obj*)heap->pending->non_roots.next;
    obj->gen = heap->to;
    MoveToBack(&heap->to->non_roots, obj);
    heap->refs(heap, obj->obj);
    return false;
  }

  // Pending Root -> To Root
  if (heap->pending->roots.next != &heap->pending->roots) {
    Obj* obj = (Obj*)heap->pending->roots.next;
    obj->gen = heap->to;
    MoveToBack(&heap->to->roots, obj);
    heap->refs(heap, obj->obj);
    return false;
  }

  if (heap->to->roots.next != &heap->to->roots
      || heap->to->non_roots.next != &heap->to->non_roots) {
    // We have finished forming the current 'to' generation. Install the
    // generation and start a new one.
    heap->to->tail = heap->older;
    heap->older = heap->to;
    heap->to = NewGen(heap->older->id + 1);
    heap->pending->id = heap->to->id;
  }

  // From Root -> To Root
  if (heap->from->roots.next != &heap->from->roots) {
    Obj* obj = (Obj*)heap->from->roots.next;
    obj->gen = heap->to;
    MoveToFront(&heap->to->roots, obj);
    heap->refs(heap, obj->obj);
    return false;
  }

  // FromNew Root -> To Root
  if (heap->from_new->roots.next != &heap->from_new->roots) {
    Obj* obj = (Obj*)heap->from_new->roots.next;
    obj->gen = heap->to;
    MoveToFront(&heap->to->roots, obj);
    heap->refs(heap, obj->obj);
    return false;
  }

  // We are done with this GC cycle. Clean up and prepare for a new GC cycle.

  // From -> Free
  // These are unreachable objects found by the GC cycle.
  MoveAllToFront(&heap->free, &heap->from->non_roots);

  // FromNew -> New
  // Objects only reachable from new objects. May as well be new.
  MoveAllToFront(&heap->new->roots, &heap->from_new->roots);
  MoveAllToFront(&heap->new->non_roots, &heap->from_new->non_roots);

  // Clean up the now empty 'from' generations.
  for (Gen* from = heap->from; from != NULL; from = heap->from) {
    heap->from = from->tail;
    FbleFree(from);
  }

  // Set up the next 'from' generation.
  // It will include all generations from 'next' to 'new'.
  heap->from = heap->new;
  heap->from->id = FROM_ID;
  while (heap->next != heap->from) {
    Gen* gen = heap->older;
    heap->older = heap->older->tail;

    gen->id = FROM_ID;
    MoveAllToFront(&gen->roots, &heap->from->roots);
    MoveAllToFront(&gen->non_roots, &heap->from->non_roots);
    gen->tail = heap->from;

    heap->from = gen;
  }

  // Set up the next 'to' generation.
  heap->to->id = heap->older->id + 1;
  heap->pending->id = heap->to->id;

  // Set up the next 'new' generation.
  heap->new = NewGen(NEW_ID);
  heap->next = heap->new;

  // GC finished. Yay!
  return true;
}

// See documentation in heap.h.
FbleHeap* FbleNewHeap(
    void (*refs)(FbleHeap* heap, void* obj),
    void (*on_free)(FbleHeap* heap, void* obj))
{
  FbleHeap* heap = FbleAlloc(FbleHeap);
  heap->refs = refs;
  heap->on_free = on_free;

  heap->older = NewGen(0);
  heap->pending = NewGen(1);
  heap->to = NewGen(1);
  heap->from = NewGen(FROM_ID);
  heap->from_new = NewGen(FROM_ID);
  heap->new = NewGen(NEW_ID);
  heap->next = heap->new;

  heap->free.prev = &heap->free;
  heap->free.next = &heap->free;

  return heap;
}

// See documentation in heap.h.
void FbleFreeHeap(FbleHeap* heap)
{
  FbleHeapFullGc(heap);

  for (Gen* gen = heap->older; gen != NULL; gen = heap->older) {
    heap->older = gen->tail;
    FbleFree(gen);
  }

  FbleFree(heap->pending);
  FbleFree(heap->to);

  for (Gen* gen = heap->from; gen != NULL; gen = heap->from) {
    heap->from = gen->tail;
    FbleFree(gen);
  }

  FbleFree(heap->from_new);
  FbleFree(heap->new);

  FbleFree(heap);
}

// See documentation in heap.h.
void* FbleNewHeapObject(FbleHeap* heap, size_t size)
{
  IncrGc(heap);

  // Objects are allocated as New Root.
  Obj* obj = FbleAllocExtra(Obj, size);
  obj->list.next = heap->new->roots.next;
  obj->list.prev = &heap->new->roots;
  obj->gen = heap->new;
  obj->refcount = 1;
  heap->new->roots.next->prev = &obj->list;
  heap->new->roots.next = &obj->list;
  return obj->obj;
}

// See documentation in heap.h.
void FbleRetainHeapObject(FbleHeap* heap, void* obj_)
{
  Obj* obj = ToObj(obj_);
  if (obj->refcount++ == 0) {
    // Non-Root -> Root
    if (obj->gen->id == heap->from->id) {
      // The object is in one of the 'from' generations, make sure it stays in
      // the canonical heap->from generation.
      obj->gen = heap->from;
    }
    MoveToBack(&obj->gen->roots, obj);
  }
}

// See documentation in heap.h.
void FbleReleaseHeapObject(FbleHeap* heap, void* obj_)
{
  Obj* obj = ToObj(obj_);
  assert(obj->refcount > 0);
  if (--obj->refcount == 0) {
    // Root -> Non-Root
    if (obj->gen->id == heap->from->id) {
      // The object is in one of the 'from' generations, make sure it stays in
      // the canonical heap->from generation.
      obj->gen = heap->from;
    } else if (obj->gen->id < heap->next->id
        && (&obj->list == obj->gen->roots.next)) {
      // We need to GC this object's generation next cycle, because this
      // object may be unreachable now.
      if (obj->gen->id < heap->to->id) {
        // older object
        heap->next = obj->gen;
      } else {
        // pending/to object.
        heap->next = heap->to;
      }
    }
    MoveToBack(&obj->gen->non_roots, obj);
  }
}

// See documentation in heap.h.
void FbleHeapObjectAddRef(FbleHeap* heap, void* src_, void* dst_)
{
  assert(dst_ != NULL);

  Obj* src = ToObj(src_);
  Obj* dst = ToObj(dst_);

  if (src->gen->id < heap->to->id
      && src->gen->id < dst->gen->id
      && src->gen->id < heap->next->id) {
    // An older generation object takes a reference to something newer. We
    // need to include that older generation in the next GC traversal, to make
    // sure we see all references to the dst object.
    heap->next = src->gen;
  } else if (dst->gen->id == NEW_ID
      && src->gen->id != NEW_ID
      && heap->to->id < heap->next->id) {
    // A from/pending/to object takes a reference to a new object. We need to
    // include the current 'to' generation in the next GC traversal to make
    // sure we see all references to the new object.
    heap->next = heap->to;
  }

  if (dst->gen->id == FROM_ID && src->gen->id != FROM_ID) {
    // An old/pending/to/new object references a 'from' option.
    dst->gen = heap->pending;
    if (dst->refcount == 0) {
      MoveToBack(&heap->pending->non_roots, dst);
    } else {
      MoveToBack(&heap->pending->roots, dst);
    }
  }
}

// See documentation in heap.h
void FbleHeapRef(FbleHeap* heap, void* obj_)
{
  Obj* obj = ToObj(obj_);

  if (obj->gen->id == FROM_ID) {
    // From/FromNew -> Pending
    obj->gen = heap->pending;
    if (obj->refcount == 0) {
      MoveToBack(&heap->pending->non_roots, obj);
    } else {
      MoveToBack(&heap->pending->roots, obj);
    }
  }
}

// See documentation in heap.h.
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
    done = heap->free.next == &heap->free;
    while (heap->free.next != &heap->free) {
      Obj* obj = (Obj*)heap->free.next;
      obj->list.prev->next = obj->list.next;
      obj->list.next->prev = obj->list.prev;
      heap->on_free(heap, obj->obj);
      FbleFree(obj);
    }
  } while (!done);
}
