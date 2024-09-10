/**
 * @file alloc.h
 *  Memory allocation routines.
 */

#ifndef FBLD_ALLOC_H_
#define FBLD_ALLOC_H_

#include <sys/types.h>    // for size_t

/**
 * @func[FbldAllocRaw] Allocates @a[size] bytes of memory.
 *  This function is not type safe. It is recommended to use the FbldAlloc and
 *  FbldAllocArray macros instead.
 *
 *  @arg[size_t][size] The number of bytes to allocate.
 *
 *  @returns[void*]
 *   A pointer to a newly allocated size bytes of memory.
 *
 *  @sideeffects
 *   Allocates size bytes that should be freed by calling FbldFree when no
 *   longer in use.
 */
void* FbldAllocRaw(size_t size);

/**
 * @func[FbldReAllocRaw] Re-allocates to @a[size] bytes of memory.
 *  This function is not type safe.
 *
 *  @arg[void*][ptr] The memory region to reallocate.
 *  @arg[size_t][size] The number of bytes to reallocate to.
 *
 *  @returns[void*]
 *   A pointer to the reallocated size bytes of memory.
 *
 *  @sideeffects
 *   The original buffer is invalidated by this call.
 */
void* FbldReAllocRaw(void* ptr, size_t size);

/**
 * @func[FbldAlloc] Type safe object allocation.
 *  @arg[type][T] The type of object to allocate.
 *
 *  @returns T*
 *   A pointer to a newly allocated object of the given type.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbldFree when no longer in use.
 */
#define FbldAlloc(T) ((T*) FbldAllocRaw(sizeof(T)))

/**
 * @func[FbldAllocExtra] Allocate object with extra space.
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
 *   The allocation should be freed by calling FbldFree when no longer in use.
 */
#define FbldAllocExtra(T, size) ((T*) FbldAllocRaw(sizeof(T) + size))

/**
 * @func[FbldAllocArray] Type safe array allocation.
 *  @arg[type][T] the type of object to allocate
 *  @arg[size_t][count] the number of objects in the array to allocate.
 *
 *  @returns T*
 *   A pointer to a newly allocated array of count objects of the given type.
 *
 *  @sideeffects
 *   The allocation should be freed by calling FbldFree when no longer in use.
 */
#define FbldAllocArray(T, count) ((T*) FbldAllocRaw((count) * sizeof(T)))

/**
 * @func[FbldReAllocArray] Type safe array re-allocation.
 *  @arg[type][T] the type of object to allocate
 *  @arg[T*][ptr] the original allocation.
 *  @arg[size_t][count] the number of objects to reallocate the array to.
 *
 *  @returns T*
 *   A pointer to a newly re-allocated array of count objects of the given type.
 *
 *  @sideeffects
 *   The original buffer is invalidated by this call.
 */
#define FbldReAllocArray(T, ptr, count) ((T*) FbldReAllocRaw(ptr, (count) * sizeof(T)))

/**
 * @func[FbldFree] Free a memory allocation.
 *  @arg[void*][ptr] The allocation to free. May be NULL.
 *
 *  @sideeffects
 *   @i Frees memory associated with the ptr.
 *   @item
 *    After this call, any accesses to the freed memory result in undefined
 *    behavior.
 *   @item
 *    Behavior is undefined if ptr was not previously returned by a call to
 *    FbldAlloc or one of its variants.
 */
void FbldFree(void* ptr);

#endif // FBLD_ALLOC_H_
