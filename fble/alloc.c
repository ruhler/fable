// alloc.c --
//   This file implements the fble allocation routines.

#include <stdio.h>    // for fprintf, stderr
#include <string.h>   // for memset

#include "fble.h"

// Alloc --
//   A circularly doubly linked list of arena allocations.
typedef struct Alloc {
  struct Alloc* prev;
  struct Alloc* next;
  const char* msg;
  size_t size;
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
  alloc->size = size;
  return (void*)alloc->data;
}

// FbleFree -- see documentation in fble.h
void FbleFree(FbleArena* arena, void* ptr)
{
  assert(arena != NULL);

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);

  // For debugging purposes, check that the allocation actually exists on the
  // arena.
  bool found = false;
  for (Alloc* a = arena->allocs->next; a != arena->allocs; a = a->next) {
    if (alloc == a) {
      found = true;
      break;
    }
  }
  assert(found && "FbleFree on bad ptr");

  alloc->next->prev = alloc->prev;
  alloc->prev->next = alloc->next;

  // Poison the data to help catch use after free.
  free(memset(alloc, 0xDD, sizeof(Alloc) + alloc->size));
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

  Arena* a = (Arena*)(((Arena**)arena) - 2);
  assert(arena == &(a->arena));

  if (a->next != NULL) {
    a->next->prev = a->prev;
    a->prev->next = a->next;
  }
  free(a);
}

// FbleAssertEmptyArena -- see documentation in fble.h
void FbleAssertEmptyArena(FbleArena* arena)
{
  if (arena->allocs->next != arena->allocs) {
    fprintf(stderr, "the following allocations are outstanding:\n");
    for (Alloc* alloc = arena->allocs->next; alloc != arena->allocs; alloc = alloc->next) {
      fprintf(stderr, "  %s %p %zi bytes\n", alloc->msg, alloc->data, alloc->size);
    }
    abort();
  }

  if (arena->arenas->next != arena->arenas) {
    fprintf(stderr, "the arena has outstanding child arenas\n");
    abort();
  }
}
