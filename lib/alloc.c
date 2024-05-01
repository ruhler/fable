/**
 * @file alloc.c
 *  Implementation of fble allocation routines.
 */

#include <fble/fble-alloc.h>

#include <stdio.h>    // for fprintf, stderr
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for malloc

/**
 * The number of allocations that haven't been freed yet.
 *
 * Used for detecting memory leaks.
 */
size_t gCurrentAllocationsCount = 0;


/** True if the exit routine has been registered. */
bool gInitialized = false;

static void Exit();

/**
 * Cleans up the global arena.
 *
 * @sideeffects
 * * Frees resources associated with the global arena.
 * * Checks for memory leaks.
 */
static void Exit()
{
  if (gCurrentAllocationsCount != 0) {
    fprintf(stderr, "ERROR: MEMORY LEAK DETECTED\n");
    fprintf(stderr, "Try running again using: valgrind --leak-check=full\n");
    abort();
  }
}

// See documentation in fble-alloc.h.
void* FbleAllocRaw(size_t size)
{
  if (!gInitialized) {
    gInitialized = true;
    atexit(&Exit);
  }
  gCurrentAllocationsCount++;
  return malloc(size);
}
// See documentation in fble-alloc.h.
void* FbleReAllocRaw(void* ptr, size_t size)
{
  return realloc(ptr, size);
}

// See documentation in fble-alloc.h.
void FbleFree(void* ptr)
{
  if (ptr == NULL) {
    return;
  }

  gCurrentAllocationsCount--;
  free(ptr);
}
