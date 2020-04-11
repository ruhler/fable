// fble-heap-test.c --
//   This file implements the main entry point for the fble-heap-test program.

#include <assert.h>   // for assert
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

static size_t sObjsAlive = 0;

typedef struct Obj Obj;

typedef struct ObjV {
  size_t size;
  Obj** xs;
} ObjV;

struct Obj {
  ObjV refs;
  int alive;
};

static void OnFree(FbleHeap* heap, Obj* obj)
{
  FbleArena* arena = heap->arena;

  assert(sObjsAlive > 0);
  sObjsAlive--;

  obj->alive = 0xDEAD;
  FbleFree(arena, obj->refs.xs);
}

static bool Alive(Obj* obj)
{
  return obj->alive == 0XA11BE;
}

static void Refs(FbleHeapCallback* callback, Obj* obj) {
  for (size_t i = 0; i < obj->refs.size; ++i) {
    callback->callback(callback, obj->refs.xs[i]);
  }
}

static Obj* Create(FbleHeap* heap) {
  FbleArena* arena = heap->arena;
  Obj* obj = heap->new(heap, sizeof(Obj));
  FbleVectorInit(arena, obj->refs);
  obj->alive = 0xA11BE;
  sObjsAlive++;
  return obj;
}

static void AddRef(FbleHeap* heap, Obj* src, Obj* dst)
{
  FbleVectorAppend(heap->arena, src->refs, dst);
  heap->add_ref(heap, src, dst);
}

static void Retain(FbleHeap* heap, Obj* obj)
{
  heap->retain(heap, obj);
}

static void Release(FbleHeap* heap, Obj* obj)
{
  heap->release(heap, obj);
}

// main --
//   The main entry point for the fble-heap-test program.
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
  FbleHeap* heap = FbleNewRefCountingHeap(arena,
      (void (*)(FbleHeapCallback*, void*))&Refs,
      (void (*)(FbleHeap*, void*))&OnFree);

  {
    // Test a simple chain
    //   a -> b -> c
    Obj* c = Create(heap);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);

    // All three references should still be available.
    assert(sObjsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    Release(heap, a);
    assert(sObjsAlive == 0);
  }

  {
    // Test a really long chain, to make sure it doesn't involve smashing the
    // stack from a recursive implementation.
    //   a -> b -> ... -> n
    Obj* x = Create(heap);
    for (size_t i = 0; i < 1000000; ++i) {
      Obj* y = Create(heap);
      AddRef(heap, y, x);
      Release(heap, x);
      x = y;
    }
    Release(heap, x);
    assert(sObjsAlive == 0);
  }

  {
    // Test shared refs
    //   a --> b -> c
    //    \-> d >-/
    Obj* c = Create(heap);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    Obj* d = Create(heap);
    AddRef(heap, d, c);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);
    AddRef(heap, a, d);
    Release(heap, d);

    // All references should still be available.
    assert(sObjsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    Release(heap, a);
    assert(sObjsAlive == 0);
  }

  {
    // Test a cycle
    //  a --> b --> c
    //   \----<----/
    Obj* c = Create(heap);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);

    AddRef(heap, c, a);

    assert(sObjsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    Release(heap, a);
    assert(sObjsAlive == 0);
  }

  {
    // Test a nested cycle
    //  a --> b --> c --> d --> e
    //   \     \----<----/     /
    //    \---------<---------/
    Obj* e = Create(heap);

    Obj* d = Create(heap);
    AddRef(heap, d, e);
    Release(heap, e);

    Obj* c = Create(heap);
    AddRef(heap, c, d);
    Release(heap, d);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    AddRef(heap, d, b);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);

    AddRef(heap, e, a);

    assert(sObjsAlive == 5);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));
    assert(Alive(e));

    Release(heap, a);
    assert(sObjsAlive == 0);
  }

  {
    // Test a cycle with multiple separate external references.
    //  --> a --> b --> c <--
    //       \-<--d-<--/
    Obj* d = Create(heap);

    Obj* c = Create(heap);
    AddRef(heap, c, d);
    Release(heap, d);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);

    AddRef(heap, d, a);
    Retain(heap, c);
    Release(heap, a);

    assert(sObjsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    Release(heap, c);
    assert(sObjsAlive == 0);
  }

  {
    // Test a reverse chain
    //   a <- b <- c
    Obj* c = Create(heap);

    Obj* b = Create(heap);
    AddRef(heap, c, b);
    Release(heap, b);

    Obj* a = Create(heap);
    AddRef(heap, b, a);
    Release(heap, a);

    // All three references should still be available.
    assert(sObjsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    Release(heap, c);
    assert(sObjsAlive == 0);
  }

  {
    // Test a reverse cycle
    //  a <-- b <-- c
    //   \---->----/
    Obj* c = Create(heap);

    Obj* b = Create(heap);
    AddRef(heap, c, b);
    Release(heap, b);

    Obj* a = Create(heap);
    AddRef(heap, b, a);
    Release(heap, a);

    AddRef(heap, a, c);

    assert(sObjsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    Release(heap, c);
    assert(sObjsAlive == 0);
  }

  {
    // Test a cycle that has an external reference
    //  a --> b --> c --> d
    //   \----<----/
    Obj* d = Create(heap);

    Obj* c = Create(heap);
    AddRef(heap, c, d);
    Release(heap, d);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);

    AddRef(heap, c, a);

    assert(sObjsAlive == 4);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));
    assert(Alive(d));

    Release(heap, a);
    assert(sObjsAlive == 0);
  }

  {
    // Test a cycle with a bunch of internal references.
    //   /---->----\_
    //  a --> b --> c
    //  \\-<-/ \-<-//
    //   \----<----/
    Obj* c = Create(heap);

    Obj* b = Create(heap);
    AddRef(heap, b, c);
    Release(heap, c);

    Obj* a = Create(heap);
    AddRef(heap, a, b);
    Release(heap, b);

    AddRef(heap, c, a);
    AddRef(heap, c, b);
    AddRef(heap, b, a);

    assert(sObjsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    Release(heap, a);
    assert(sObjsAlive == 0);
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
    Obj* a = Create(heap);
    Obj* b = Create(heap);
    Obj* c = Create(heap);
    AddRef(heap, a, c);
    AddRef(heap, a, b);
    Release(heap, b);
    Release(heap, c);

    AddRef(heap, b, c);
    AddRef(heap, c, a);

    assert(sObjsAlive == 3);
    assert(Alive(a));
    assert(Alive(b));
    assert(Alive(c));

    Release(heap, a);
    assert(sObjsAlive == 0);
  }

  FbleDeleteRefCountingHeap(heap);
  FbleAssertEmptyArena(arena);
  FbleDeleteArena(arena);
  return 0;
}
