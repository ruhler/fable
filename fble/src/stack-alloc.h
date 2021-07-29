// stack-alloc.h --
//   Header file describing a malloc-like API for allocating memory in
//   last-in-first-out order.

#ifndef FBLE_INTERNAL_STACK_ALLOC_H_
#define FBLE_INTERNAL_STACK_ALLOC_H_

#include <sys/types.h>    // for size_t

// FbleStackAllocator --
//   Abstract type of an allocator to use for allocations.
typedef struct FbleStackAllocator FbleStackAllocator;

// FbleNewStackAllocator --
//   Create a new stack allocator.
//
// Returns:
//   A newly allocated stack allocator.
//
// Side effects:
//   Allocates a stack allocator that should be freed using
//   FbleFreeStackAllocator when no longer in use.
FbleStackAllocator* FbleNewStackAllocator();

// FbleFreeStackAllocator --
//   Free resources associated with a stack allocator.
//
// Inputs:
//   allocator - the allocator to free.
//
// Side effects:
// * Frees resources associated with the given allocator.
// * The allocator must not have any outstanding allocations at the time of
//   free.
void FbleFreeStackAllocator(FbleStackAllocator* allocator);

// FbleStackAlloc --
//   Allocate a block of memory using the given stack allocator.
//
// Inputs:
//   allocator - the allocator to allocate memory from.
//   size - the number of bytes to allocate.
//
// Results:
//   A block of memory allocated.
//
// Side effects:
//   Allocates memory that should be freed with FbleStackFree when no longer
//   needed.
void* FbleStackAlloc(FbleStackAllocator* allocator, size_t size);

// FbleStackFree --
//   Free a block of allocated memory.
//
// Inputs:
//   allocator - the allocator to free the memory from.
//   ptr - the block of memory to free.
//
// Side effects:
// * Frees resources associated with the given pointer.
// * Behavior is undefined if 'ptr' is not the same as the pointer most
//   recently returned from FbleStackAlloc on this same allocator.
void FbleStackFree(FbleStackAllocator* allocator, void* ptr);

#endif // FBLE_INTERNAL_STACK_ALLOC_H_
