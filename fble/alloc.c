// alloc.c --
//   This file implements the fble allocation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for abort, malloc, free

#include "fble.h"

// Alloc --
//   A circularly doubly linked list of arena allocations.
typedef struct Alloc {
  struct Alloc* prev;
  struct Alloc* next;
  const char* msg;
  char data[];
} Alloc;

// FbleArena -- See fble.h for documentation.
//
// Fields:
//   allocs - The list of allocations for this arena. It is a circular, doubly
//            linked list with the first node a dummy node.
//   arenas - The list of child arenas for this arena. It is a circular,
//            doubly linked list with the first node a dummy node.
struct FbleArena {
  struct Alloc* allocs;
  struct Arena* arenas;
};

// Arena --
//   A circularly doubly linked list of child arenas.
typedef struct Arena {
  struct Arena* prev;
  struct Arena* next;
  struct FbleArena arena;
} Arena;


// FbleArenaAlloc -- see documentation in fble.h
void* FbleArenaAlloc(FbleArena* arena, size_t size, const char* msg)
{
  assert(arena != NULL);
  Alloc* alloc = malloc(sizeof(Alloc) + size);
  alloc->prev = arena->allocs;
  alloc->next = arena->allocs->next;
  alloc->prev->next = alloc;
  alloc->next->prev = alloc;
  alloc->msg = msg;
  return (void*)alloc->data;
}

// FbleFree -- see documentation in fble.h
void FbleFree(FbleArena* arena, void* ptr)
{
  assert(arena != NULL);

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);

  alloc->next->prev = alloc->prev;
  alloc->prev->next = alloc->next;
  free(alloc);
}

// FbleNewArena -- see documentation in fble.h
FbleArena* FbleNewArena(FbleArena* parent)
{
  Arena* arena = malloc(sizeof(Arena));
  arena->prev = NULL;
  arena->next = NULL;
  if (parent != NULL) {
    arena->prev = parent->arenas;
    arena->next = parent->arenas->next;
    arena->prev->next = arena;
    arena->next->prev = arena;
  }

  arena->arena.allocs = malloc(sizeof(Alloc));
  arena->arena.allocs->next = arena->arena.allocs;
  arena->arena.allocs->prev = arena->arena.allocs;
  arena->arena.allocs->msg = "?dummy?";

  arena->arena.arenas = malloc(sizeof(Arena));
  arena->arena.arenas->next = arena->arena.arenas;
  arena->arena.arenas->prev = arena->arena.arenas;
  arena->arena.arenas->arena.allocs = NULL;
  arena->arena.arenas->arena.arenas = NULL;
  return &(arena->arena);
}

// FbleDeleteArena -- see documentation in fble.h
void FbleDeleteArena(FbleArena* arena)
{
  while (arena->allocs->next != arena->allocs) {
    FbleFree(arena, arena->allocs->next->data);
  }
  free(arena->allocs);

  while (arena->arenas->next != arena->arenas) {
    FbleDeleteArena(&(arena->arenas->next->arena));
  }
  free(arena->arenas);

  Arena* a = ((Arena*)arena) - 2;
  assert(arena == &a->arena);

  a->next->prev = a->prev;
  a->prev->next = a->next;
  free(a);
}

// FbleAssertEmptyArena -- see documentation in fble.h
void FbleAssertEmptyArena(FbleArena* arena)
{
  if (arena->allocs->next != arena->allocs) {
    fprintf(stderr, "the following allocations are outstanding:\n");
    for (Alloc* alloc = arena->allocs->next; alloc != arena->allocs; alloc = alloc->next) {
      fprintf(stderr, "  %s\n", alloc->msg);
    }
    abort();
  }

  if (arena->arenas->next != arena->arenas) {
    fprintf(stderr, "the arena has outstanding child arenas\n");
    abort();
  }
}
