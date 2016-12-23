// Allocator.c --
//
//   This file implements routines for allocating memory that will be freed in
//   bulk.

#include <string.h>

#include "fblc.h"

struct AllocList {
  AllocList* next;
  uint8_t data[];
};


// InitAllocator --
//
//   Initialize an Allocator.
//
// Inputs:
//   alloc - The allocator to initialize.
//
// Results:
//   None.
//
// Side effects:
//   The allocator is initialized and ready to be used for allocation.

void InitAllocator(Allocator* alloc)
{
  alloc->allocations = NULL;
}

// Alloc --
//
//   Allocate a block of memory 'size' bytes in length.
//
// Inputs:
//   alloc - The allocator to use for the allocation.
//   size - The number of bytes to allocate.
//
// Results:
//   A pointer to 'size' bytes of memory with undefined contents. The memory
//   will be freed when FreeAll is called on the allocator.
//
// Side effects:
//   Memory is allocated.

void* Alloc(Allocator* alloc, size_t size)
{
  AllocList* allocation = MALLOC(sizeof(AllocList) + size);
  allocation->next = alloc->allocations;
  alloc->allocations = allocation;
  return allocation->data;
}

// FreeAll --
//
//   Free all memory allocated with this allocator.
//
// Inputs:
//   alloc - The allocator to free the memory for.
//
// Results:
//   None.
//
// Side effects:
//   All memory allocated with this allocator is freed and should no longer be
//   used.

void FreeAll(Allocator* alloc)
{
  while (alloc->allocations != NULL) {
    AllocList* this = alloc->allocations;
    alloc->allocations = alloc->allocations->next;
    FREE(this);
  }
}

// VectorInit --
//
//   Initialize a Vector for allocations.
//
// Inputs:
//   alloc - The allocator to use for the allocations.
//   vector - The vector to initialize.
//   size - The size in bytes of a single element of the vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized for allocation.

void VectorInit(Allocator* alloc, Vector* vector, size_t size)
{
  vector->allocator = alloc;
  vector->size = size;
  vector->capacity = 4;
  vector->count = 0;
  vector->data = Alloc(alloc, vector->capacity * size);
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
    int old_capacity = vector->capacity;
    vector->capacity *= 2;
    vector->data = Alloc(
        vector->allocator, vector->capacity * vector->size);
    memcpy(vector->data, old, old_capacity * vector->size);
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
  // TODO: Do something to save the unused space.
  *count = vector->count;
  return vector->data;
}
