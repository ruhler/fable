/**
 * @file alloc.c
 *  Implementation of fble allocation routines.
 */

#include <fble/fble-alloc.h>

#include <stdio.h>    // for fprintf, stderr
#include <stdbool.h>  // for bool
#include <stdlib.h>   // for malloc

/**
 * @value[gTotalBytesAllocated]
 * @ Total number of bytes currently allocated via FbleAlloc routines.
 *  @type[size_t]
 */
size_t gTotalBytesAllocated = 0;

/**
 * @value[gMaxTotalBytesAllocated]
 * @ Max value of gTotalBytesAllocated since last cleared.
 *  @type[size_t]
 */
size_t gMaxTotalBytesAllocated = 0;

/**
 * @value[gInitialized] True if the exit routine has been registered.
 *  @type[bool]
 */
bool gInitialized = false;

/**
 * @struct[Alloc] An allocation.
 *  @field[size_t][size] Size of the allocation.
 *  @field[char*][data] Allocated memory region available to the user.
 */
typedef struct {
  size_t size;
  char data[];
} Alloc;

static void Exit();

/**
 * @func[Exit] Exit function to check for memory leaks.
 *  @sideeffects
 *   Prints an error message and aborts if memory leaks are detected.
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
// See documentation in fble-alloc.h.
void* FbleReAllocRaw(void* ptr, size_t size)
{
  if (ptr == NULL) {
    return FbleAllocRaw(size);
  }

  Alloc* alloc = ((Alloc*)ptr) - 1;
  gTotalBytesAllocated += size - alloc->size;
  if (gTotalBytesAllocated > gMaxTotalBytesAllocated) {
    gMaxTotalBytesAllocated = gTotalBytesAllocated;
  }

  alloc = realloc(alloc, sizeof(Alloc) + size);
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
  gTotalBytesAllocated -= alloc->size;
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
