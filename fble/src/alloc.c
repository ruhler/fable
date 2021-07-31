// alloc.c --
//   This file implements the fble allocation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for malloc

#include "fble-alloc.h"

// gTotalBytesAllocated --
//   The total number of bytes currently allocated via FbleAlloc routines.
size_t gTotalBytesAllocated = 0;

// gMaxTotalBytesAllocated --
//   The maximum value of gTotalBytesAllocated since gMaxTotalBytesAllocated
//   was last cleared.
size_t gMaxTotalBytesAllocated = 0;

// gInititialized --
//   True if the exit routine has been registered.
bool gInitialized = false;

typedef struct {
  size_t size;
  char data[];
} Alloc;

static void Exit();


// Exit --
//   Clean up the global arena.
//
// Side effects:
// * Frees resources associated with the global arena.
// * Checks for memory leaks.
void Exit()
{
  if (gTotalBytesAllocated != 0) {
    fprintf(stderr, "ERROR: MEMORY LEAK DETECTED\n");
    fprintf(stderr, "Try running again using: valgrind --leak-check=full\n");
    abort();
  }
}

// FbleRawAlloc -- see documentation in fble-alloc.h
void* FbleRawAlloc(size_t size, const char* msg)
{
  if (!gInitialized) {
    gInitialized = true;
    atexit(&Exit);
  }

  gTotalBytesAllocated += size;
  if (gTotalBytesAllocated > gMaxTotalBytesAllocated) {
    gMaxTotalBytesAllocated = gTotalBytesAllocated;
  }

  Alloc* alloc = malloc(sizeof(Alloc) + size);
  alloc->size = size;
  return (void*)alloc->data;
}

// FbleFree -- see documentation in fble-alloc.h
void FbleFree(void* ptr)
{
  if (ptr == NULL) {
    return;
  }

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);

  gTotalBytesAllocated -= alloc->size;
  free(alloc);
}

// FbleMaxTotalBytesAllocated -- see documentation in fble-alloc.h
size_t FbleMaxTotalBytesAllocated()
{
  return gMaxTotalBytesAllocated;
}

// FbleResetMaxTotalBytesAllocated -- see documentation in fble-alloc.h
void FbleResetMaxTotalBytesAllocated()
{
  gMaxTotalBytesAllocated = gTotalBytesAllocated;
}
