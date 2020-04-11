// rc-heap.c --
//   This file implements a reference counting based heap.

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for NULL

#include "fble-alloc.h"
#include "fble-vector.h"
#include "fble-heap.h"

// Special object id that we gaurentee no valid object will have.
// This is be used as a sentinel value while breaking cycles that have no
// external references.
#define NULL_OBJ_ID 0

// Cycle --
//   Represents a set of objects that form a cycle.
//
// Fields:
//   refcount - the total number of references from objects outside this cycle
//              to objects inside this cycle.
//   size - the number of objects inside this cycle.
//
// TODO: This does not support nested cycles, which could cause us to hold on
// to objects longer than necessary after deleted references between nested
// cycles.
typedef struct {
  size_t refcount;
  size_t size;
} Cycle;

// Obj --
//   An object allocated on the heap.
//
// Fields:
//   id - A unique identifier for the object. Ids are assigned in increasing
//        order of object allocation.
//   refcount - The number of references to this object.
//   cycle - A pointer to the cycle this object belongs to, or NULL if this
//           object does not belong to a cycle.
//   obj - the obj pointer visible to the user.
typedef struct {
  size_t id;
  size_t refcount;
  Cycle* cycle;
  char obj[];
} Obj;

// ObjV -- A vector of Obj
typedef struct {
  size_t size;
  Obj** xs;
} ObjV;

static Obj* ToObj(void* obj_);

// Heap --
//   The FbleHeap for the reference counting heap.
//
// Fields:
//   next_id - the id to use for the next object that is allocated.
typedef struct {
  FbleHeap _base;
  size_t next_id;
} Heap;

// Map --
//   A hash based mapping from key to index.
typedef struct {
  size_t capacity;
  size_t* xs;
} Map;

#define INITIAL_MAP_CAPACITY 3

// Set --
//   A set of references with map from index to reference and reverse hashmap
//   from reference to index.
//
// Fields:
//   objs - the set of objects in the map.
//   map - a hash map from object to its index in objs.
#define MAP_EMPTY ((size_t) (-1))
typedef struct {
  ObjV objs;
  Map map;
} Set;

static void MapInit(FbleArena* arena, Map* map, size_t capacity);
static size_t* MapIndex(Obj** objs, Map* map, Obj* obj);
static size_t Insert(FbleArena* arena, Set* set, Obj* obj);
static bool Contains(Set* set, Obj* obj);

// ObjStack -- 
//   A stack of objects implemented as a singly linked list.
typedef struct ObjStack {
  Obj* obj;
  struct ObjStack* tail;
} ObjStack;

static void CollectRefs(Heap* heap, Obj* obj, ObjV* refs);
static void ReleaseRefs(Heap* heap, Obj* obj, size_t depth, ObjStack** stack);
static void CycleRefAddRefs(Heap* heap, Obj* obj);

static void ReleaseInternal(Heap* heap, Obj* obj, size_t depth, ObjStack** stack);

// MapInit --
//   Initialize a Map.
//
// Inputs:
//   arena - arena to use for allocations.
//   map - a pointer to an uninitialized Map.
//   capacity - the initial capacity to use for the reverse map.
//
// Results: 
//   none
//
// Side effects:
//   Initializes the map. The caller should call FbleFree(arena, map->xs)
//   when done with using the map to release memory allocated for the Map.
static void MapInit(FbleArena* arena, Map* map, size_t capacity)
{
  map->capacity = capacity;
  map->xs = FbleArrayAlloc(arena, size_t, capacity);
  for (size_t i = 0; i < capacity; ++i) {
    map->xs[i] = MAP_EMPTY;
  }
}

// MapIndex --
//   Return a pointer to the map index for the given reference.
//
// Inputs:
//   objs - the array of objects in the set.
//   map - the map.
//   obj - the object to look up.
//
// Returns:
//   A pointer to the map index where the reference is located, or where the
//   reference would be inserted if it isn't already in the map.
//
// Side effects: 
//   None.
static size_t* MapIndex(Obj** objs, Map* map, Obj* obj)
{
  size_t bucket = (size_t)obj % map->capacity;
  size_t index = map->xs[bucket];
  while (index != MAP_EMPTY && objs[index] != obj) {
    bucket = (bucket + 1) % map->capacity;
    index = map->xs[bucket];
  }
  return map->xs + bucket;
}

// Insert --
//   Insert an object into the given set.
//
// Inputs:
//   arena - the arena to use for allocations.
//   set - the set to insert into.
//   obj - the object to insert.
//
// Results:
//   The index of the object in set->objs after insertion, which will be
//   the same index as before insertion if the object was already in the
//   map.
//
// Side effects:
//   Appends the object to objs if it was not already in objs. If the
//   object was already in objs, does nothing.
//   May resize the map, invalidating any pointers into map.
static size_t Insert(FbleArena* arena, Set* set, Obj* obj)
{
  size_t* index = MapIndex(set->objs.xs, &set->map, obj);
  if (*index != MAP_EMPTY) {
    return *index;
  }

  *index = set->objs.size;
  FbleVectorAppend(arena, set->objs, obj);

  if (3 * set->objs.size > set->map.capacity) {
    // It is time to resize the map.
    Map map;
    MapInit(arena, &map, 2 * set->map.capacity - 1);
    for (size_t i = 0; i < set->map.capacity; ++i) {
      size_t ix = set->map.xs[i];
      if (ix != MAP_EMPTY) {
        *MapIndex(set->objs.xs, &map, set->objs.xs[ix]) = ix;
      }
    }
    FbleFree(arena, set->map.xs);
    set->map = map;
  }
  return set->objs.size - 1;
}

// Contains --
//   Check whether a set contains the given object.
//
// Inputs:
//   set - the set to check.
//   obj - the object to check for.
//
// Results: 
//   True if obj is in the set, false otherwise.
//
// Side effects:
//   None.
static bool Contains(Set* map, Obj* obj)
{
  return *MapIndex(map->objs.xs, &map->map, obj) != MAP_EMPTY;
}

// CollectRefs --
//   Collect the references of an object into a vector.
//
// Inputs:
//   heap - the heap
//   obj - the object to get the references of.
//   refs - a preinitialized vector to add the references to.
//
// Results:
//   None.
//
// Side effects:
//   Adds objects referenced by obj to the refs vector.
typedef struct {
  FbleHeapCallback _base;
  FbleArena* arena;
  ObjV* refs;
} CollectRefsCallback;

static void CollectRef(CollectRefsCallback* this, void* obj_)
{
  Obj* obj = ToObj(obj_);
  FbleVectorAppend(this->arena, *this->refs, obj);
}

static void CollectRefs(Heap* heap, Obj* obj, ObjV* refs)
{
    CollectRefsCallback callback = {
      ._base = { .callback = (void(*)(FbleHeapCallback*, void*))&CollectRef },
      .arena = heap->_base.arena,
      .refs = refs
    };
    heap->_base.refs(&callback._base, obj->obj);
}

// ReleaseRefs --
//   Call ReleaseInternal on all objects referenced by the given object.
//
// Inputs:
//   heap - the heap
//   obj - the object whose references to release.
//   depth - the maximum depth of recursion allowed.
//   stack - stack to add objects to incase the recursion depth is reached.
//
// Results:
//   None.
//
// Side effects:
//   Calls ReleaseInternal on all objects referenced by obj.

typedef struct {
  FbleHeapCallback _base;
  Heap* heap;
  size_t depth;
  ObjStack** stack;
} ReleaseRefCallback;

static void ReleaseRef(ReleaseRefCallback* this, void* obj_)
{
  ReleaseInternal(this->heap, ToObj(obj_), this->depth, this->stack);
}

static void ReleaseRefs(Heap* heap, Obj* obj, size_t depth, ObjStack** stack)
{
  ReleaseRefCallback callback = {
    ._base = { .callback = (void(*)(FbleHeapCallback*, void*))&ReleaseRef },
    .heap = heap,
    .depth = depth,
    .stack = stack,
  };
  heap->_base.refs(&callback._base, obj->obj);
}

// CycleRefAddRefs --
//   Increment the refcount of a cycle by the number of objects referenced by
//   an object that belong to the same cycle as the object.
//
//   Used when removing an object from a cycle, at which point all references
//   from the object to other objects in the cycle go from being internal
//   references to external references.
//
// Inputs:
//   heap - the heap
//   obj - the object whose references to traverse.
//
// Results:
//   None.
//
// Side effects:
//   For each object referenced by obj that belongs to the same cycle as obj,
//   increment the refcount on the object's cycle.

typedef struct {
  FbleHeapCallback _base;
  Cycle* cycle;
} CycleRefAddRefCallback;

static void CycleRefAddRef(CycleRefAddRefCallback* this, void* obj_)
{
  Obj* obj = ToObj(obj_);
  if (obj->cycle != NULL && obj->cycle == this->cycle) {
    this->cycle->refcount++;
  }
}

static void CycleRefAddRefs(Heap* heap, Obj* obj)
{
  CycleRefAddRefCallback callback = {
    ._base = { .callback = (void(*)(FbleHeapCallback*, void*))&CycleRefAddRef },
    .cycle = obj->cycle,
  };
  heap->_base.refs(&callback._base, obj->obj);
}

// Release --
//   Release an object recursively.
//
// Inputs:
//   heap - the heap
//   obj - the object to release.
//   depth - maximum recursion depth allowed, to avoid smashing the stack.
//   stack - stack to push subsequent objs on if recursion limit is reached.
//
// Results:
//   none.
//
// Side effect:
//   Recursively releases an object.
static void ReleaseInternal(Heap* heap, Obj* obj, size_t depth, ObjStack** stack)
{
  FbleArena* arena = heap->_base.arena;

  if (depth == 0) {
    ObjStack* nstack = FbleAlloc(arena, ObjStack);
    nstack->obj = obj;
    nstack->tail = *stack;
    *stack = nstack;
    return;
  }

  assert(obj->refcount > 0);
  obj->refcount--;

  if (obj->cycle != NULL) {
    assert(obj->cycle->refcount > 0);
    obj->cycle->refcount--;

    if (obj->refcount == 0) {
      // It is safe to remove this reference from the cycle, because nobody
      // points to this reference anymore.

      // Change outgoing internal references to external references.
      CycleRefAddRefs(heap, obj);

      // Remove the object from the cycle.
      assert(obj->cycle->size > 0);
      obj->cycle->size--;
      if (obj->cycle->size == 0) {
        FbleFree(arena, obj->cycle);
      }
      obj->cycle = NULL;
    }
  }

  if (obj->refcount == 0) {
    // We should already have removed the reference from its cycle.
    assert(obj->cycle == NULL);

    if (obj->id != NULL_OBJ_ID) {
      ReleaseRefs(heap, obj, depth - 1, stack);
    }
    heap->_base.on_free(&heap->_base, obj->obj);
    FbleFree(arena, obj);
  } else if (obj->cycle != NULL && obj->cycle->refcount == 0) {
    // The cycle is unreachable, though there are still other objects in
    // the cycle refering to this object. To break the cycle, we drop all
    // references out of this object. That should be enough to naturally
    // unravel the cycle. Because we can't effect what added will
    // traverse, we mark the object specially by changing its id to
    // NULL_OBJ_ID.
    assert(obj->id != NULL_OBJ_ID);
    obj->id = NULL_OBJ_ID;

    // Increment the cycle refcount for each reference in the cycle to make up
    // for the decrement that will come when we release those references.
    CycleRefAddRefs(heap, obj);

    // Release all child references.
    // We collect the child references into a vector first instead of
    // releasing them directly, because we expect obj to be freed as a result
    // of this. It would be bad if we freed obj in the middle of doing the
    // release callback.
    ObjV children;
    FbleVectorInit(arena, children);
    CollectRefs(heap, obj, &children);
    for (size_t i = 0; i < children.size; ++i) {
      ReleaseInternal(heap, children.xs[i], depth - 1, stack);
    }
    FbleFree(arena, children.xs);
  }
}

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

// New -- see documentation for FbleHeap.new in fble-heap.h
static void* New(FbleHeap* heap_, size_t size)
{
  Heap* heap = (Heap*)heap_;
  assert(heap->next_id != NULL_OBJ_ID);

  Obj* obj = (Obj*)FbleRawAlloc(heap->_base.arena, sizeof(Obj) + size,
      FbleAllocMsg(__FILE__, __LINE__));

  obj->id = heap->next_id++;
  obj->refcount = 1;
  obj->cycle = NULL;
  return obj->obj;
}

// Retain -- see documentation for FbleHeap.retain in fble-heap.h
static void Retain(FbleHeap* heap, void* obj_)
{
  Obj* obj = ToObj(obj_);
  obj->refcount++;
  assert(obj->refcount != 0);
  if (obj->cycle != NULL) {
    obj->cycle->refcount++;
    assert(obj->cycle->refcount != 0);
  }
}

// Release -- see documentation for FbleHeap.release in fble-heap.h
static void Release(FbleHeap* heap_, void* obj_)
{
  Obj* obj = ToObj(obj_);
  Heap* heap = (Heap*)heap_;

  ObjStack* stack = NULL;
  ReleaseInternal(heap, obj, 10000, &stack);
  while (stack != NULL) {
    ObjStack* ostack = stack;
    obj = stack->obj;
    stack = stack->tail;
    FbleFree(heap->_base.arena, ostack);
    ReleaseInternal(heap, obj, 10000, &stack);
  }
}

// AddRef -- see documentation for FbleHeap.add_ref in fble-heap.h
static void AddRef(FbleHeap* heap_, void* src_, void* dst_)
{
  Obj* src = ToObj(src_);
  Obj* dst = ToObj(dst_);
  Heap* heap = (Heap*)heap_;

  Retain(heap_, dst_);

  if (dst->cycle != NULL && src->cycle == dst->cycle) {
    // src and dst belong to the same cycle. So undo the cycle refcount
    // increment that happened in Retain, because this is an internal
    // reference.
    assert(dst->cycle->refcount > 0);
    dst->cycle->refcount--;

    // src and dst were already part of a cycle. No need to worry about
    // introducing a new cycle.
    return;
  }

  if (src->id > dst->id) {
    // src is unreachable from dst. No way this new edge could introduce a
    // cycle.
    return;
  }

  FbleArena* arena = heap->_base.arena;

  // There is potentially a cycle from dst --*--> src --> dst
  // Change all objects with ids between src->id and dst->id to src->id.
  // If any subset of those objects form a path between dst and src, put them
  // all together in the same cycle.
  ObjV stack;
  FbleVectorInit(arena, stack);

  Set visited;
  FbleVectorInit(arena, visited.objs);
  MapInit(arena, &visited.map, INITIAL_MAP_CAPACITY);

  // Keep track of a reverse mapping from child to parent objects. The child
  // object at index i is visited.objs[i].
  struct { size_t size; ObjV* xs; } reverse;
  FbleVectorInit(arena, reverse);

  FbleVectorAppend(arena, stack, dst);
  Insert(arena, &visited, dst);

  {
    ObjV* parents = FbleVectorExtend(arena, reverse);
    FbleVectorInit(arena, *parents);
  }

  // Traverse all objects reachable from dst with ids between src->id and
  // dst->id, setting their ids to src->id.
  while (stack.size > 0) {
    Obj* obj = stack.xs[--stack.size];
    obj->id = src->id;

    ObjV children;
    FbleVectorInit(arena, children);
    CollectRefs(heap, obj, &children);
    for (size_t i = 0; i < children.size; ++i) {
      Obj* child = children.xs[i];
      if (child->id >= src->id) {
        ObjV* parents = NULL;
        size_t j = Insert(arena, &visited, child);
        if (j < reverse.size) {
          parents = reverse.xs + j;
        } else {
          FbleVectorAppend(arena, stack, child);
          parents = FbleVectorExtend(arena, reverse);
          FbleVectorInit(arena, *parents);
        }
        FbleVectorAppend(arena, *parents, obj);
      }
    }
    FbleFree(arena, children.xs);
  }

  // Traverse backwards from src (if reached) to identify all objects in a newly
  // created cycle.
  Set cycle;
  FbleVectorInit(arena, cycle.objs);
  MapInit(arena, &cycle.map, INITIAL_MAP_CAPACITY);
  if (Contains(&visited, src)) {
    FbleVectorAppend(arena, stack, src);
    Insert(arena, &cycle, src);
  }

  while (stack.size > 0) {
    Obj* obj = stack.xs[--stack.size];

    size_t i = Insert(arena, &visited, obj);
    assert(i < reverse.size);
    ObjV* parents = reverse.xs + i;
    for (size_t i = 0; i < parents->size; ++i) {
      Obj* parent = parents->xs[i];
      size_t size = cycle.objs.size;
      if (size == Insert(arena, &cycle, parent)) {
        FbleVectorAppend(arena, stack, parent);
      }
    }
  }

  FbleFree(arena, stack.xs);
  FbleFree(arena, visited.objs.xs);
  FbleFree(arena, visited.map.xs);
  for (size_t i = 0; i < reverse.size; ++i) {
    FbleFree(arena, reverse.xs[i].xs);
  }
  FbleFree(arena, reverse.xs);

  if (cycle.objs.size > 0) {
    // Compute refcounts for the cycle.
    Cycle* new_cycle = FbleAlloc(arena, Cycle);
    new_cycle->refcount = 0;
    new_cycle->size = 0;

    for (size_t i = 0; i < cycle.objs.size; ++i) {
      Obj* obj = cycle.objs.xs[i];
      assert(obj->cycle != new_cycle);
      if (obj->cycle != NULL) {
        assert(obj->cycle != new_cycle);
        assert(obj->cycle->size > 0);
        obj->cycle->size--;
        if (obj->cycle->size == 0) {
          FbleFree(arena, obj->cycle);
        }
        obj->cycle = NULL;
      }

      obj->cycle = new_cycle;
      new_cycle->size++;

      new_cycle->refcount += obj->refcount;

      ObjV children;
      FbleVectorInit(arena, children);
      CollectRefs(heap, obj, &children);
      for (size_t j = 0; j < children.size; ++j) {
        if (Contains(&cycle, children.xs[j])) {
          new_cycle->refcount--;
        }
      }

      FbleFree(arena, children.xs);
    }
  }

  FbleFree(arena, cycle.objs.xs);
  FbleFree(arena, cycle.map.xs);
}

// DelRef -- see documentation for FbleHeap.del_ref in fble-heap.h
static void DelRef(FbleHeap* heap, void* src_, void* dst_)
{
  Obj* src = ToObj(src_);
  Obj* dst = ToObj(dst_);
  if (dst->cycle != NULL && dst->cycle == src->cycle) {
    // src and dst belong to the same cycle. Do a fake increment on the
    // refcount for the destination cycle, because the subsequent call to
    // FbleRefRelease is going to do a decrement on the cycle refcount.
    dst->cycle->refcount++;
  }
  Release(heap, dst_);
}

// FbleNewRefCountingHeap -- see documentation in fble-heap.h
FbleHeap* FbleNewRefCountingHeap(
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

  heap->next_id = NULL_OBJ_ID + 1;
  return &heap->_base;
}

// FbleDeleteRefCountingHeap -- see documentation in fble-heap.h
void FbleDeleteRefCountingHeap(FbleHeap* heap)
{
  FbleFree(heap->arena, heap);
}
