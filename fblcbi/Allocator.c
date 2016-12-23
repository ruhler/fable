// Allocator.c --
//
//   This file implements routines for allocating memory that will be freed in
//   bulk.

#include <string.h>

#include "fblc.h"

// VectorInit --
//
//   Initialize a Vector for allocations.
//
// Inputs:
//   arena - The arena to use for the allocations.
//   vector - The vector to initialize.
//   size - The size in bytes of a single element of the vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized for allocation.

void VectorInit(FblcArena* arena, Vector* vector, size_t size)
{
  vector->arena = arena;
  vector->size = size;
  vector->capacity = 4;
  vector->count = 0;
  vector->data = arena->alloc(arena, vector->capacity * size);
}

// VectorAppend --
//
//   Append an element to a vector.
//
// Inputs:
//   vector - The vector to append an element to.
//
// Results:
//   A pointer to the newly appended element.
//
// Side effects:
//   The size of the vector is expanded by one element. Pointers returned for
//   previous elements may become invalid.

void* VectorAppend(Vector* vector)
{
  if (vector->count == vector->capacity) {
    // TODO: Do something smarter to avoid wasting the previous allocation.
    void* old = vector->data;
    size_t old_capacity = vector->capacity;
    vector->capacity *= 2;
    size_t old_size = old_capacity * vector->size;
    size_t size = vector->capacity * vector->size;
    vector->data = vector->arena->alloc(vector->arena, size);
    memcpy(vector->data, old, old_size);
    vector->arena->free(vector->arena, old);
  }
  return (void*)((uintptr_t)vector->data + vector->count++ * vector->size);
}

// VectorExtract
//
//   Extract the raw data from a completed vector.
//
// Inputs:
//   vector - The vector to extract data from.
//   count - An out parameter for the final number of elements in the vector.
//
// Results:
//   The element data for the vector.
//
// Side effects:
//   count is updated with the final number of elements in the vector.

void* VectorExtract(Vector* vector, size_t* count)
{
  // TODO: Do something to save the unused space?
  *count = vector->count;
  return vector->data;
}
