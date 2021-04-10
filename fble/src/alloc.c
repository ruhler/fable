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

// FbleArena -- See fble-alloc.h for documentation.
//
// Fields:
//   allocs - The list of allocations for this arena. It is a circular, doubly
//            linked list with the first node a dummy node.
//   size - the sum of the sizes of current allocations.
//   max_size - the maximum historic sum of the sizes of allocations.
struct FbleArena {
  struct Alloc* allocs;
  size_t size;
  size_t max_size;
};


// FbleRawAlloc -- see documentation in fble-alloc.h
void* FbleRawAlloc(FbleArena* arena, size_t size, const char* msg)
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

  arena->size += size;
  arena->max_size = arena->size > arena->max_size ? arena->size : arena->max_size;
  return (void*)alloc->data;
}

// FbleFree -- see documentation in fble-alloc.h
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

  arena->size -= alloc->size;

  // Poison the data to help catch use after free.
  free(memset(alloc, 0xDD, sizeof(Alloc) + alloc->size));
}

// FbleNewArena -- see documentation in fble-alloc.h
FbleArena* FbleNewArena()
{
  FbleArena* arena = malloc(sizeof(FbleArena));
  arena->allocs = malloc(sizeof(Alloc));
  arena->allocs->next = arena->allocs;
  arena->allocs->prev = arena->allocs;
  arena->allocs->msg = "?dummy?";

  arena->size = 0;
  arena->max_size = 0;
  return arena;
}

// FbleFreeArena -- see documentation in fble-alloc.h
void FbleFreeArena(FbleArena* arena)
{
  // Check for memory leaks.
  if (arena->allocs->next != arena->allocs) {
    fprintf(stderr, "the following allocations are outstanding:\n");
    for (Alloc* alloc = arena->allocs->next; alloc != arena->allocs; alloc = alloc->next) {
      fprintf(stderr, "  %s %p %zi bytes\n", alloc->msg, alloc->data, alloc->size);
    }
    abort();
  }

  while (arena->allocs->next != arena->allocs) {
    FbleFree(arena, arena->allocs->next->data);
  }
  free(arena->allocs);
  free(arena);
}

// FbleArenaMaxSize -- see documentation in fble-alloc.h
size_t FbleArenaMaxSize(FbleArena* arena)
{
  return arena->max_size;
}
