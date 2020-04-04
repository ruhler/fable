// ref.c --
//   This file implements the ref.h apis.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "ref.h"

// Special ref id that we gaurentee no valid ref will have.
// This can be used as a sentinel value.
#define NULL_REF_ID 0

// FbleRef -- see definition and documentation in ref.h
//
// Fields:
//   id - A unique identifier for the node. Ids are assigned in increasing
//        order of node allocation.
//   refcount - The number of references to this node. 0 if the node
//              is a child node in a cycle.
//   cycle - A pointer to the head of the cycle this node belongs to. NULL
//           if the node is not a child node of a cycle.

// FbleRefArena -- See documentation in fble/ref.h.
struct FbleRefArena {
  FbleArena* arena;
  size_t next_id;
  void (*free)(struct FbleRefArena* arena, FbleRef* ref);
  void (*added)(FbleRefCallback* add, FbleRef* ref);
};

// RMap --
//   A hash based mapping from key to index.
typedef struct {
  size_t capacity;
  size_t* xs;
} RMap;

#define INITIAL_RMAP_CAPACITY 3

// Set --
//   A set of references with map from index to reference and reverse hashmap
//   from reference to index.
//
// Fields:
//   refs - the set of references in the map.
//   rmap - a hash map from reference to its index in refs.
#define RMAP_EMPTY ((size_t) (-1))
typedef struct {
  FbleRefV refs;
  RMap rmap;
} Set;

// RefStack -- 
//   A stack of references implemented as a singly linked list.
typedef struct RefStack {
  FbleRef* ref;
  struct RefStack* tail;
} RefStack;

// RefReleaseCallback --
//   Callback structure used for RefRelease.
typedef struct {
  FbleRefCallback _base;
  FbleRefArena* arena;

  // Maximum recursion depth allowed, to avoid smashing the stack, and the
  // explicit stack to revert to in case the recursion depth is reached.
  size_t depth;
  RefStack** stack;

  // Singly linked list through cycle field of nodes in the cycle being
  // released. A node is on this list iff it has its id set to NULL_REF_ID.
  FbleRef* cycle;
} RefReleaseCallback;

// CycleAddedChildCallback --
//   Callback used in the implementation of CycleAdded.
typedef struct {
  FbleRefCallback _base;
  FbleRefArena* arena;

  // Singly linked list through cycle field of nodes in the cycle that have
  // been visited. A node is on this list iff it has its id set to
  // NULL_REF_ID.
  FbleRef* cycle;

  // The vector for accumulating added nodes.
  FbleRefV* added;
} CycleAddedChildCallback;

static void RMapInit(FbleArena* arena, RMap* rmap, size_t capacity);
static size_t* RMapIndex(FbleRef** refs, RMap* rmap, FbleRef* ref);
static size_t Insert(FbleArena* arena, Set* set, FbleRef* ref);
static bool Contains(Set* set, FbleRef* ref);

static FbleRef* CycleHead(FbleRef* ref);

static void RefReleaseChild(RefReleaseCallback* data, FbleRef* child);
static void RefRelease(FbleRefArena* arena, FbleRef* ref, size_t depth, RefStack** stack);

static void CycleAddedChild(CycleAddedChildCallback* data, FbleRef* child);
static void CycleAdded(FbleRefArena* arena, FbleRef* ref, FbleRefV* added);

// RMapInit --
//   Initialize an RMap.
//
// Inputs:
//   arena - arena to use for allocations.
//   rmap - a pointer to an uninitialized RMap.
//   capacity - the initial capacity to use for the reverse map.
//
// Results: 
//   none
//
// Side effects:
//   Initializes the rmap. The caller should call FbleFree(arena, rmap->xs)
//   when done with using the map to release memory allocated for the RMap.
static void RMapInit(FbleArena* arena, RMap* rmap, size_t capacity)
{
  rmap->capacity = capacity;
  rmap->xs = FbleArrayAlloc(arena, size_t, capacity);
  for (size_t i = 0; i < capacity; ++i) {
    rmap->xs[i] = RMAP_EMPTY;
  }
}

// RMapIndex --
//   Return a pointer to the rmap index for the given reference.
//
// Inputs:
//   refs - the array of references in the set.
//   rmap - the rmap.
//   ref - the reference to look up.
//
// Returns:
//   A pointer to the rmap index where the reference is located, or where the
//   reference would be inserted if it isn't already in the rmap.
//
// Side effects: 
//   None.
static size_t* RMapIndex(FbleRef** refs, RMap* rmap, FbleRef* ref)
{
  size_t bucket = (size_t)ref % rmap->capacity;
  size_t index = rmap->xs[bucket];
  while (index != RMAP_EMPTY && refs[index] != ref) {
    bucket = (bucket + 1) % rmap->capacity;
    index = rmap->xs[bucket];
  }
  return rmap->xs + bucket;
}

// Insert --
//   Insert a reference into the given set.
//
// Inputs:
//   arena - the arena to use for allocations.
//   set - the set to insert into.
//   ref - the reference to insert.
//
// Results:
//   The index of the reference in set->refs after insertion, which will be
//   the same index as before insertion if the reference was already in the
//   map.
//
// Side effects:
//   Appends the reference to refs if it was not already in refs. If the
//   reference was already in refs, does nothing.
//   May resize the rmap, invalidating any pointers into rmap.
static size_t Insert(FbleArena* arena, Set* set, FbleRef* ref)
{
  size_t* index = RMapIndex(set->refs.xs, &set->rmap, ref);
  if (*index != RMAP_EMPTY) {
    return *index;
  }

  *index = set->refs.size;
  FbleVectorAppend(arena, set->refs, ref);

  if (3 * set->refs.size > set->rmap.capacity) {
    // It is time to resize the rmap.
    RMap rmap;
    RMapInit(arena, &rmap, 2 * set->rmap.capacity - 1);
    for (size_t i = 0; i < set->rmap.capacity; ++i) {
      size_t ix = set->rmap.xs[i];
      if (ix != RMAP_EMPTY) {
        *RMapIndex(set->refs.xs, &rmap, set->refs.xs[ix]) = ix;
      }
    }
    FbleFree(arena, set->rmap.xs);
    set->rmap = rmap;
  }
  return set->refs.size - 1;
}

// Contains --
//   Check whether a set contains the given reference.
//
// Inputs:
//   set - the set to check.
//   ref - the reference to check for.
//
// Results: 
//   True if ref is in the set, false otherwise.
//
// Side effects:
//   None.
static bool Contains(Set* map, FbleRef* ref)
{
  return *RMapIndex(map->refs.xs, &map->rmap, ref) != RMAP_EMPTY;
}

// CycleHead --
//   Get the head of the biggest cycle that ref belongs to.
//
// Inputs:
//   ref - The reference to get the head of.
//
// Results:
//   The head of the biggest cycle that ref belongs to.
//
// Side effects:
//   None.
static FbleRef* CycleHead(FbleRef* ref)
{
  while (ref->cycle != NULL) {
    ref = ref->cycle;
  }
  return ref;
}

// RefReleaseChild --
//   Callback function used when releasing references.
static void RefReleaseChild(RefReleaseCallback* data, FbleRef* child)
{
  // If child->id is NULL_REF_ID, we have already processed the child and
  // there is nothing more to do.
  if (child->id != NULL_REF_ID) {
    if (CycleHead(child)->id == NULL_REF_ID) {
      // This child belongs to the cycle being released.
      child->id = NULL_REF_ID;
      child->cycle = data->cycle;
      data->cycle = child;

      // TODO: Could this smash the stack?
      data->arena->added(&data->_base, child);
    } else {
      // This child is outside of the cycle being released.
      RefRelease(data->arena, child, data->depth, data->stack);
    }
  }
}

// RefRelease --
//   Release a reference recursively.
//
// Inputs:
//   arena - arena to use for allocations.
//   ref - the reference to release.
//   depth - maximum recursion depth allowed, to avoid smashing the stack.
//   stack - stack to push subsequent refs on if recursion limit is reached.
//
// Results:
//   none.
//
// Side effect:
//   Recursively releases a reference.
static void RefRelease(FbleRefArena* arena, FbleRef* ref, size_t depth, RefStack** stack)
{
  if (depth == 0) {
    RefStack* nstack = FbleAlloc(arena->arena, RefStack);
    nstack->ref = ref;
    nstack->tail = *stack;
    *stack = nstack;
    return;
  }

  ref = CycleHead(ref);
  assert(ref->cycle == NULL);
  assert(ref->refcount > 0);
  ref->refcount--;

  if (ref->refcount == 0) {
    ref->id = NULL_REF_ID;
    ref->cycle = NULL;

    RefReleaseCallback callback = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&RefReleaseChild },
      .arena = arena,
      .depth = depth - 1,
      .stack = stack,
      .cycle = ref,
    };

    arena->added(&callback._base, ref);
    while (callback.cycle != NULL) {
      FbleRef* r = callback.cycle;
      callback.cycle = r->cycle;
      arena->free(arena, r);
    }
  }
}

// CycleAddedChild --
//   Called for each child visited in a cycle. If the child is internal, adds
//   it to the stack to process. Otherwise calls the 'add' callback on the
//   child's cycle head.
static void CycleAddedChild(CycleAddedChildCallback* data, FbleRef* child)
{
  if (child->id != NULL_REF_ID) {
    if (CycleHead(child)->id == NULL_REF_ID) {
      child->id = NULL_REF_ID;
      child->cycle = data->cycle;
      data->cycle = child;

      // TODO: Could this smash the stack?
      data->arena->added(&data->_base, child);
    } else {
      child = CycleHead(child);
      FbleVectorAppend(data->arena->arena, *data->added, child);
    }
  }
}

// CycleAdded --
//   Get the list of added nodes for a cycle.
//
// Inputs:
//   arena - the reference arena.
//   ref - the reference to get the list of added for.
//   added - vector to add references to.
//
// Results:
//   None.
//
// Side effects:
//   Appends the cycle head of every reference x that is external to the cycle
//   but reachable by direct reference from a node in the cycle.
static void CycleAdded(FbleRefArena* arena, FbleRef* ref, FbleRefV* added)
{
  assert(ref->cycle == NULL);

  // We're going to be a little sneaky here to more efficiently keep track of
  // which nodes are in the cycle and have been visited. All nodes in the
  // cycle should have the cycle field pointing to ref and the same id as ref.
  // Because we know that, we can overwrite the cycle and id fields to track
  // which nodes we have visited, so long as we restore the fields when we are
  // done.
  size_t id = ref->id;

  ref->id = NULL_REF_ID;

  CycleAddedChildCallback callback = {
    ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&CycleAddedChild },
    .arena = arena,
    .cycle = ref,
    .added = added
  };

  arena->added(&callback._base, ref);

  // Restore the 'cycle' and 'id' fields for all the nodes in the cycle.
  while (callback.cycle != NULL) {
    FbleRef* r = callback.cycle;
    callback.cycle = r->cycle;
    r->id = id;
    r->cycle = ref;
  }

  ref->cycle = NULL;
}

// FbleNewRefArena -- see documentation in ref.h
FbleRefArena* FbleNewRefArena(
    FbleArena* arena, 
    void (*free)(FbleRefArena* arena, FbleRef* ref),
    void (*added)(FbleRefCallback* add, FbleRef* ref))
{
  FbleRefArena* ref_arena = FbleAlloc(arena, FbleRefArena);
  ref_arena->arena = arena;
  ref_arena->next_id = NULL_REF_ID + 1;
  ref_arena->free = free;
  ref_arena->added = added;
  return ref_arena;
}

// FbleDeleteRefArena -- see documentation in ref.h
void FbleDeleteRefArena(FbleRefArena* arena)
{
  FbleFree(arena->arena, arena);
}

// FbleRefArenaArena -- see documentation in ref.h
FbleArena* FbleRefArenaArena(FbleRefArena* arena)
{
  return arena->arena;
}

// FbleRefInit -- see documentation in ref.h
void FbleRefInit(FbleRefArena* arena, FbleRef* ref)
{
  ref->id = arena->next_id++;
  ref->refcount = 1;
  ref->cycle = NULL;
}

// FbleRefRetain -- see documentation in ref.h
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  ref->refcount++;
}

// FbleRefRelease -- see documentation in ref.h
void FbleRefRelease(FbleRefArena* arena, FbleRef* ref)
{
  RefStack* stack = NULL;
  RefRelease(arena, ref, 10000, &stack);
  while (stack != NULL) {
    RefStack* ostack = stack;
    ref = stack->ref;
    stack = stack->tail;
    FbleFree(arena->arena, ostack);
    RefRelease(arena, ref, 10000, &stack);
  }
}

// FbleRefAdd -- see documentation in ref.h
void FbleRefAdd(FbleRefArena* arena, FbleRef* src, FbleRef* dst)
{
  if (CycleHead(src) == CycleHead(dst)) {
    // Ignore new references within the same cycle.
    return;
  }

  FbleRefRetain(arena, dst);
  if (src->id > dst->id) {
    return;
  }

  src = CycleHead(src);
  dst = CycleHead(dst);

  // There is potentially a cycle from dst --*--> src --> dst
  // Change all nodes with ids between src->id and dst->id to src->id.
  // If any subset of those nodes form a path between dst and src, set dst as
  // their cycle and transfer their external refcount to dst.
  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);

  Set visited;
  FbleVectorInit(arena->arena, visited.refs);
  RMapInit(arena->arena, &visited.rmap, INITIAL_RMAP_CAPACITY);

  // Keep track of a reverse mapping from child to parent nodes. The child
  // node at index i is visited.refs[i].
  struct { size_t size; FbleRefV* xs; } reverse;
  FbleVectorInit(arena->arena, reverse);

  FbleVectorAppend(arena->arena, stack, dst);
  Insert(arena->arena, &visited, dst);

  {
    FbleRefV* parents = FbleVectorExtend(arena->arena, reverse);
    FbleVectorInit(arena->arena, *parents);
  }

  // Traverse all nodes reachable from dst with ids between src->id and
  // dst->id, setting their ids to src->id.
  while (stack.size > 0) {
    FbleRef* ref = stack.xs[--stack.size];
    assert(ref->cycle == NULL);
    ref->id = src->id;

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    CycleAdded(arena, ref, &children);

    for (size_t i = 0; i < children.size; ++i) {
      FbleRef* child = children.xs[i];
      if (child->id >= src->id) {
        FbleRefV* parents = NULL;
        size_t j = Insert(arena->arena, &visited, child);
        if (j < reverse.size) {
          parents = reverse.xs + j;
        } else {
          FbleVectorAppend(arena->arena, stack, child);
          parents = FbleVectorExtend(arena->arena, reverse);
          FbleVectorInit(arena->arena, *parents);
        }
        FbleVectorAppend(arena->arena, *parents, ref);
      }
    }
    FbleFree(arena->arena, children.xs);
  }

  // Traverse backwards from src (if reached) to identify all nodes in a newly
  // created cycle.
  Set cycle;
  FbleVectorInit(arena->arena, cycle.refs);
  RMapInit(arena->arena, &cycle.rmap, INITIAL_RMAP_CAPACITY);
  if (Contains(&visited, src)) {
    FbleVectorAppend(arena->arena, stack, src);
    Insert(arena->arena, &cycle, src);
  }

  while (stack.size > 0) {
    FbleRef* ref = stack.xs[--stack.size];

    size_t i = Insert(arena->arena, &visited, ref);
    assert(i < reverse.size);
    FbleRefV* parents = reverse.xs + i;
    for (size_t i = 0; i < parents->size; ++i) {
      FbleRef* parent = parents->xs[i];
      size_t size = cycle.refs.size;
      if (size == Insert(arena->arena, &cycle, parent)) {
        FbleVectorAppend(arena->arena, stack, parent);
      }
    }
  }

  FbleFree(arena->arena, stack.xs);
  FbleFree(arena->arena, visited.refs.xs);
  FbleFree(arena->arena, visited.rmap.xs);
  for (size_t i = 0; i < reverse.size; ++i) {
    FbleFree(arena->arena, reverse.xs[i].xs);
  }
  FbleFree(arena->arena, reverse.xs);

  if (cycle.refs.size > 0) {
    // Fix up refcounts for the cycle by moving any refs from nodes outside the
    // cycle to the head node of the cycle and removing all refs from nodes
    // inside the cycle.
    size_t total = 0;
    size_t internal = 0;

    for (size_t i = 0; i < cycle.refs.size; ++i) {
      FbleRef* ref = cycle.refs.xs[i];
      total += ref->refcount;

      FbleRefV children;
      FbleVectorInit(arena->arena, children);
      CycleAdded(arena, ref, &children);

      for (size_t j = 0; j < children.size; ++j) {
        if (Contains(&cycle, children.xs[j])) {
          internal++;
        }
      }

      FbleFree(arena->arena, children.xs);
    }
    
    for (size_t i = 0; i < cycle.refs.size; ++i) {
      cycle.refs.xs[i]->refcount = 0;
      cycle.refs.xs[i]->cycle = dst;
    }

    dst->refcount = total - internal;
    dst->cycle = NULL;
  }

  FbleFree(arena->arena, cycle.refs.xs);
  FbleFree(arena->arena, cycle.rmap.xs);
}

// FbleRefDelete -- see documentation in ref.h
void FbleRefDelete(FbleRefArena* arena, FbleRef* src, FbleRef* dst)
{
  src = CycleHead(src);
  dst = CycleHead(dst);
  assert (src != dst && "TODO: RefDelete inside of cycles");
  FbleRefRelease(arena, dst);
}
