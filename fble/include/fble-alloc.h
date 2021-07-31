// fble-alloc.h --
//   Header file describing memory allocation functionality for fble.

#ifndef FBLE_ALLOC_H_
#define FBLE_ALLOC_H_

#include <sys/types.h>    // for size_t

// FbleRawAlloc --
//   Allocate size bytes of memory. This function is not type safe. It is
//   recommended to use the FbleAlloc and FbleArrayAlloc macros instead.
//
// Inputs:
//   size - The number of bytes to allocate.
//
// Result:
//   A pointer to a newly allocated size bytes of memory.
//
// Side effects:
// * Allocates size bytes that should be freed by calling FbleFree when no
//   longer in use.
void* FbleRawAlloc(size_t size);

// FbleAlloc --
//   A type safe way of allocating objects.
//
// Inputs:
//   T - The type of object to allocate.
//
// Results:
//   A pointer to a newly allocated object of the given type.
//
// Side effects:
// * The allocation should be freed by calling FbleFree when no longer in use.
#define FbleAlloc(T) ((T*) FbleRawAlloc(sizeof(T)))

// FbleAllocExtra --
//   Allocate an object with additional extra space. For use with objects like:
//   struct {
//     ...
//     Foo foo[]
//   };
//
// Inputs:
//   T - The type of object to allocate.
//   size - The size of the extra space to include.
//
// Results:
//   A pointer to a newly allocated object of the given type with extra size.
//
// Side effects:
// * The allocation should be freed by calling FbleFree when no longer in use.
#define FbleAllocExtra(T, size) ((T*) FbleRawAlloc(sizeof(T) + size))

// FbleArrayAlloc --
//   A type safe way of allocating an array of objects.
//
// Inputs:
//   T - the type of object to allocate
//   count - the number of objects in the array to allocate.
//
// Results:
//   A pointer to a newly allocated array of count objects of the given type.
//
// Side effects:
// * The allocation should be freed by calling FbleFree when no longer in use.
#define FbleArrayAlloc(T, count) ((T*) FbleRawAlloc((count) * sizeof(T)))

// FbleFree --
//   Free a memory allocation.
//
// Inputs:
//   ptr - The allocation to free. May be NULL.
//
// Side effects:
// * Frees memory associated with the ptr.
// * After this call, any accesses to the freed memory result in undefined
//   behavior.
// * Behavior is undefined if ptr was not previously returned by a call to
//   FbleAlloc or one of its variants.
void FbleFree(void* ptr);

// FbleMaxTotalBytesAllocated
//   Returns the maximum number of bytes allocated using the fble allocation
//   routines since the most recent call to FbleResetMaxTotalBytesAllocated.
//
// Results:
//   The high water mark of allocations in total number of bytes allocated.
//
// Side effects:
//   None.
size_t FbleMaxTotalBytesAllocated();

// FbleResetMaxTotalBytesAllocated
//   Resets the max total alloc size to the current number of bytes
//   allocated using the fble allocation routines.
//
// Side effects:
// * Resets the max total alloc size to the current number of bytes allocated
//   using the fble allocation routines.
void FbleResetMaxTotalBytesAllocated();

#endif // FBLE_ALLOC_H_
