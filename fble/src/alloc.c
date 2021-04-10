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

// FbleArena
//   Tracks all the current fble based memory allocations.
//
// Fields:
//   allocs - The list of allocations. It is a circular, doubly
//            linked list with the first node a dummy node.
//   size - the sum of the sizes of current allocations.
//   max_size - the maximum historic sum of the sizes of allocations.
typedef struct FbleArena {
  struct Alloc* allocs;
  size_t size;
  size_t max_size;
} FbleArena;

FbleArena* gArena = NULL;

static void Init();
static void Exit();


// Init --
//   Initialize the global arena and register the Exit function.
//
// Side Effects:
// * Initialize gArena and registers the Exit function to be called at process
// exit.
void Init()
{
  assert(gArena == NULL);

  gArena = malloc(sizeof(FbleArena));
  gArena->allocs = malloc(sizeof(Alloc));
  gArena->allocs->next = gArena->allocs;
  gArena->allocs->prev = gArena->allocs;
  gArena->allocs->msg = "?dummy?";

  gArena->size = 0;
  gArena->max_size = 0;

  atexit(&Exit);
}

// Exit --
//   Clean up the global arena.
//
// Side effects:
// * Frees resources associated with the global arena.
// * Checks for memory leaks.
void Exit()
{
  assert(gArena != NULL);

  // Check for memory leaks.
  if (gArena->allocs->next != gArena->allocs) {
    fprintf(stderr, "the following allocations are outstanding:\n");
    for (Alloc* alloc = gArena->allocs->next; alloc != gArena->allocs; alloc = alloc->next) {
      fprintf(stderr, "  %s %p %zi bytes\n", alloc->msg, alloc->data, alloc->size);
    }
    abort();
  }

  free(gArena->allocs);
  free(gArena);
  gArena = NULL;
}

// FbleRawAlloc -- see documentation in fble-alloc.h
void* FbleRawAlloc(size_t size, const char* msg)
{
  if (gArena == NULL) {
    Init();
  }

  Alloc* alloc = malloc(sizeof(Alloc) + size);
  alloc->prev = gArena->allocs;
  alloc->next = gArena->allocs->next;
  alloc->prev->next = alloc;
  alloc->next->prev = alloc;
  alloc->msg = msg;
  alloc->size = size;
  alloc->arena = gArena;

  gArena->size += size;
  gArena->max_size = gArena->size > gArena->max_size ? gArena->size : gArena->max_size;
  return (void*)alloc->data;
}

// FbleFree -- see documentation in fble-alloc.h
void FbleFree(void* ptr)
{
  assert(gArena != NULL && "FbleFree called before first allocation");

  if (ptr == NULL) {
    return;
  }

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);
  assert(alloc->arena == gArena && "FbleFree on bad ptr");

  alloc->next->prev = alloc->prev;
  alloc->prev->next = alloc->next;

  gArena->size -= alloc->size;

  // Poison the data to help catch use after free.
  free(memset(alloc, 0xDD, sizeof(Alloc) + alloc->size));
}

// FbleMaxTotalBytesAllocated -- see documentation in fble-alloc.h
size_t FbleMaxTotalBytesAllocated()
{
  return gArena == NULL ? 0 : gArena->max_size;
}

// FbleResetMaxTotalBytesAllocated -- see documentation in fble-alloc.h
void FbleResetMaxTotalBytesAllocated()
{
  if (gArena != NULL) {
    gArena->max_size = gArena->size;
  }
}
