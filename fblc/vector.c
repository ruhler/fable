// vector.c --
//   This file implements the fblc vector routines.

#include <string.h>     // for memcpy

#include "fblc.h"

// FblcVectorIncrSize -- see fblc.h for documentation of this function.
void FblcVectorIncrSize(FblcArena* arena, size_t elem_size, size_t* size, void** xs)
{
  // We assume the capacity of the array is the smallest power of 2 that holds
  // size elements. If size is equal to the capacity of the array, we double
  // the capacity of the array, which preserves the invariant after the size is
  // incremented.
  size_t s = (*size)++;
  if (s > 0 && (s & (s - 1)) == 0) {
    void* resized = arena->alloc(arena, 2 * s * elem_size, FBLC_ALLOC_MSG(__FILE__, __LINE__));
    memcpy(resized, *xs, s * elem_size);
    arena->free(arena, *xs);
    *xs = resized;
  }
}
