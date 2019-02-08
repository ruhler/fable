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
//   round_id - Temporary state used for detecting cycles. If set to the
//              current round id, the node has been visited this round.
//   round_new - Temporary state used for detecting cycles. If round_id
//               matches the current round id and this is true, then the node
//               has not yet been initially processed for this round.

struct FbleRefArena {
  FbleArena* arena;
  size_t next_id;
  size_t next_round_id;
  void (*free)(struct FbleRefArena* arena, FbleRef* ref);
  void (*added)(struct FbleRefArena* arena, FbleRef* ref, FbleRefV* refs);
};

static bool Contains(FbleRefV* refs, FbleRef* ref);

static FbleRef* CycleHead(FbleRef* ref);
static void CycleAdded(FbleRefArena* arena, FbleRef* ref, FbleRefV* refs);
static void CycleFree(FbleRefArena* arena, FbleRef* ref);

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

// CycleAdded --
//   Get the list of added nodes for a cycle.
//
// Inputs:
//   arena - the reference arena.
//   ref - the reference to get the list of added for.
//   refs - output vector to append the added references to.
//
// Results:
//   None.
//
// Side effects:
//   Appends to refs the cycle head of every reference x that is external to
//   the cycle but reachable by direct reference from a node in the cycle.
static void CycleAdded(FbleRefArena* arena, FbleRef* ref, FbleRefV* refs)
{
  assert(ref->cycle == NULL);

  FbleRefV visited;
  FbleVectorInit(arena->arena, visited);

  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);
  FbleVectorAppend(arena->arena, stack, ref);

  while (stack.size > 0) {
    FbleRef* r = stack.xs[--stack.size];

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    arena->added(arena, r, &children);
    for (size_t i = 0; i < children.size; ++i) {
      FbleRef* child = children.xs[i];
      if (child == ref || CycleHead(child) == ref) {
        if (child != ref && !Contains(&visited, child)) {
          FbleVectorAppend(arena->arena, visited, child);
          FbleVectorAppend(arena->arena, stack, child);
        }
      } else {
        FbleVectorAppend(arena->arena, *refs, CycleHead(child));
      }
    }

    FbleFree(arena->arena, children.xs);
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
    arena->added(arena, r, &children);
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
    void (*added)(FbleRefArena* arena, FbleRef* ref, FbleRefV* refs))
{
  FbleRefArena* ref_arena = FbleAlloc(arena, FbleRefArena);
  ref_arena->arena = arena;
  ref_arena->next_id = 1;
  ref_arena->next_round_id = 1;
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
  ref->round_id = 0;
  ref->round_new = false;
}

// FbleRefRetain -- see documentation in fble-ref.h
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  ref->refcount++;
}

// FbleRefRelease -- see documentation in fble-ref.h
void FbleRefRelease(FbleRefArena* arena, FbleRef* ref)
{
  FbleRefV refs;
  FbleVectorInit(arena->arena, refs);
  FbleVectorAppend(arena->arena, refs, CycleHead(ref));

  while (refs.size > 0) {
    FbleRef* r = refs.xs[--refs.size];
    assert(r->cycle == NULL);
    assert(r->refcount > 0);
    r->refcount--;
    if (r->refcount == 0) {
      // TODO: Inline CycleAdded, CycleFree into a single iteration, rather
      // than iterating over the cycles twice here?
      CycleAdded(arena, r, &refs);
      CycleFree(arena, r);
    }
  }

  FbleFree(arena->arena, refs.xs);
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
  size_t round = arena->next_round_id++;
  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);

  dst->round_id = round;
  dst->round_new = true;
  FbleVectorAppend(arena->arena, stack, dst);

  FbleRefV cycle;
  FbleVectorInit(arena->arena, cycle);
  while (stack.size > 0) {
    FbleRef* ref = stack.xs[stack.size - 1];
    assert(ref->cycle == NULL);
    assert(ref->round_id == round);

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    CycleAdded(arena, ref, &children);

    if (ref->round_new) {
      ref->round_new = false;
      for (size_t i = 0; i < children.size; ++i) {
        FbleRef* child = children.xs[i];
        if (child->round_id != round && child->id >= src->id) {
          child->round_id = round;
          child->round_new = true;
          FbleVectorAppend(arena->arena, stack, child);
        }
      }
    } else {
      stack.size--;
      ref->id = src->id;
      bool in_cycle = (ref == src);
      // TODO: Find a more efficient way to check whether ref has a child node
      // in the cycle.
      for (size_t i = 0; !in_cycle && i < children.size; ++i) {
        for (size_t j = 0; !in_cycle && j < cycle.size; ++j) {
          if (cycle.xs[j] == children.xs[i]) {
            in_cycle = true;
          }
        }
      }

      if (in_cycle) {
        FbleVectorAppend(arena->arena, cycle, ref);
      }
    }

    FbleFree(arena->arena, children.xs);
  }

  bool in_cycle = false;
  for (size_t i = 0; !in_cycle && i < cycle.size; ++i) {
    if (dst == cycle.xs[i]) {
      in_cycle = true;
    }
  }

  if (in_cycle) {
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
      CycleAdded(arena, ref, &children);

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
  FbleFree(arena->arena, stack.xs);
}
