// fble-ref-test.c --
//   This file implements the main entry point for the fble-ref-test program.

#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

typedef struct FbleRef FbleRef;
typedef struct FbleRefArena FbleRefArena;

// FbleRefV --
//   A vector of FbleRefs.
typedef struct {
  size_t size;
  FbleRef** xs;
} FbleRefV;

// FbleRefArena --
//   An arena used for allocation references of a common kind.
struct FbleRefArena {
  FbleArena* arena;
  size_t next_id;
  size_t next_round_id;

  // free --
  //   Free the object associated with the given ref, because the ref is no
  //   longer acessible.
  //
  // Inputs:
  //   arena - the reference arena.
  //   ref - the references to free.
  //
  // Results:
  //   None.
  //
  // Side effects:
  //   Unspecified.
  void (*free)(FbleRefArena* arena, FbleRef* ref);

  // added --
  //   Return the list of references that have been added to the given ref.
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
  //   Appends to refs every reference x for which FbleRefAdd(arena, ref, x)
  //   has been called.
  void (*added)(FbleRefArena* arena, FbleRef* ref, FbleRefV* refs);
};

// FbleRef --
//   A reference to an object. The intention is that the user knows how to get
//   a pointer to the object given a pointer to this reference, typically
//   because the two pointers are the same.
//
//   When references form cycles, we designate one of the nodes in the cycle
//   as the head of the cycle. All other nodes in the cycle are children of
//   the cycle. The child nodes of cycles are treated specially. Note that the
//   head of a cycle may be a child of some other cycle.
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
struct FbleRef {
  size_t id;
  size_t refcount;
  FbleRef* cycle;
  size_t round_id;
  bool round_new;
};

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
  int round_id = arena->next_round_id++;
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
      if (child == ref || child->cycle == ref) {
        if (child != ref && child->round_id != round_id) {
          assert(child->cycle == ref);
          child->round_id = round_id;
          FbleVectorAppend(arena->arena, stack, CycleHead(child));
        }
      } else {
        FbleVectorAppend(arena->arena, *refs, child);
      }
    }

    FbleFree(arena->arena, children.xs);
  }
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
  int round_id = arena->next_round_id++;
  FbleRefV in_cycle;
  FbleVectorInit(arena->arena, in_cycle);

  FbleRefV stack;
  FbleVectorInit(arena->arena, stack);
  FbleVectorAppend(arena->arena, stack, ref);

  while (stack.size > 0) {
    FbleRef* r = stack.xs[--stack.size];
    FbleVectorAppend(arena->arena, in_cycle, r);

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    arena->added(arena, r, &children);
    for (size_t i = 0; i < children.size; ++i) {
      FbleRef* child = children.xs[i];
      if (child->cycle == ref) {
        if (child->round_id != round_id) {
          child->round_id = round_id;
          FbleVectorAppend(arena->arena, stack, child);
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

// FbleRefInit --
//   Initialize and retain the reference pointed to by ref.
//
// Inputs:
//   arena - The reference arena the ref should belong to.
//   ref - The reference to initialize.
//
// Results:
//   None.
//
// Side effects:
//   Initializes the reference and performs an FbleRefRetain on the reference.
void FbleRefInit(FbleRefArena* arena, FbleRef* ref)
{
  ref->id = arena->next_id++;
  ref->refcount = 1;
  ref->cycle = NULL;
  ref->round_id = 0;
  ref->round_new = false;
}

// FbleRefRetain --
//   Cause ref, and any other references that are referred to directly or
//   indirectly from ref, to be retained until a corresponding FbleRefRelease
//   call is made.
//
// Inputs:
//   arena - The reference arena the ref belongs to.
//   ref - The reference to retain.
//
// Results:
//   None.
//
// Side effects:
//   The ref object is retained until a corresponding FbleRefRelease call is
//   made.
void FbleRefRetain(FbleRefArena* arena, FbleRef* ref)
{
  ref = CycleHead(ref);
  ref->refcount++;
}

// FbleRefRelease --
//   Release the given reference, cause the reference to be freed if there are
//   no outstanding references to it.
//
// Inputs:
//   arena - The reference arena the ref belongs to.
//   ref - The reference to release.
//
// Results:
//   None.
//
// Side effects:
//   The ref object is released. If there are no more references to it, the
//   ref object is freed.
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

// FbleRefAdd --
//   Add a reference from src to dst.
//
// Inputs:
//   arena - the reference arena.
//   src - the source node.
//   dst - the destination node.
//
// Results:
//   None.
//
// Side effects:
//   Adds a reference from the src node to the dst node, so that dst is
//   retained at least as long as src is retained.
//
// TODO: Clarify whether 'added' should include this reference from src to dst
// at the time of this call or not. I think the current implementation assumes
// 'added' does include the reference.
void FbleRefAdd(FbleRefArena* arena, FbleRef* src, FbleRef* dst)
{
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

static size_t sRefsAlive = 0;

typedef struct {
  FbleRef _base;
  FbleRefV added;
  int alive;
} Ref;

static void Free(FbleRefArena* arena, FbleRef* ref)
{
  assert(sRefsAlive > 0);
  sRefsAlive--;

  Ref* r = (Ref*)ref;
  r->alive = 0xDEAD;
  FbleFree(arena->arena, r->added.xs);
  FbleFree(arena->arena, r);
}

static bool Alive(Ref* ref)
{
  return ref->alive == 0XA11BE;
}

static void Added(FbleRefArena* arena, FbleRef* ref, FbleRefV* refs) {
  Ref* r = (Ref*)ref;
  for (size_t i = 0; i < r->added.size; ++i) {
    FbleVectorAppend(arena->arena, *refs, r->added.xs[i]);
  }
}

static Ref* Create(FbleRefArena* arena) {
  Ref* ref = FbleAlloc(arena->arena, Ref);
  FbleRefInit(arena, &ref->_base);
  FbleVectorInit(arena->arena, ref->added);
  ref->alive = 0xA11BE;
  sRefsAlive++;
  return ref;
}

static void RefAdd(FbleRefArena* arena, Ref* src, Ref* dst)
{
  FbleVectorAppend(arena->arena, src->added, &dst->_base);
  FbleRefAdd(arena, &src->_base, &dst->_base);
}

static void RefRelease(FbleRefArena* arena, Ref* ref)
{
  FbleRefRelease(arena, &ref->_base);
}

// main --
//   The main entry point for the fble-ref-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints an error to stderr and exits the program in the case of error.
int main(int argc, char* argv[])
{
  FbleArena* arena = FbleNewArena(NULL);
  FbleRefArena ref_arena = {
    .arena = arena,
    .next_id = 1,
    .next_round_id = 1,
    .free = &Free,
    .added = &Added,
  };

  {
    // Test a simple chain
    //   a -> b -> c
    Ref* c = Create(&ref_arena);

    Ref* b = Create(&ref_arena);
    RefAdd(&ref_arena, b, c);
    RefRelease(&ref_arena, c);

    Ref* a = Create(&ref_arena);
    RefAdd(&ref_arena, a, b);
    RefRelease(&ref_arena, b);

    // All three references should still be available.
    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(&ref_arena, a);
    assert(sRefsAlive == 0);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a really long chain, to make sure it doesn't involve smashing the
    // stack from a recursive implementation.
    //   a -> b -> ... -> n
    Ref* x = Create(&ref_arena);
    for (size_t i = 0; i < 1000000; ++i) {
      Ref* y = Create(&ref_arena);
      RefAdd(&ref_arena, y, x);
      RefRelease(&ref_arena, x);
      x = y;
    }
    RefRelease(&ref_arena, x);
    assert(sRefsAlive == 0);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test shared refs
    //   a --> b -> c
    //    \-> d >-/
    Ref* c = Create(&ref_arena);

    Ref* b = Create(&ref_arena);
    RefAdd(&ref_arena, b, c);
    RefRelease(&ref_arena, c);

    Ref* d = Create(&ref_arena);
    RefAdd(&ref_arena, d, c);

    Ref* a = Create(&ref_arena);
    RefAdd(&ref_arena, a, b);
    RefRelease(&ref_arena, b);
    RefAdd(&ref_arena, a, d);
    RefRelease(&ref_arena, d);

    // All references should still be available.
    assert(sRefsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    RefRelease(&ref_arena, a);
    assert(sRefsAlive == 0);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a cycle
    //  a --> b --> c
    //   \----<----/
    Ref* c = Create(&ref_arena);

    Ref* b = Create(&ref_arena);
    RefAdd(&ref_arena, b, c);
    RefRelease(&ref_arena, c);

    Ref* a = Create(&ref_arena);
    RefAdd(&ref_arena, a, b);
    RefRelease(&ref_arena, b);

    RefAdd(&ref_arena, c, a);

    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(&ref_arena, a);
    assert(sRefsAlive == 0);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a nested cycle
    //  a --> b --> c --> d --> e
    //   \     \----<----/     /
    //    \---------<---------/
    Ref* e = Create(&ref_arena);

    Ref* d = Create(&ref_arena);
    RefAdd(&ref_arena, d, e);
    RefRelease(&ref_arena, e);

    Ref* c = Create(&ref_arena);
    RefAdd(&ref_arena, c, d);
    RefRelease(&ref_arena, d);

    Ref* b = Create(&ref_arena);
    RefAdd(&ref_arena, b, c);
    RefRelease(&ref_arena, c);

    RefAdd(&ref_arena, d, b);

    Ref* a = Create(&ref_arena);
    RefAdd(&ref_arena, a, b);
    RefRelease(&ref_arena, b);

    RefAdd(&ref_arena, e, a);

    assert(sRefsAlive == 5);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));
    assert(Alive(e));

    RefRelease(&ref_arena, a);
    assert(sRefsAlive == 0);
    FbleAssertEmptyArena(arena);
  }

  FbleDeleteArena(arena);
  return 0;
}
