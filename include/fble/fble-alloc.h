/**
 * @file fble-alloc.h
 *  Memory allocation routines.
 */

#ifndef FBLE_ALLOC_H_
#define FBLE_ALLOC_H_

#include <sys/types.h>    // for size_t

/**
 * @func[FbleAllocRaw] Allocates @a[size] bytes of memory.
 *  This function is not type safe. It is recommended to use the FbleAlloc and
 *  FbleAllocArray macros instead.
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
void* FbleAllocRaw(size_t size);

/**
 * @func[FbleAlloc] Type safe object allocation.
 *  @arg[type][T] The type of object to allocate.
 *
 *  @returns T*
 *   A pointer to a newly allocated object of the given type.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAlloc(T) ((T*) FbleAllocRaw(sizeof(T)))

/**
 * @func[FbleAllocExtra] Allocate object with extra space.
 *  For use with objects like:
 *
 *  @code[c] @
 *   struct {
 *     ...
 *     Foo foo[]
 *   };
 *
 *  @arg[type][T] The type of object to allocate.
 *  @arg[size_t][size] The size of the extra space to include.
 *
 *  @returns T*
 *   A pointer to a newly allocated object of the given type with extra size.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAllocExtra(T, size) ((T*) FbleAllocRaw(sizeof(T) + size))

/**
 * @func[FbleAllocArray] Type safe array allocation.
 *  @arg[type][T] the type of object to allocate
 *  @arg[size_t][count] the number of objects in the array to allocate.
 *
 *  @returns T*
 *   A pointer to a newly allocated array of count objects of the given type.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbleFree when no longer in use.
 */
#define FbleAllocArray(T, count) ((T*) FbleAllocRaw((count) * sizeof(T)))

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
