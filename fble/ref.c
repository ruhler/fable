// ref.c --
//   This file implements the fble-ref.h apis.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble-ref.h"

// FbleRef -- see definition and documentation in fble-ref.h
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

typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRefV* refs;
} AddToVectorCallback;

static void AddToVector(AddToVectorCallback* add, FbleRef* ref);
static bool Contains(FbleRefV* refs, FbleRef* ref);

static FbleRef* CycleHead(FbleRef* ref);
static void CycleAdded(FbleRefArena* arena, FbleRefCallback* add, FbleRef* ref);
static void CycleFree(FbleRefArena* arena, FbleRef* ref);

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

// Contains --
//   Check of a vector of references contains a give referece.
//
// Inputs:
//   refs - the vector of references.
//   ref - the reference to check for
//
// Results:
//   true if the vector of references contains the given reference, false
//   otherwise.
//
// Side effects:
//   None.
static bool Contains(FbleRefV* refs, FbleRef* ref)
{
  for (size_t i = 0; i < refs->size; ++i) {
    if (refs->xs[i] == ref) {
      return true;
    }
  }
  return false;
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
  FbleRefV* visited;
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
    if (child != data->ref && !Contains(data->visited, child)) {
      FbleVectorAppend(data->arena, *data->visited, child);
      FbleVectorAppend(data->arena, *data->stack, child);
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

  FbleRefV visited;
  FbleVectorInit(arena->arena, visited);

  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);
  FbleVectorAppend(arena->arena, stack, ref);

  CycleAddedChildCallback callback = {
    ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&CycleAddedChild },
    .arena = arena->arena,
    .ref = ref,
    .visited = &visited,
    .stack = &stack,
    .add = add
  };

  while (stack.size > 0) {
    arena->added(&callback._base, stack.xs[--stack.size]);
  }

  FbleFree(arena->arena, visited.xs);
  FbleFree(arena->arena, stack.xs);
}

// CycleFree --
//   Free the object associated with the given ref and its cycle, because the
//   ref is no longer acessible.
//
// Inputs:
//   arena - the reference arena.
//   ref - the reference whose cycle to free.
//
// Results:
//   None.
//
// Side effects:
//   Unspecified.
static void CycleFree(FbleRefArena* arena, FbleRef* ref)
{
  assert(ref->cycle == NULL);

  FbleRefV in_cycle;
  FbleVectorInit(arena->arena, in_cycle);

  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);
  FbleVectorAppend(arena->arena, stack, ref);
  FbleVectorAppend(arena->arena, in_cycle, ref);

  while (stack.size > 0) {
    FbleRef* r = stack.xs[--stack.size];

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    AddToVectorCallback callback = {
      ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddToVector },
      .arena = arena->arena,
      .refs = &children
    };
    arena->added(&callback._base, r);
    for (size_t i = 0; i < children.size; ++i) {
      FbleRef* child = children.xs[i];
      if (CycleHead(child) == ref) {
        if (!Contains(&in_cycle, child)) {
          FbleVectorAppend(arena->arena, stack, child);
          FbleVectorAppend(arena->arena, in_cycle, child);
        }
      }
    }

    FbleFree(arena->arena, children.xs);
  }

  for (size_t i = 0; i < in_cycle.size; ++i) {
    arena->free(arena, in_cycle.xs[i]);
  }

  FbleFree(arena->arena, in_cycle.xs);
  FbleFree(arena->arena, stack.xs);
}

// FbleNewRefArena -- see documentation in fble-ref.h
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

// FbleDeleteRefArena -- see documentation in fble-ref.h
void FbleDeleteRefArena(FbleRefArena* arena)
{
  FbleFree(arena->arena, arena);
}

// FbleRefArenaArena -- see documentation in fble-ref.h
FbleArena* FbleRefArenaArena(FbleRefArena* arena)
{
  return arena->arena;
}

// FbleRefInit -- see documentation in fble-ref.h
void FbleRefInit(FbleRefArena* arena, FbleRef* ref)
{
  ref->id = arena->next_id++;
  ref->refcount = 1;
  ref->cycle = NULL;
}

// FbleRefRetain -- see documentation in fble-ref.h
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  ref->refcount++;
}

typedef struct {
  FbleRefCallback _base;
  FbleArena* arena;
  FbleRefV* refs;
} RefReleaseCallback;

// RefRelease --
//   Callback function to decrement the refcount of a ref and add it to the
//   list of refs if the refcount has gone to zero.
static void RefRelease(RefReleaseCallback* data, FbleRef* ref)
{
  assert(ref->cycle == NULL);
  assert(ref->refcount > 0);
  ref->refcount--;
  if (ref->refcount == 0) {
    FbleVectorAppend(data->arena, *data->refs, ref);
  }
}

// FbleRefRelease -- see documentation in fble-ref.h
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
      assert(r->refcount == 0);

      RefReleaseCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&RefRelease },
        .arena = arena->arena,
        .refs = &refs
      };

      // TODO: Inline CycleAdded, CycleFree into a single iteration, rather
      // than iterating over the cycles twice here?
      CycleAdded(arena, &callback._base, r);
      CycleFree(arena, r);
    }

    FbleFree(arena->arena, refs.xs);
  }
}

// FbleRefAdd -- see documentation in fble-ref.h
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

  FbleRefV visited;
  FbleVectorInit(arena->arena, visited);

  // Keep track of a reverse mapping from child to parent nodes. The child
  // node at index i is visited[i].
  struct { size_t size; FbleRefV* xs; } reverse;
  FbleVectorInit(arena->arena, reverse);

  FbleVectorAppend(arena->arena, stack, dst);
  FbleVectorAppend(arena->arena, visited, dst);

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
        for (size_t j = 0; parents == NULL && j < visited.size; ++j) {
          if (visited.xs[j] == child) {
            parents = reverse.xs + j;
          }
        }

        if (parents == NULL) {
          FbleVectorAppend(arena->arena, stack, child);
          FbleVectorAppend(arena->arena, visited, child);
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
  FbleRefV cycle;
  FbleVectorInit(arena->arena, cycle);
  for (size_t i = 0; i < visited.size; ++i) {
    if (visited.xs[i] == src) {
      FbleVectorAppend(arena->arena, stack, src);
      FbleVectorAppend(arena->arena, cycle, src);
    }
  }

  while (stack.size > 0) {
    FbleRef* ref = stack.xs[--stack.size];

    FbleRefV* parents = NULL;
    for (size_t i = 0; parents == NULL && i < visited.size; ++i) {
      if (visited.xs[i] == ref) {
        parents = reverse.xs + i;
      }
    }
    assert(parents != NULL);

    for (size_t i = 0; i < parents->size; ++i) {
      FbleRef* parent = parents->xs[i];
      if (!Contains(&cycle, parent)) {
        FbleVectorAppend(arena->arena, stack, parent);
        FbleVectorAppend(arena->arena, cycle, parent);
      }
    }
  }

  FbleFree(arena->arena, stack.xs);
  FbleFree(arena->arena, visited.xs);
  for (size_t i = 0; i < reverse.size; ++i) {
    FbleFree(arena->arena, reverse.xs[i].xs);
  }
  FbleFree(arena->arena, reverse.xs);

  if (cycle.size > 0) {
    // Fix up refcounts for the cycle by moving any refs from nodes outside the
    // cycle to the head node of the cycle and removing all refs from nodes
    // inside the cycle.
    size_t total = 0;
    size_t internal = 0;

    for (size_t i = 0; i < cycle.size; ++i) {
      FbleRef* ref = cycle.xs[i];
      total += ref->refcount;

      FbleRefV children;
      FbleVectorInit(arena->arena, children);
      AddToVectorCallback callback = {
        ._base = { .callback = (void(*)(struct FbleRefCallback*, FbleRef*))&AddToVector },
        .arena = arena->arena,
        .refs = &children
      };
      CycleAdded(arena, &callback._base, ref);

      // TODO: Figure out a more efficient way to check if a child is in the
      // cycle.
      for (size_t j = 0; j < children.size; ++j) {
        for (size_t k = 0; k < cycle.size; ++k) {
          if (children.xs[j] == cycle.xs[k]) {
            internal++;
          }
        }
      }

      FbleFree(arena->arena, children.xs);
    }
    
    for (size_t i = 0; i < cycle.size; ++i) {
      cycle.xs[i]->refcount = 0;
      cycle.xs[i]->cycle = dst;
    }

    dst->refcount = total - internal;
    dst->cycle = NULL;
  }

  FbleFree(arena->arena, cycle.xs);
}
