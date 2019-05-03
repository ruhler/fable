// alloc.c --
//   This file implements the fble allocation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for malloc
#include <string.h>   // for memset

#include "fble-alloc.h"

// Alloc --
//   A circularly doubly linked list of arena allocations.
typedef struct Alloc {
  struct Alloc* prev;
  struct Alloc* next;

  // msg, size, and arena are all for debugging purposes only.
  const char* msg;
  size_t size;
  struct FbleArena* arena;

  // The allocation returned to the caller.
  char data[];
} Alloc;

// FbleArena -- See fble-arena.h for documentation.
//
// Fields:
//   allocs - The list of allocations for this arena. It is a circular, doubly
//            linked list with the first node a dummy node.
//   parent - the parent arena for this arena, or NULL.
//   children - The list of child arenas for this arena. It is a circular,
//              doubly linked list with the first node a dummy node.
//   size - the sum of the sizes of current allocations, including allocations
//          on child arenas.
//   max_size - the maximum historic sum of the sizes of allocations.
struct FbleArena {
  struct Alloc* allocs;
  struct FbleArena* parent;
  struct Arena* children;
  size_t size;
  size_t max_size;
};

// Arena --
//   A circularly doubly linked list of child arenas.
typedef struct Arena {
  struct Arena* prev;
  struct Arena* next;
  struct FbleArena arena;
} Arena;


// FbleArenaAlloc -- see documentation in fble-arena.h
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
  alloc->arena = arena;

  while (arena != NULL) {
    arena->size += size;
    arena->max_size = arena->size > arena->max_size ? arena->size : arena->max_size;
    arena = arena->parent;
  }

  return (void*)alloc->data;
}

// FbleFree -- see documentation in fble-arena.h
void FbleFree(FbleArena* arena, void* ptr)
{
  if (ptr == NULL) {
    return;
  }

  assert(arena != NULL);

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);
  assert(alloc->arena == arena && "FbleFree on bad ptr");

  alloc->next->prev = alloc->prev;
  alloc->prev->next = alloc->next;

  while (arena != NULL) {
    arena->size -= alloc->size;
    arena = arena->parent;
  }

  // Poison the data to help catch use after free.
  free(memset(alloc, 0xDD, sizeof(Alloc) + alloc->size));
}

// FbleNewArena -- see documentation in fble-arena.h
FbleArena* FbleNewArena(FbleArena* parent)
{
  Arena* arena = malloc(sizeof(Arena));
  arena->prev = NULL;
  arena->next = NULL;
  if (parent != NULL) {
    arena->prev = parent->children;
    arena->next = parent->children->next;
    arena->prev->next = arena;
    arena->next->prev = arena;
  }

  arena->arena.allocs = malloc(sizeof(Alloc));
  arena->arena.allocs->next = arena->arena.allocs;
  arena->arena.allocs->prev = arena->arena.allocs;
  arena->arena.allocs->msg = "?dummy?";

  arena->arena.parent = parent;

  arena->arena.children = malloc(sizeof(Arena));
  arena->arena.children->next = arena->arena.children;
  arena->arena.children->prev = arena->arena.children;
  arena->arena.children->arena.allocs = NULL;
  arena->arena.children->arena.children = NULL;

  arena->arena.size = 0;
  arena->arena.max_size = 0;
  return &(arena->arena);
}

// FbleDeleteArena -- see documentation in fble-arena.h
void FbleDeleteArena(FbleArena* arena)
{
  while (arena->allocs->next != arena->allocs) {
    FbleFree(arena, arena->allocs->next->data);
  }
  free(arena->allocs);

  while (arena->children->next != arena->children) {
    FbleDeleteArena(&(arena->children->next->arena));
  }
  free(arena->children);

  Arena* a = (Arena*)(((Arena**)arena) - 2);
  assert(arena == &(a->arena));

  if (a->next != NULL) {
    a->next->prev = a->prev;
    a->prev->next = a->next;
  }
  free(a);
}

// FbleAssertEmptyArena -- see documentation in fble-arena.h
void FbleAssertEmptyArena(FbleArena* arena)
{
  if (arena->allocs->next != arena->allocs) {
    fprintf(stderr, "the following allocations are outstanding:\n");
    for (Alloc* alloc = arena->allocs->next; alloc != arena->allocs; alloc = alloc->next) {
      fprintf(stderr, "  %s %p %zi bytes\n", alloc->msg, alloc->data, alloc->size);
    }
    abort();
  }

  if (arena->children->next != arena->children) {
    fprintf(stderr, "the arena has outstanding child arenas\n");
    abort();
  }
}

// FbleArenaMaxSize -- see documentation in fble-arena.h
size_t FbleArenaMaxSize(FbleArena* arena)
{
  return arena->max_size;
}
