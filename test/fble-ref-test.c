// fble-ref-test.c --
//   This file implements the main entry point for the fble-ref-test program.

#include <assert.h>   // for assert
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"
#include "ref.h"

static size_t sRefsAlive = 0;

typedef struct {
  FbleRef _base;
  FbleRefV added;
  int alive;
} Ref;

static void Free(FbleRefArena* arena, FbleRef* ref)
{
  FbleArena* arena_ = FbleRefArenaArena(arena);

  assert(sRefsAlive > 0);
  sRefsAlive--;

  Ref* r = (Ref*)ref;
  r->alive = 0xDEAD;
  FbleFree(arena_, r->added.xs);
  FbleFree(arena_, r);
}

static bool Alive(Ref* ref)
{
  return ref->alive == 0XA11BE;
}

static void Added(FbleRefCallback* add, FbleRef* ref) {
  Ref* r = (Ref*)ref;
  for (size_t i = 0; i < r->added.size; ++i) {
    add->callback(add, r->added.xs[i]);
  }
}

static Ref* Create(FbleRefArena* arena) {
  FbleArena* arena_ = FbleRefArenaArena(arena);
  Ref* ref = FbleAlloc(arena_, Ref);
  FbleRefInit(arena, &ref->_base);
  FbleVectorInit(arena_, ref->added);
  ref->alive = 0xA11BE;
  sRefsAlive++;
  return ref;
}

static void RefAdd(FbleRefArena* arena, Ref* src, Ref* dst)
{
  FbleVectorAppend(FbleRefArenaArena(arena), src->added, &dst->_base);
  FbleRefAdd(arena, &src->_base, &dst->_base);
}

static void RefRetain(FbleRefArena* arena, Ref* ref)
{
  FbleRefRetain(arena, &ref->_base);
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
  FbleArena* arena = FbleNewArena();
  FbleRefArena* ref_arena = FbleNewRefArena(arena, &Free, &Added);

  {
    // Test a simple chain
    //   a -> b -> c
    Ref* c = Create(ref_arena);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);

    // All three references should still be available.
    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  {
    // Test a really long chain, to make sure it doesn't involve smashing the
    // stack from a recursive implementation.
    //   a -> b -> ... -> n
    Ref* x = Create(ref_arena);
    for (size_t i = 0; i < 1000000; ++i) {
      Ref* y = Create(ref_arena);
      RefAdd(ref_arena, y, x);
      RefRelease(ref_arena, x);
      x = y;
    }
    RefRelease(ref_arena, x);
    assert(sRefsAlive == 0);
  }

  {
    // Test shared refs
    //   a --> b -> c
    //    \-> d >-/
    Ref* c = Create(ref_arena);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    Ref* d = Create(ref_arena);
    RefAdd(ref_arena, d, c);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);
    RefAdd(ref_arena, a, d);
    RefRelease(ref_arena, d);

    // All references should still be available.
    assert(sRefsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  {
    // Test a cycle
    //  a --> b --> c
    //   \----<----/
    Ref* c = Create(ref_arena);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);

    RefAdd(ref_arena, c, a);

    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  {
    // Test a nested cycle
    //  a --> b --> c --> d --> e
    //   \     \----<----/     /
    //    \---------<---------/
    Ref* e = Create(ref_arena);

    Ref* d = Create(ref_arena);
    RefAdd(ref_arena, d, e);
    RefRelease(ref_arena, e);

    Ref* c = Create(ref_arena);
    RefAdd(ref_arena, c, d);
    RefRelease(ref_arena, d);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    RefAdd(ref_arena, d, b);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);

    RefAdd(ref_arena, e, a);

    assert(sRefsAlive == 5);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));
    assert(Alive(e));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  {
    // Test a cycle with multiple separate external references.
    //  --> a --> b --> c <--
    //       \-<--d-<--/
    Ref* d = Create(ref_arena);

    Ref* c = Create(ref_arena);
    RefAdd(ref_arena, c, d);
    RefRelease(ref_arena, d);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);

    RefAdd(ref_arena, d, a);
    RefRetain(ref_arena, c);
    RefRelease(ref_arena, a);

    assert(sRefsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    RefRelease(ref_arena, c);
    assert(sRefsAlive == 0);
  }

  {
    // Test a reverse chain
    //   a <- b <- c
    Ref* c = Create(ref_arena);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, c, b);
    RefRelease(ref_arena, b);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, b, a);
    RefRelease(ref_arena, a);

    // All three references should still be available.
    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(ref_arena, c);
    assert(sRefsAlive == 0);
  }

  {
    // Test a reverse cycle
    //  a <-- b <-- c
    //   \---->----/
    Ref* c = Create(ref_arena);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, c, b);
    RefRelease(ref_arena, b);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, b, a);
    RefRelease(ref_arena, a);

    RefAdd(ref_arena, a, c);

    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(ref_arena, c);
    assert(sRefsAlive == 0);
  }

  {
    // Test a cycle that has an external reference
    //  a --> b --> c --> d
    //   \----<----/
    Ref* d = Create(ref_arena);

    Ref* c = Create(ref_arena);
    RefAdd(ref_arena, c, d);
    RefRelease(ref_arena, d);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);

    RefAdd(ref_arena, c, a);

    assert(sRefsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  {
    // Test a cycle with a bunch of internal references.
    //   /---->----\_
    //  a --> b --> c
    //  \\-<-/ \-<-//
    //   \----<----/
    Ref* c = Create(ref_arena);

    Ref* b = Create(ref_arena);
    RefAdd(ref_arena, b, c);
    RefRelease(ref_arena, c);

    Ref* a = Create(ref_arena);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);

    RefAdd(ref_arena, c, a);
    RefAdd(ref_arena, c, b);
    RefAdd(ref_arena, b, a);

    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  {
    // Regression test.
    //  a -->--b->\.
    //   \-->------c
    //    \---<---/
    //
    // This triggered a bug when adding the final reference from c to a. We
    // push c on the stack for processing, process b assuming c will be fine,
    // but because we haven't processed c yet by the time we are done we b, we
    // failed to recognize that b belongs to the cycle as well.
    Ref* a = Create(ref_arena);
    Ref* b = Create(ref_arena);
    Ref* c = Create(ref_arena);
    RefAdd(ref_arena, a, c);
    RefAdd(ref_arena, a, b);
    RefRelease(ref_arena, b);
    RefRelease(ref_arena, c);

    RefAdd(ref_arena, b, c);
    RefAdd(ref_arena, c, a);

    assert(sRefsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    RefRelease(ref_arena, a);
    assert(sRefsAlive == 0);
  }

  FbleDeleteRefArena(ref_arena);
  FbleAssertEmptyArena(arena);
  FbleDeleteArena(arena);
  return 0;
}
