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
// Fields:
//   id - A unique identifier for the node. Ids are assigned in increasing
//        order of node allocation.
//   refcount - The number of references to this node. 0 if the node
//              belongs to a cycle.
//   cycle - A pointer to the head of the cycle this node belongs to. NULL
//           if the node does not belong to a cycle.
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
  if (ref->cycle != NULL) {
    assert(ref->cycle->cycle == NULL);
    ref = ref->cycle;
  }
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
  FbleVectorAppend(arena->arena, refs, ref);

  while (refs.size > 0) {
    FbleRef* r = refs.xs[--refs.size];

    if (r->cycle != NULL) {
      r = r->cycle;
    }

    assert(r->refcount > 0);
    r->refcount--;
    if (r->refcount == 0) {
      if (r->cycle == NULL) {
        arena->added(arena, r, &refs);
        arena->free(arena, r);
      } else {
        size_t round = arena->next_round_id++;
        FbleRefV stack;
        FbleVectorInit(arena->arena, stack);
        FbleRefV in_cycle;
        FbleVectorInit(arena->arena, in_cycle);

        r->round_id = round;
        FbleVectorAppend(arena->arena, stack, r);

        while (stack.size > 0) {
          FbleRef* node = stack.xs[--stack.size];
          FbleVectorAppend(arena->arena, in_cycle, node);

          FbleRefV children;
          FbleVectorInit(arena->arena, children);
          arena->added(arena, node, &children);
          for (size_t i = 0; i < children.size; ++i) {
            FbleRef* child = children.xs[i];
            if (child->cycle == r) {
              if (child->round_id != round) {
                child->round_id = round;
                FbleVectorAppend(arena->arena, stack, child);
              }
            } else {
              FbleVectorAppend(arena->arena, refs, child);
            }
          }
          FbleFree(arena->arena, children.xs);
        }

        for (size_t i = 0; i < in_cycle.size; ++i) {
          arena->free(arena, in_cycle.xs[i]);
        }

        FbleFree(arena->arena, stack.xs);
        FbleFree(arena->arena, in_cycle.xs);
      }
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

  while (stack.size > 0) {
    FbleRef* ref = stack.xs[stack.size - 1];
    assert(ref->cycle == NULL && "TODO: Handle this case.");
    assert(ref->round_id == round);

    FbleRefV children;
    FbleVectorInit(arena->arena, children);
    arena->added(arena, ref, &children);

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
      for (size_t i = 0; !in_cycle && i < children.size; ++i) {
        in_cycle = (children.xs[i]->cycle == dst);
      }

      if (in_cycle) {
        ref->cycle = dst;
      }
    }

    FbleFree(arena->arena, children.xs);
  }

  if (dst->cycle == dst) {
    // Fix up refcounts for the cycle by moving any refs from nodes outside the
    // cycle to the head node of the cycle and removing all refs from nodes
    // inside the cycle.
    size_t total = 0;
    size_t internal = 0;

    // Note: We repurpose round_new to indicate we have processed this node.
    // TODO: Pick a better name for round_new.
    dst->round_new = true;  
    FbleVectorAppend(arena->arena, stack, dst);
    while (stack.size > 0) {
      FbleRef* ref = stack.xs[--stack.size];
      total += ref->refcount;
      ref->refcount = 0;

      FbleRefV children;
      FbleVectorInit(arena->arena, children);
      arena->added(arena, ref, &children);

      for (size_t i = 0; i < children.size; ++i) {
        FbleRef* child = children.xs[i];
        if (child->cycle == dst) {
          internal++;
          if (!child->round_new) {
            child->round_new = true;
            FbleVectorAppend(arena->arena, stack, child);
          }
        }
      }

      FbleFree(arena->arena, children.xs);
    }

    dst->refcount = total - internal;
  }

  FbleFree(arena->arena, stack.xs);
}

typedef struct {
  FbleRef _base;
  FbleRefV added;
  int alive;
} Ref;

static void Free(FbleRefArena* arena, FbleRef* ref)
{
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
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(&ref_arena, a);
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
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    RefRelease(&ref_arena, a);
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

    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(&ref_arena, a);
    FbleAssertEmptyArena(arena);
  }

  FbleDeleteArena(arena);
  return 0;
}
