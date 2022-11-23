/**
 * @file fble-alloc.h
 * Memory allocation routines.
 */

#ifndef FBLE_ALLOC_H_
#define FBLE_ALLOC_H_

#include <sys/types.h>    // for size_t

/**
 * Allocates 'size' bytes of memory. This function is not type safe. It is
 * recommended to use the FbleAlloc and FbleArrayAlloc macros instead.
 *
 * @param size The number of bytes to allocate.
 * @returns  A pointer to a newly allocated size bytes of memory.
 * @sideeffects
 * * Allocates size bytes that should be freed by calling FbleFree when no
 *   longer in use.
 */
void* FbleRawAlloc(size_t size);

/**
 * Type safe object allocation.
 *
 * @param T The type of object to allocate.
 *
 * @returns
 *   A pointer to a newly allocated object of the given type.
 *
 * @sideeffects
 * * The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAlloc(T) ((T*) FbleRawAlloc(sizeof(T)))

/**
 * Allocate object with extra space.
 *
 * For use with objects like:
 *   struct {
 *     ...
 *     Foo foo[]
 *   };
 *
 * @param T     The type of object to allocate.
 * @param size  The size of the extra space to include.
 *
 * @returns
 *   A pointer to a newly allocated object of the given type with extra size.
 *
 * @sideeffects
 * * The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAllocExtra(T, size) ((T*) FbleRawAlloc(sizeof(T) + size))

/**
 * Type safe array allocation.
 *
 * @param T       the type of object to allocate
 * @param count   the number of objects in the array to allocate.
 *
 * @returns
 *   A pointer to a newly allocated array of count objects of the given type.
 *
 * @sideeffects
 * * The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleArrayAlloc(T, count) ((T*) FbleRawAlloc((count) * sizeof(T)))

/**
 * Free a memory allocation.
 *
 * @param ptr   The allocation to free. May be NULL.
 *
 * @sideeffects
 * * Frees memory associated with the ptr.
 * * After this call, any accesses to the freed memory result in undefined
 *   behavior.
 * * Behavior is undefined if ptr was not previously returned by a call to
 *   FbleAlloc or one of its variants.
 */
void FbleFree(void* ptr);

/** Abstract type of an allocator to use for allocations. */
typedef struct FbleStackAllocator FbleStackAllocator;

/**
 * Create a new stack allocator.
 *
 * @returns
 *   A newly allocated stack allocator.
 *
 * @sideeffects
 * * Allocates a stack allocator that should be freed using
 *   FbleFreeStackAllocator when no longer in use.
 */
FbleStackAllocator* FbleNewStackAllocator();

/**
 * Free resources associated with a stack allocator.
 *
 * @param allocator the allocator to free.
 *
 * @sideeffects
 * * Frees resources associated with the given allocator.
 * * The allocator must not have any outstanding allocations at the time of
 *   free.
 */
void FbleFreeStackAllocator(FbleStackAllocator* allocator);

/**
 * Allocate memory on stack.
 *
 * This function is not type safe. It is recommended to use the FbleStackAlloc
 * macros instead.
 *
 * @param allocator   the allocator to allocate memory from.
 * @param size        the number of bytes to allocate.
 *
 * @returns
 *   A block of memory allocated.
 *
 * @sideeffects
 * * Allocates memory that should be freed with FbleStackFree when no longer
 *   needed.
 */
void* FbleRawStackAlloc(FbleStackAllocator* allocator, size_t size);

/**
 * Type safe stack allocation.
 *
 * @param allocator   The allocator to allocate memory from.
 * @param T           The type of object to allocate.
 * 
 * @returns
 *   A pointer to a newly stack allocated object of the given type.
 *
 * @sideeffects
 *   Allocates memory that should be freed with FbleStackFree when no longer
 *   needed.
 */
#define FbleStackAlloc(allocator, T) ((T*) FbleRawStackAlloc(allocator, sizeof(T)))

/**
 * Allocate stack object with extra space.
 *
 * For use with objects like:
 *   struct {
 *     ...
 *     Foo foo[]
 *   };
 *
 * @param allocator   The stack allocator.
 * @param T           The type of object to allocate.
 * @param size        The size of the extra space to include.
 *
 * @returns
 *   A pointer to a newly allocated object of the given type with extra size.
 *
 * @sideeffects
 * * The allocation should be freed by calling FbleStackFree when no longer in
 *   use.
 */
#define FbleStackAllocExtra(allocator, T, size) ((T*) FbleRawStackAlloc(allocator, sizeof(T) + size))

/**
 * Free a block of allocated memory.
 *
 * @param allocator   the allocator to free the memory from.
 * @param ptr         the block of memory to free.
 *
 * @sideeffects
 * * Frees resources associated with the given pointer.
 * * Behavior is undefined if 'ptr' is not the same as the pointer most
 *   recently returned from FbleStackAlloc on this same allocator.
 */
void FbleStackFree(FbleStackAllocator* allocator, void* ptr);

/**
 * Get max bytes allocated.
 *
 * @returns
 *   The high watermark of bytes allocated using the fble allocation routines
 *   since the most recent call to FbleResetMaxTotalBytesAllocated.
 */
size_t FbleMaxTotalBytesAllocated();

/**
 * Reset max bytes allocated.
 *
 * @sideeffects
 * * Resets the max total alloc size to the current number of bytes allocated
 *   using the fble allocation routines.
 */
void FbleResetMaxTotalBytesAllocated();

#endif // FBLE_ALLOC_H_
