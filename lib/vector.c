/**
 * @file vector.c
 * Fble vector routines.
 */

#include <fble/fble-vector.h>

#include <string.h>     // for memcpy

#include <fble/fble-alloc.h>

// FbleExtendVectorRaw -- see fble.h for documentation of this function.
void FbleExtendVectorRaw(size_t elem_size, size_t* size, void** xs)
{
  // We assume the capacity of the array is the smallest power of 2 that holds
  // size elements. If size is equal to the capacity of the array, we double
  // the capacity of the array, which preserves the invariant after the size is
  // incremented.
  size_t s = (*size)++;
  if (s > 0 && (s & (s - 1)) == 0) {
    void* resized = FbleAllocRaw(2 * s * elem_size);
    memcpy(resized, *xs, s * elem_size);
    FbleFree(*xs);
    *xs = resized;
  }
}
