/**
 * @file alloc.c
 *  Implementation of fble allocation routines.
 */

#define _GNU_SOURCE   // for MAP_ANONYMOUS

#include <fble/fble-alloc.h>

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for malloc
#include <sys/mman.h> // for mmap, munmap
#include <unistd.h>   // for sysconf


/** Total number of bytes currently allocated via FbleAlloc routines. */
size_t gTotalBytesAllocated = 0;

/**
 * The maximum value of gTotalBytesAllocated since gMaxTotalBytesAllocated
 * was last cleared.
 */
size_t gMaxTotalBytesAllocated = 0;

/** True if the exit routine has been registered. */
bool gInitialized = false;

/**
 * An allocation.
 */
typedef struct {
  size_t size;      /**< Size of the allocation. */
  char data[];      /**< Allocated memory region available to the user. */
} Alloc;

static void CountAlloc(size_t size);
static void CountFree(size_t size);
static void Exit();

/**
 * Updates allocation metrics for a new allocation.
 *
 * @param size  The size of the new allocation.
 *
 * @sideeffects
 * * Initializes the allocation atExit routine if appropriate.
 * * Updates gTotalBytesAllocated and gMaxTotalBytesAllocated.
 */
static void CountAlloc(size_t size)
{
  if (!gInitialized) {
    gInitialized = true;
    atexit(&Exit);
  }

  gTotalBytesAllocated += size;
  if (gTotalBytesAllocated > gMaxTotalBytesAllocated) {
    gMaxTotalBytesAllocated = gTotalBytesAllocated;
  }
}

/**
 * Updates allocation tracking for a freed allocation.
 *
 * @param size  The size of the freed allocation.
 *
 * @sideeffects
 * * Updates gTotalBytesAllocated.
 */
static void CountFree(size_t size)
{
  gTotalBytesAllocated -= size;
}

/**
 * Cleans up the global arena.
 *
 * @sideeffects
 * * Frees resources associated with the global arena.
 * * Checks for memory leaks.
 */
static void Exit()
{
  if (gTotalBytesAllocated != 0) {
    fprintf(stderr, "ERROR: MEMORY LEAK DETECTED\n");
    fprintf(stderr, "Try running again using: valgrind --leak-check=full\n");
    abort();
  }
}

// See documentation in fble-alloc.h.
void* FbleAllocRaw(size_t size)
{
  CountAlloc(size);
  Alloc* alloc = malloc(sizeof(Alloc) + size);
  alloc->size = size;
  return (void*)alloc->data;
}

// See documentation in fble-alloc.h.
void FbleFree(void* ptr)
{
  if (ptr == NULL) {
    return;
  }

  Alloc* alloc = ((Alloc*)ptr) - 1;
  assert(ptr == (void*)alloc->data);
  CountFree(alloc->size);
  free(alloc);
}

// See documentation in fble-alloc.h.
size_t FbleMaxTotalBytesAllocated()
{
  return gMaxTotalBytesAllocated;
}

// See documentation in fble-alloc.h.
void FbleResetMaxTotalBytesAllocated()
{
  gMaxTotalBytesAllocated = gTotalBytesAllocated;
}
