/**
 * @file fble-alloc.h
 *  Memory allocation routines.
 */

#ifndef FBLE_ALLOC_H_
#define FBLE_ALLOC_H_

#include <sys/types.h>    // for size_t

/**
 * @func[FbleRawAlloc] Allocates @a[size] bytes of memory.
 *  This function is not type safe. It is recommended to use the FbleAlloc and
 *  FbleArrayAlloc macros instead.
 *
 *  @arg[size_t][size] The number of bytes to allocate.
 *
 *  @returns[void*]
 *   A pointer to a newly allocated size bytes of memory.
 *
 *  @sideeffects
 *   Allocates size bytes that should be freed by calling FbleFree when no
 *   longer in use.
 */
void* FbleRawAlloc(size_t size);

/**
 * @func[FbleAlloc] Type safe object allocation.
 *  @arg[<type>][T] The type of object to allocate.
 *
 *  @returns T*
 *   A pointer to a newly allocated object of the given type.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAlloc(T) ((T*) FbleRawAlloc(sizeof(T)))

/**
 * @func[FbleAllocExtra] Allocate object with extra space.
 *  For use with objects like:
 *
 *  @code[c]
 *   struct {
 *     ...
 *     Foo foo[]
 *   };
 *
 *  @arg[<type>][T] The type of object to allocate.
 *  @arg[size_t][size] The size of the extra space to include.
 *
 *  @returns T*
 *   A pointer to a newly allocated object of the given type with extra size.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAllocExtra(T, size) ((T*) FbleRawAlloc(sizeof(T) + size))

/**
 * @func[FbleArrayAlloc] Type safe array allocation.
 *  @arg[<type>][T] the type of object to allocate
 *  @arg[size_t][count] the number of objects in the array to allocate.
 *
 *  @returns T*
 *   A pointer to a newly allocated array of count objects of the given type.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleArrayAlloc(T, count) ((T*) FbleRawAlloc((count) * sizeof(T)))

/**
 * @func[FbleFree] Free a memory allocation.
 *  @arg[void*][ptr] The allocation to free. May be NULL.
 *
 *  @sideeffects
 *   @i Frees memory associated with the ptr.
 *   @item
 *    After this call, any accesses to the freed memory result in undefined
 *    behavior.
 *   @item
 *    Behavior is undefined if ptr was not previously returned by a call to
 *    FbleAlloc or one of its variants.
 */
void FbleFree(void* ptr);

/** Abstract type of an allocator to use for allocations. */
typedef struct FbleStackAllocator FbleStackAllocator;

/**
 * @func[FbleNewStackAllocator] Create a new stack allocator.
 *  @returns FbleStackAllocator*
 *   A newly allocated stack allocator.
 *
 *  @sideeffects
 *   Allocates a stack allocator that should be freed using
 *   FbleFreeStackAllocator when no longer in use.
 */
FbleStackAllocator* FbleNewStackAllocator();

/**
 * @func[FbleFreeStackAllocator] Free resources associated with a stack allocator.
 *  @arg[FbleStackAllocator*][allocator] the allocator to free.
 *
 *  @sideeffects
 *   @i Frees resources associated with the given allocator.
 *   @item
 *    The allocator must not have any outstanding allocations at the time of
 *    free.
 */
void FbleFreeStackAllocator(FbleStackAllocator* allocator);

/**
 * @func[FbleRawStackAlloc] Allocate memory on stack.
 *  This function is not type safe. It is recommended to use the
 *  FbleStackAlloc macros instead.
 *
 *  @arg[FbleStackAllocator*][allocator] the allocator to allocate memory from.
 *  @arg[size_t             ][size     ] the number of bytes to allocate.
 *
 *  @returns void*
 *   A block of memory allocated.
 *
 *  @sideeffects
 *   Allocates memory that should be freed with FbleStackFree when no longer
 *   needed.
 */
void* FbleRawStackAlloc(FbleStackAllocator* allocator, size_t size);

/**
 * @func[FbleStackAlloc] Type safe stack allocation.
 *  @arg[FbleStackAllocator*][allocator] The allocator to allocate memory from.
 *  @arg[<type>][T] The type of object to allocate.
 * 
 *  @returns T*
 *   A pointer to a newly stack allocated object of the given type.
 *
 *  @sideeffects
 *   Allocates memory that should be freed with FbleStackFree when no longer
 *   needed.
 */
#define FbleStackAlloc(allocator, T) ((T*) FbleRawStackAlloc(allocator, sizeof(T)))

/**
 * @func[FbleStackAllocExtra] Allocate stack object with extra space.
 *  For use with objects like:
 *
 *  @code[c]
 *   struct {
 *     ...
 *     Foo foo[]
 *   };
 *
 *  @arg[FbleStackAllocator*][allocator]
 *   The stack allocator.
 *  @arg[<type>][T]
 *   The type of object to allocate.
 *  @arg[size_t][size]
 *   The size of the extra space to include.
 *
 *  @returns T*
 *   A pointer to a newly allocated object of the given type with extra size.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleStackFree when no longer in
 *   use.
 */
#define FbleStackAllocExtra(allocator, T, size) ((T*) FbleRawStackAlloc(allocator, sizeof(T) + size))

/**
 * @func[FbleStackFree] Free a block of allocated memory.
 *  @arg[FbleStackAllocator*] allocator
 *   the allocator to free the memory from.
 *  @arg[void*] ptr
 *   the block of memory to free.
 *
 *  @sideeffects
 *   @i Frees resources associated with the given pointer.
 *   @item
 *    Behavior is undefined if @a[ptr] is not the same as the pointer most
 *    recently returned from FbleStackAlloc on this same allocator.
 */
void FbleStackFree(FbleStackAllocator* allocator, void* ptr);

/**
 * @func[FbleMaxTotalBytesAllocated] Gets the max bytes allocated.
 *  @returns size_t
 *   The high watermark of bytes allocated using the fble allocation routines
 *   since the most recent call to FbleResetMaxTotalBytesAllocated.
 */
size_t FbleMaxTotalBytesAllocated();

/**
 * @func[FbleResetMaxTotalBytesAllocated] Resets the max bytes allocated.
 *  @sideeffects
 *   Resets the max total alloc size to the current number of bytes allocated
 *   using the fble allocation routines.
 */
void FbleResetMaxTotalBytesAllocated();

#endif // FBLE_ALLOC_H_
