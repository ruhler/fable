/**
 * @file alloc.c
 *  Implementation of fbld allocation routines.
 */

#include "alloc.h"

#include <stdio.h>    // for fprintf, stderr
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for malloc

/**
 * @value[gNumAllocations]
 *  Total number of regions allocated via FbldAlloc routines that have not yet
 *  been freed. Used for detecting memory leak at exit.
 *
 *  @type[size_t]
 */
size_t gNumAllocations = 0;

/**
 * @value[gInitialized] True if the exit routine has been registered.
 *  @type[bool]
 */
bool gInitialized = false;

static void Exit();

/**
 * @func[Exit] Exit function to check for memory leaks.
 *  @sideeffects
 *   Prints an error message and aborts if memory leaks are detected.
 */
static void Exit()
{
  if (gNumAllocations != 0) {
    fprintf(stderr, "ERROR: MEMORY LEAK DETECTED\n");
    fprintf(stderr, "Try running again using: valgrind --leak-check=full\n");
    abort();
  }
}

// See documentation in alloc.h.
void* FbldAllocRaw(size_t size)
{
  if (!gInitialized) {
    gInitialized = true;
    atexit(&Exit);
  }

  gNumAllocations++;
  return malloc(size);
}
// See documentation in alloc.h.
void* FbldReAllocRaw(void* ptr, size_t size)
{
  if (ptr == NULL) {
    return FbldAllocRaw(size);
  }
  return realloc(ptr, size);
}

// See documentation in alloc.h.
void FbldFree(void* ptr)
{
  if (ptr == NULL) {
    return;
  }

  gNumAllocations--;
  free(ptr);
}
