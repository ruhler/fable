// ref.c --
//   This file implements the ref.h apis.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "ref.h"

// Special ref id that we gaurentee no valid ref will have.
// This can be used as a sentinel value.
#define NULL_REF_ID 0

// FbleCycle --
//   Represents a set of objects that form a cycle.
//
// Fields:
//   refcount - the total number of references from objects outside this cycle
//              to objects inside this cycle.
//   size - the number of objects inside this cycle.
struct FbleCycle {
  size_t refcount;
  size_t size;
};

// FbleRef -- see definition and documentation in ref.h
//
// Fields:
//   id - A unique identifier for the node. Ids are assigned in increasing
//        order of node allocation.
//   refcount - The number of references to this node.
//   cycle - A pointer to the cycle this object belongs to, or NULL if this
//           object does not belong to a cycle.

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

// AddCallback --
//   Callback structure used for Add
typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRefV* vector;
} AddCallback;

// RefReleaseCallback --
//   Callback structure used for RefReleaseChild.
typedef struct {
  FbleRefCallback _base;
  FbleRefArena* arena;

  // Maximum recursion depth allowed, to avoid smashing the stack, and the
  // explicit stack to revert to in case the recursion depth is reached.
  size_t depth;
  RefStack** stack;
} RefReleaseCallback;

// CycleRefAddCallback --
//   Callback structure used for CycleRefAdd
typedef struct {
  FbleRefCallback _base;
  FbleCycle* cycle;
} CycleRefAddCallback;

static void RMapInit(FbleArena* arena, RMap* rmap, size_t capacity);
static size_t* RMapIndex(FbleRef** refs, RMap* rmap, FbleRef* ref);
static size_t Insert(FbleArena* arena, Set* set, FbleRef* ref);
static bool Contains(Set* set, FbleRef* ref);

static void AddChild(AddCallback* data, FbleRef* child);
static void RefReleaseChild(RefReleaseCallback* data, FbleRef* child);
static void CycleRefAdd(CycleRefAddCallback* data, FbleRef* child);
static void RefRelease(FbleRefArena* arena, FbleRef* ref, size_t depth, RefStack** stack);

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

// AddChild --
//   Callback function used when collecting child references.
static void AddChild(AddCallback* data, FbleRef* child)
{
  FbleVectorAppend(data->arena, *data->vector, child);
}

// RefReleaseChild --
//   Callback function used when releasing references.
static void RefReleaseChild(RefReleaseCallback* data, FbleRef* child)
{
  RefRelease(data->arena, child, data->depth, data->stack);
}

// CycleRefAdd --
//   Increment the refcount for a cycle if the child belongs to the cycle.
//
//   Used when removing an object from a cycle, at which point all references
//   from the object to children in the cycle go from being internal
//   references to external references.
//
// Inputs:
//   data - information about which cycle to look for.
//   child - the child to increment based on.
//
// Results:
//   None.
//
// Side effects:
//   If the child belongs to the target cycle, increment the refcount on the
//   cycle.
static void CycleRefAdd(CycleRefAddCallback* data, FbleRef* child)
{
  if (child->cycle != NULL && child->cycle == data->cycle) {
    data->cycle->refcount++;
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

  assert(ref->refcount > 0);
  ref->refcount--;

  if (ref->cycle != NULL) {
    assert(ref->cycle->refcount > 0);
    ref->cycle->refcount--;

    if (ref->refcount == 0) {
      // It is safe to remove this reference from the cycle, because nobody
      // points to this reference anymore.

      // Change outgoing internal references to external references.
      CycleRefAddCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&CycleRefAdd },
        .cycle = ref->cycle,
      };
      arena->added(&callback._base, ref);

      // Remove the object from the cycle.
      assert(ref->cycle->size > 0);
      ref->cycle->size--;
      if (ref->cycle->size == 0) {
        FbleFree(arena->arena, ref->cycle);
      }
      ref->cycle = NULL;
    }
  }

  if (ref->refcount == 0) {
    // We should already have removed the reference from its cycle.
    assert(ref->cycle == NULL);

    RefReleaseCallback callback = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&RefReleaseChild },
      .arena = arena,
      .depth = depth - 1,
      .stack = stack,
    };

    if (ref->id != NULL_REF_ID) {
      arena->added(&callback._base, ref);
    }
    arena->free(arena, ref);
  } else if (ref->cycle != NULL && ref->cycle->refcount == 0) {
    // The cycle is unreachable, though there are still other objects in
    // the cycle refering to this reference. To break the cycle, we drop all
    // references out of this reference. That should be enough to naturally
    // unravel the cycle. Because we can't effect what arena->added will
    // traverse, we mark the reference specially by changing its id to
    // NULL_REF_ID.
    assert(ref->id != NULL_REF_ID);
    ref->id = NULL_REF_ID;

    // TODO: Consider optimizing the callbacks here to reduce number of
    // traversals.

    // Increment the cycle refcount for each child in the cycle to make up for
    // the decrement that will come when we release those references.
    CycleRefAddCallback callback1 = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&CycleRefAdd },
      .cycle = ref->cycle,
    };
    arena->added(&callback1._base, ref);

    // Release all child references.
    // We collect the child references into a vector first instead of
    // releasing them directly, because we expect ref to be freed as a result
    // of this. It would be bad if we freed ref in the middle of doing the
    // release callback.
    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    AddCallback callback2 = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddChild },
      .arena = arena->arena,
      .vector = &children
    };
    arena->added(&callback2._base, ref);

    for (size_t i = 0; i < children.size; ++i) {
      RefRelease(arena, children.xs[i], depth - 1, stack);
    }
    FbleFree(arena->arena, children.xs);
  }
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
  assert(arena->next_id != NULL_REF_ID);
  ref->id = arena->next_id++;
  ref->refcount = 1;
  ref->cycle = NULL;
}

// FbleRefRetain -- see documentation in ref.h
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref)
{
  if (ref != NULL) {
    ref->refcount++;
    assert(ref->refcount != 0);
    if (ref->cycle != NULL) {
      ref->cycle->refcount++;
      assert(ref->cycle->refcount != 0);
    }
  }
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
  FbleRefRetain(arena, dst);

  if (dst->cycle != NULL && src->cycle == dst->cycle) {
    // src and dst belong to the same cycle. So undo the cycle refcount
    // increment that happened in FbleRefRetain, because this is an internal
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

  // There is potentially a cycle from dst --*--> src --> dst
  // Change all nodes with ids between src->id and dst->id to src->id.
  // If any subset of those nodes form a path between dst and src, put them
  // all together in the same cycle.
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
    ref->id = src->id;

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    AddCallback callback = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddChild },
      .arena = arena->arena,
      .vector = &children
    };
    arena->added(&callback._base, ref);

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
    // Compute refcounts for the cycle.
    FbleCycle* new_cycle = FbleAlloc(arena->arena, FbleCycle);
    new_cycle->refcount = 0;
    new_cycle->size = 0;

    for (size_t i = 0; i < cycle.refs.size; ++i) {
      FbleRef* ref = cycle.refs.xs[i];
      assert(ref->cycle != new_cycle);
      if (ref->cycle != NULL) {
        assert(ref->cycle != new_cycle);
        assert(ref->cycle->size > 0);
        ref->cycle->size--;
        if (ref->cycle->size == 0) {
          FbleFree(arena->arena, ref->cycle);
        }
        ref->cycle = NULL;
      }

      ref->cycle = new_cycle;
      new_cycle->size++;

      new_cycle->refcount += ref->refcount;

      // TODO: Use a more efficient callback here?
      FbleRefV children;
      FbleVectorInit(arena->arena, children);
      AddCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddChild },
        .arena = arena->arena,
        .vector = &children
      };
      arena->added(&callback._base, ref);

      for (size_t j = 0; j < children.size; ++j) {
        if (Contains(&cycle, children.xs[j])) {
          new_cycle->refcount--;
        }
      }

      FbleFree(arena->arena, children.xs);
    }
  }

  FbleFree(arena->arena, cycle.refs.xs);
  FbleFree(arena->arena, cycle.rmap.xs);
}

// FbleRefDelete -- see documentation in ref.h
void FbleRefDelete(FbleRefArena* arena, FbleRef* src, FbleRef* dst)
{
  if (dst->cycle != NULL && dst->cycle == src->cycle) {
    // src and dst belong to the same cycle. Do a fake increment on the
    // refcount for the destination cycle, because the subsequent call to
    // FbleRefRelease is going to do a decrement on the cycle refcount.
    dst->cycle->refcount++;
  }
  FbleRefRelease(arena, dst);
}
