// stack-alloc.c
//   This file implements the stack allocation routines.

#include "stack-alloc.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble-alloc.h"

typedef struct Alloc {
  struct Alloc* tail;
  char data[];
} Alloc;

struct FbleStackAllocator {
  Alloc* allocs;
};

// FbleNewStackAllocator -- see documentation in stack-alloc.h
FbleStackAllocator* FbleNewStackAllocator()
{
  FbleStackAllocator* allocator = FbleAlloc(FbleStackAllocator);
  allocator->allocs = NULL;
  return allocator;
}

// FbleFreeStackAllocator -- see documentation in stack-alloc.h
void FbleFreeStackAllocator(FbleStackAllocator* allocator)
{
  assert(allocator->allocs == NULL);
  FbleFree(allocator);
}

// FbleStackAlloc -- see documentation in stack-alloc.h
void* FbleStackAlloc(FbleStackAllocator* allocator, size_t size)
{
  Alloc* alloc = FbleAllocExtra(Alloc, size);
  alloc->tail = allocator->allocs;
  allocator->allocs = alloc;
  return alloc->data;
}

// FbleStackFree -- see documentation in stack-alloc.h
void FbleStackFree(FbleStackAllocator* allocator, void* ptr)
{
  Alloc* alloc = allocator->allocs;
  assert(ptr == alloc->data);
  allocator->allocs = alloc->tail;
  FbleFree(alloc);
}
