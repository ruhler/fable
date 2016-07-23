// FblcAllocator.c --
//
//   This file implements routines for allocating memory that will be freed in
//   bulk.

#include "FblcInternal.h"

struct FblcAllocList {
  FblcAllocList* next;
  uint8_t data[];
};


// FblcInitAllocator --
//
//   Initialize an FblcAllocator.
//
// Inputs:
//   alloc - The allocator to initialize.
//
// Results:
//   None.
//
// Side effects:
//   The allocator is initialized and ready to be used for allocation.

void FblcInitAllocator(FblcAllocator* alloc)
{
  alloc->allocations = NULL;
}

// FblcAlloc --
//
//   Allocate a block of memory 'size' bytes in length.
//
// Inputs:
//   alloc - The allocator to use for the allocation.
//   size - The number of bytes to allocate.
//
// Results:
//   A pointer to 'size' bytes of memory with undefined contents. The memory
//   will be freed when FblcFreeAll is called on the allocator.
//
// Side effects:
//   Memory is allocated.

void* FblcAlloc(FblcAllocator* alloc, int size)
{
  FblcAllocList* allocation = MALLOC(sizeof(FblcAllocList) + size);
  allocation->next = alloc->allocations;
  alloc->allocations = allocation;
  return allocation->data;
}

// FblcFreeAll --
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

void FblcFreeAll(FblcAllocator* alloc)
{
  while (alloc->allocations != NULL) {
    FblcAllocList* this = alloc->allocations;
    alloc->allocations = alloc->allocations->next;
    FREE(this);
  }
}

// FblcVectorInit --
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

void FblcVectorInit(FblcAllocator* alloc, FblcVector* vector, int size)
{
  vector->allocator = alloc;
  vector->size = size;
  vector->capacity = 4;
  vector->count = 0;
  vector->data = FblcAlloc(alloc, vector->capacity * size);
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

void* FblcVectorAppend(FblcVector* vector)
{
  if (vector->count == vector->capacity) {
    // TODO: Do something smarter to avoid wasting the previous allocation.
    void* old = vector->data;
    int old_capacity = vector->capacity;
    vector->capacity *= 2;
    vector->data = FblcAlloc(
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

void* FblcVectorExtract(FblcVector* vector, int* count)
{
  // TODO: Do something to save the unused space.
  *count = vector->count;
  return vector->data;
}
