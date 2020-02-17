// ref.c --
//   This file implements the ref.h apis.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "ref.h"

// FbleRef -- see definition and documentation in ref.h
//
// Fields:
//   id - A unique identifier for the node. Ids are assigned in increasing
//        order of node allocation.
//   refcount - The number of references to this node. 0 if the node
//              is a child node in a cycle.
//   cycle - A pointer to the head of the cycle this node belongs to. NULL
//           if the node is not a child node of a cycle.

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

typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRefV* refs;
} AddToVectorCallback;

static void RMapInit(FbleArena* arena, RMap* rmap, size_t capacity);
static size_t* RMapIndex(FbleRef** refs, RMap* rmap, FbleRef* ref);
static size_t Insert(FbleArena* arena, Set* set, FbleRef* ref);
static bool Contains(Set* set, FbleRef* ref);

static void AddToVector(AddToVectorCallback* add, FbleRef* ref);

static FbleRef* CycleHead(FbleRef* ref);
static void CycleAdded(FbleRefArena* arena, FbleRefCallback* add, FbleRef* ref);

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

// AddToVector --
//   An FbleRefCallback that appends a ref to the given vector.
//
// Input:
//   add - this callback.
//   ref - the ref to add.
//
// Results:
//   none.
//
// Side effects:
//   Adds the ref to the vector.
static void AddToVector(AddToVectorCallback* add, FbleRef* ref)
{
  FbleVectorAppend(add->arena, *add->refs, ref);
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

// CycleAddedChildCallback --
//   Callback used in the implementation of CycleAdded.
typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRef* ref;
  Set visited;
  FbleRefV* stack;
  FbleRefCallback* add;
} CycleAddedChildCallback;

// CycleAddedChild --
//   Called for each child visited in a cycle. If the child is internal, adds
//   it to the stack to process. Otherwise calls the 'add' callback on the
//   child's cycle head.
static void CycleAddedChild(CycleAddedChildCallback* data, FbleRef* child)
{
  if (child == data->ref || CycleHead(child) == data->ref) {
    if (child != data->ref) {
      size_t size = data->visited.refs.size;
      if (size == Insert(data->arena, &data->visited, child)) {
        FbleVectorAppend(data->arena, *data->stack, child);
      }
    }
  } else {
    data->add->callback(data->add, CycleHead(child));
  }
}

// CycleAdded --
//   Get the list of added nodes for a cycle.
//
// Inputs:
//   arena - the reference arena.
//   add - callback to call for each reference.
//   ref - the reference to get the list of added for.
//
// Results:
//   None.
//
// Side effects:
//   Calls the add callback on the cycle head of every reference x that is external to
//   the cycle but reachable by direct reference from a node in the cycle.
static void CycleAdded(FbleRefArena* arena, FbleRefCallback* add, FbleRef* ref)
{
  assert(ref->cycle == NULL);

  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);
  FbleVectorAppend(arena->arena, stack, ref);

  CycleAddedChildCallback callback = {
    ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&CycleAddedChild },
    .arena = arena->arena,
    .ref = ref,
    .stack = &stack,
    .add = add
  };
  FbleVectorInit(arena->arena, callback.visited.refs);
  RMapInit(arena->arena, &callback.visited.rmap, INITIAL_RMAP_CAPACITY);

  while (stack.size > 0) {
    arena->added(&callback._base, stack.xs[--stack.size]);
  }

  FbleFree(arena->arena, stack.xs);
  FbleFree(arena->arena, callback.visited.refs.xs);
  FbleFree(arena->arena, callback.visited.rmap.xs);
}

// FbleNewRefArena -- see documentation in ref.h
FbleRefArena* FbleNewRefArena(
    FbleArena* arena, 
    void (*free)(FbleRefArena* arena, FbleRef* ref),
    void (*added)(FbleRefCallback* add, FbleRef* ref))
{
  FbleRefArena* ref_arena = FbleAlloc(arena, FbleRefArena);
  ref_arena->arena = arena;
  ref_arena->next_id = 1;
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

// A stack of references implemented as a singly linked list.
typedef struct Stack {
  FbleRef* ref;
  struct Stack* tail;
} Stack;

typedef struct {
  FbleRefCallback _base;
  FbleRefArena* arena;
  FbleRefV* refs;
  FbleRef* ref;
  Set in_cycle;

  size_t depth;
  Stack* stack;
} RefReleaseCallback;

// RefReleaseChild --
//   Callback function used when releasing references. Decrements the
//   reference count of a ref not in the cycle, adding it to the list of nodes
//   to free if the refcount has gone to zero. Accumulates the list of nodes
//   in the cycle.
static void RefReleaseChild(RefReleaseCallback* data, FbleRef* child)
{
  if (child == data->ref) {
    // Nothing to do. We've already taken care of this node.
  } else if (CycleHead(child) == data->ref) {
    // Mark in-cycle children for traversal if we haven't visited them yet.
    size_t size = data->in_cycle.refs.size;
    if (size == Insert(data->arena->arena, &data->in_cycle, child)) {
      if (data->depth < 1000) {
        // Stack depth isn't too bad, directly do the recursive call here.
        data->depth++;
        data->arena->added(&data->_base, child);
        data->depth--;
      } else {
        // Stack depth is a tad on the deep side; don't do the recursive call
        // directly here to make sure we don't smash the stack.
        Stack* nstack = FbleAlloc(data->arena->arena, Stack);
        nstack->ref = child;
        nstack->tail = data->stack;
        data->stack = nstack;
      }
    }
  } else {
    FbleRef* head = CycleHead(child);
    assert(head->cycle == NULL);
    assert(head->refcount > 0);
    head->refcount--;
    if (head->refcount == 0) {
      FbleVectorAppend(data->arena->arena, *data->refs, head);
    }
  }
}

// FbleRefRelease -- see documentation in ref.h
void FbleRefRelease(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  assert(ref->cycle == NULL);
  assert(ref->refcount > 0);
  ref->refcount--;

  if (ref->refcount == 0) {
    FbleRefV refs;
    FbleVectorInit(arena->arena, refs);
    FbleVectorAppend(arena->arena, refs, ref);

    while (refs.size > 0) {
      FbleRef* r = refs.xs[--refs.size];
      assert(r->cycle == NULL);
      assert(r->refcount == 0);

      RefReleaseCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&RefReleaseChild },
        .arena = arena,
        .refs = &refs,
        .ref = r,
        .depth = 0,
        .stack = NULL,
      };

      FbleVectorInit(arena->arena, callback.in_cycle.refs);
      RMapInit(arena->arena, &callback.in_cycle.rmap, INITIAL_RMAP_CAPACITY);
      Insert(arena->arena, &callback.in_cycle, r);

      arena->added(&callback._base, r);
      while (callback.stack != NULL) {
        Stack* stack = callback.stack;
        callback.stack = stack->tail;
        FbleRef* ref = stack->ref;
        FbleFree(arena->arena, stack);
        arena->added(&callback._base, ref);
      }

      for (size_t i = 0; i < callback.in_cycle.refs.size; ++i) {
        arena->free(arena, callback.in_cycle.refs.xs[i]);
      }

      FbleFree(arena->arena, callback.in_cycle.refs.xs);
      FbleFree(arena->arena, callback.in_cycle.rmap.xs);
    }

    FbleFree(arena->arena, refs.xs);
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
    AddToVectorCallback callback = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddToVector },
      .arena = arena->arena,
      .refs = &children
    };
    CycleAdded(arena, &callback._base, ref);

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
      AddToVectorCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddToVector },
        .arena = arena->arena,
        .refs = &children
      };
      CycleAdded(arena, &callback._base, ref);

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
