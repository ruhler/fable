/**
 * @file type-heap.c
 *  Mark-sweep based garbage collector for types.
 */

#include "type.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>

#include "type-heap.h"

/**
 * @struct[ObjList] A list of objects.
 *  @field[ObjList*][prev] Previous entry in doubly linked list of objects.
 *  @field[ObjList*][next] Next entry in doubly linked list of objects.
 */
typedef struct ObjList {
  struct ObjList* prev;
  struct ObjList* next;
} ObjList;

/**
 * @struct[Gen] A generation of allocated objects.
 *  @field[size_t][id] The id of the generation.
 *  @field[ObjList][roots] The list of root objects in this generation.
 *  @field[ObjList][non_roots] The list of non-root objects in this generation.
 *  @field[Gen*][tail] Singly linked list of generations.
 */
typedef struct Gen {
  size_t id;
  ObjList roots;
  ObjList non_roots;
  struct Gen* tail;
} Gen;

// Special generation IDs.
#define GC_ID ((size_t)-4)
#define MARK_ID ((size_t)-3)
#define SAVE_ID ((size_t)-2)
#define NEW_ID ((size_t)-1)

/**
 * @struct[Obj] An object allocated on the heap.
 *  @field[ObjList][list] List of objects this object belongs to.
 *  @field[Gen*][gen] The generation the object currently belongs to.
 *  @field[size_t][refcount]
 *   The number of external (non-cyclic) references to this object.  Objects
 *   with refcount greater than 0 are roots.
 *  @field[char*][data] The object pointer visible to the user.
 */
typedef struct {
  ObjList list;
  Gen* gen;
  size_t refcount;
  char data[];
} Obj;

/**
 * @func[ToObj] Gets the Obj* corresponding to a FbleType* obj pointer.
 *  Defined as a macro instead of a function to improve performance.
 *
 *  @arg[FbleType*][type] The FbleType* obj pointer.
 *
 *  @returns[Obj*]
 *   The corresponding Obj* pointer.
 *
 *  @sideeffects
 *   None.
 */
#define ToObj(type) (((Obj*)type)-1)

/**
 * @struct[FbleTypeHeap] GC managed heap of types.
 *  See documentation in heap.h.
 *
 *  @field[Gen*][old]
 *   List of older generations of objects.
 *
 *   These generations will not be traversed this GC cycle.  In order from
 *   youngest generation (largest id) to oldest generation (smallest id).
 *  
 *   Invariants (unless it has been recorded in the 'next' field it needs to
 *   be fixed next GC):
 *
 *   @i Older generations do not reference younger generations.
 *   @item
 *    All objects in a generation are reachable from the first root object
 *    listed in the generation.
 *
 *  @field[Gen*][mark]
 *   Temporary generation for objects that have been marked but not yet
 *   traversed. id is MARK_ID.
 *
 *  @field[Gen*][gc]
 *   The generations of objects being traversed this GC cycle.  This
 *   represents the union of all the generations being traversed this GC
 *   cycle.
 *  
 *   For convenience, we move all objects to the roots and non_roots fields of
 *   the first generation on the list.
 *  
 *   The rest of the generations on the list we keep around to maintain the
 *   map from Gen* to id and to be able to clean up the generation after the
 *   current GC traversal has completed.
 *  
 *   All generations in this list have id GC_ID.
 *
 *  @field[Gen*][save]
 *   Objects that have been marked to be saved this GC cycle, for example,
 *   because of some reference taken from old or new generations.
 *  
 *   The id is SAVE_ID.
 *
 *  @field[Gen*][new]
 *   The generation where newly allocated objects will be placed.  Not
 *   traversed in the current GC cycle.
 *  
 *   The id is NEW_ID.
 *
 *  @field[Gen*][next]
 *   The oldest generation we plan to traverse in the next GC cycle.
 *   Borrowed, not owned. This could point to 'new' or one of the 'old'
 *   generations.
 *
 *  @field[ObjList][free]
 *   List of free objects.
 *  
 *   These objects have been determined to be unreachable. They have not yet
 *   had their on_free callbacks called.
 *
 *  @field[FbleModulePath*][context]
 *   The module currently being compiled. Stored with the type heap for
 *   convenience only, it really has nothing to do with allocating objects.
 */
struct FbleTypeHeap {
  Gen* old;
  Gen* mark;
  Gen* gc;
  Gen* save;
  Gen* new;
  Gen* next;
  ObjList free;
  FbleModulePath* context;
};

static void MoveToFront(ObjList* dest, Obj* obj);
static void MoveToBack(ObjList* dest, Obj* obj);
static void MoveAllToFront(ObjList* dest, ObjList* source);

static Gen* NewGen(size_t id);
static bool GenIsEmpty(Gen* gen);

static bool IncrGc(FbleTypeHeap* heap);
static void FullGc(FbleTypeHeap* heap);

/**
 * @func[MoveToFront] Move an object to the front of the given list.
 *  @arg[ObjList*][dest] Where to move the object to.
 *  @arg[Obj*][obj] The object to move.
 *  @sideeffects Moves the object to the front of the given list.
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
 *  @sideeffects Moves the object to the back of the given list.
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
 *  @sideeffects Moves all objects from @a[source] to @a[dest].
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
 *  @arg[size_t][id] The id to assign to the new generation.
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
 * @func[GenIsEmpty] Returns true if the generation is empty.
 *  @arg[Gen*][gen] The generation to check.
 *  @sideeffects None.
 */
static bool GenIsEmpty(Gen* gen)
{
  return (gen->roots.next == &gen->roots)
    && (gen->non_roots.next == &gen->non_roots);
}

/**
 * @func[IncrGc] Does an incremental amount of GC work.
 *  @arg[FbleTypeHeap*][heap] The heap to do GC on.
 *
 *  @returns[bool]
 *   true if this completed a round of GC. False otherwise.
 *
 *  @sideeffects
 *   Does some GC work, which may involve moving objects around, traversing
 *   objects, freeing objects, etc.
 */
static bool IncrGc(FbleTypeHeap* heap)
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
    FbleTypeOnFree((FbleType*)obj->data);
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

  // Mark Non-Root -> Old Non-Root
  if (heap->mark->non_roots.next != &heap->mark->non_roots) {
    Obj* obj = (Obj*)heap->mark->non_roots.next;
    FbleTypeRefs(heap, (FbleType*)obj->data);
    obj->gen = heap->old;
    MoveToBack(&heap->old->non_roots, obj);
    return false;
  }

  // Mark Root -> To Root
  if (heap->mark->roots.next != &heap->mark->roots) {
    Obj* obj = (Obj*)heap->mark->roots.next;
    FbleTypeRefs(heap, (FbleType*)obj->data);
    obj->gen = heap->old;
    MoveToBack(&heap->old->roots, obj);
    return false;
  }

  if (!GenIsEmpty(heap->old)) {
    // We have finished traversing all objects reachable from the previous
    // root. Start the next 'old' generation.
    Gen* old = NewGen(heap->old->id + 1);
    old->tail = heap->old;
    heap->old = old;
  }

  // GC Root -> Old
  if (heap->gc->roots.next != &heap->gc->roots) {
    Obj* obj = (Obj*)heap->gc->roots.next;
    obj->gen = heap->mark;
    FbleTypeRefs(heap, (FbleType*)obj->data);
    obj->gen = heap->old;
    MoveToFront(&heap->old->roots, obj);
    return false;
  }

  // Save Root -> Old
  if (heap->save->roots.next != &heap->save->roots) {
    Obj* obj = (Obj*)heap->save->roots.next;
    obj->gen = heap->mark;
    FbleTypeRefs(heap, (FbleType*)obj->data);
    obj->gen = heap->old;
    MoveToFront(&heap->old->roots, obj);
    return false;
  }

  // Save NonRoot -> New
  if (heap->save->non_roots.next != &heap->save->non_roots) {
    Obj* obj = (Obj*)heap->save->non_roots.next;
    FbleTypeRefs(heap, (FbleType*)obj->data);
    obj->gen = heap->new;
    MoveToFront(&heap->new->non_roots, obj);
    return false;
  }

  // We are done with this GC cycle. Clean up and prepare for a new GC cycle.

  // Gc NonRoots -> Free
  // These are unreachable objects found by the GC cycle.
  MoveAllToFront(&heap->free, &heap->gc->non_roots);

  // Clean up the now empty 'gc' generations.
  for (Gen* gc = heap->gc; gc != NULL; gc = heap->gc) {
    heap->gc = gc->tail;
    FbleFree(gc);
  }
  
  // Set up the next 'gc' generation.
  // It will include all generations from 'next' to 'new'.
  heap->new->tail = heap->old;
  heap->gc = heap->new;
  heap->gc->id = GC_ID;
  heap->old = heap->next->tail;
  heap->next->tail = NULL;
  for (Gen* gen = heap->gc->tail; gen != NULL; gen = gen->tail) {
    gen->id = GC_ID;
    MoveAllToFront(&heap->gc->roots, &gen->roots);
    MoveAllToFront(&heap->gc->non_roots, &gen->non_roots);
  }

  if (heap->old == NULL) {
    heap->old = NewGen(0);
  }
  heap->new = NewGen(NEW_ID);
  heap->next = heap->new;

  // GC finished. Yay!
  return true;
}

// See documentation in type.h.
FbleTypeHeap* FbleNewTypeHeap()
{
  FbleTypeHeap* heap = FbleAlloc(FbleTypeHeap);
  heap->context = NULL;

  heap->old = NewGen(0);
  heap->mark = NewGen(MARK_ID);
  heap->gc = NewGen(GC_ID);
  heap->save = NewGen(SAVE_ID);
  heap->new = NewGen(NEW_ID);
  heap->next = heap->new;

  heap->free.prev = &heap->free;
  heap->free.next = &heap->free;

  return heap;
}

// See documentation in type.h.
void FbleFreeTypeHeap(FbleTypeHeap* heap)
{
  FullGc(heap);

  for (Gen* gen = heap->old; gen != NULL; gen = heap->old) {
    heap->old = gen->tail;
    FbleFree(gen);
  }

  FbleFree(heap->mark);

  for (Gen* gen = heap->gc; gen != NULL; gen = heap->gc) {
    heap->gc = gen->tail;
    FbleFree(gen);
  }

  FbleFree(heap->save);
  FbleFree(heap->new);

  if (heap->context != NULL) {
    FbleFreeModulePath(heap->context);
  }

  FbleFree(heap);
}

// See documentation in type-heap.h.
FbleType* FbleAllocType(FbleTypeHeap* heap, size_t size)
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
  return (FbleType*)obj->data;
}

// See documentation in type.h.
FbleType* FbleRetainType(FbleTypeHeap* heap, FbleType* type)
{
  if (type == NULL) {
    return NULL;
  }

  Obj* obj = ToObj(type);
  if (obj->refcount++ == 0) {
    // Non-Root -> Root
    if (obj->gen->id == GC_ID) {
      // The object is in one of the 'gc' generations, make sure it stays in
      // the canonical heap->gc generation.
      obj->gen = heap->gc;
    }
    MoveToBack(&obj->gen->roots, obj);
  }

  return type;
}

// See documentation in heap.h.
void FbleReleaseType(FbleTypeHeap* heap, FbleType* type)
{
  if (type == NULL) {
    return;
  }

  Obj* obj = ToObj(type);
  assert(obj->refcount > 0);
  if (--obj->refcount == 0) {
    // Root -> Non-Root
    if (obj->gen->id == GC_ID) {
      // The object is in one of the 'gc' generations, make sure it stays in
      // the canonical heap->gc generation.
      obj->gen = heap->gc;
    }
    
    if (obj->gen->id < heap->next->id
        && obj->gen->id <= heap->old->id
        && (&obj->list == obj->gen->roots.next)) {
      // This object is the primary root of an old generation.
      // We need to GC this object's generation next cycle, because it may be
      // unreachable now.
      heap->next = obj->gen;
    }
    MoveToBack(&obj->gen->non_roots, obj);
  }
}

// See documentation in heap.h.
void FbleTypeAddRef(FbleTypeHeap* heap, FbleType* src_, FbleType* dst_)
{
  assert(dst_ != NULL);

  Obj* src = ToObj(src_);
  Obj* dst = ToObj(dst_);

  if (src->gen->id <= heap->old->id
      && src->gen->id < dst->gen->id
      && src->gen->id < heap->next->id) {
    // An older generation object takes a reference to something newer. We
    // need to include that older generation in the next GC traversal, to make
    // sure we see all references to the dst object.
    heap->next = src->gen;
  } else if (src->gen->id == MARK_ID
      && dst->gen->id == NEW_ID
      && src->gen->id < heap->next->id) {
    // Mark references New. We need to include the old generation where the
    // marked object ends up in the next GC traversal, to make sure we see all
    // references to the dst object.
    heap->next = heap->old;
  }

  Gen* moveto = NULL;
  if (dst->gen->id == GC_ID) {
    if (src->gen->id == MARK_ID) {
      // Mark references GC
      moveto = heap->mark;
    } else if (src->gen->id != GC_ID) {
      // Old/Save/New references GC
      moveto = heap->save;
    }
  } else if (src->gen->id == MARK_ID && dst->gen->id == SAVE_ID) {
    // Mark references Save
    moveto = heap->mark;
  }

  if (moveto != NULL) {
    dst->gen = moveto;
    if (dst->refcount == 0) {
      MoveToBack(&moveto->non_roots, dst);
    } else {
      MoveToBack(&moveto->roots, dst);
    }
  }
}

/**
 * @func[FullGc] Does a full GC.
 *  Causes the garbage collector to perform a full garbage collection,
 *  collecting all objects that are currently unreachable.
 *
 *  Full GC can be a very expensive operation. This method is primarily
 *  intended to be used to help in testing and debugging of memory use.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to perform GC on.
 *
 *  @sideeffects
 *   Causes all currently unreachable objects on the heap to be freed.
 */
void FullGc(FbleTypeHeap* heap)
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
      FbleTypeOnFree((FbleType*)obj->data);
      FbleFree(obj);
    }
  } while (!done);
}

void FbleTypeHeapSetContext(FbleTypeHeap* heap, FbleModulePath* context)
{
  if (heap->context != NULL) {
    FbleFreeModulePath(heap->context);
  }
  heap->context = FbleCopyModulePath(context);
}

FbleModulePath* FbleTypeHeapGetContext(FbleTypeHeap* heap)
{
  return heap->context;
}
