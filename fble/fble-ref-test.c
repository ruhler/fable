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
struct FbleRef {
  size_t id;
  size_t refcount;
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
  ref->refcount++;
}

// FbleRefRetain --
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
  assert(ref->refcount > 0);
  ref->refcount--;
  if (ref->refcount == 0) {
    // TODO: Use iteration instead of recursion to avoid smashing the stack
    // for long reference chains.
    FbleRefV added;
    FbleVectorInit(arena->arena, added);
    arena->added(arena, ref, &added);
    for (size_t i = 0; i < added.size; ++i) {
      FbleRefRelease(arena, added.xs[i]);
    }
    FbleFree(arena->arena, added.xs);
    arena->free(arena, ref);
  }
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
void FbleRefAdd(FbleRefArena* arena, FbleRef* src, FbleRef* dst)
{
  FbleRefRetain(arena, dst);
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
    .free = &Free,
    .added = &Added,
  };

  {
    // 1. Test a simple chain a -> b -> c
    Ref* a = Create(&ref_arena);
    Ref* b = Create(&ref_arena);
    Ref* c = Create(&ref_arena);

    RefAdd(&ref_arena, a, b);
    RefAdd(&ref_arena, b, c);
    RefRelease(&ref_arena, c);
    RefRelease(&ref_arena, b);

    // All three references should still be available.
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(&ref_arena, a);
    FbleAssertEmptyArena(arena);
  }

  FbleDeleteArena(arena);
  return 0;
}
