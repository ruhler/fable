// vector.c --
//   This file implements the fble vector routines.

#include <string.h>     // for memcpy

#include "fble-alloc.h"
#include "fble-vector.h"

// FbleVectorIncrSize -- see fble.h for documentation of this function.
void FbleVectorIncrSize(size_t elem_size, size_t* size, void** xs)
{
  // We assume the capacity of the array is the smallest power of 2 that holds
  // size elements. If size is equal to the capacity of the array, we double
  // the capacity of the array, which preserves the invariant after the size is
  // incremented.
  size_t s = (*size)++;
  if (s > 0 && (s & (s - 1)) == 0) {
    void* resized = FbleRawAlloc(2 * s * elem_size, FbleAllocMsg(__FILE__, __LINE__));
    memcpy(resized, *xs, s * elem_size);
    FbleFree(*xs);
    *xs = resized;
  }
}
