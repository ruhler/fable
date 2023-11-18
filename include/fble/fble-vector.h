/**
 * @file fble-vector.h
 *  Fble Vector APIs
 *
 *  A common data structure in fble is an array of elements with a size. By
 *  convention, fble uses the same data structure layout and naming for all of
 *  these vector data structures. The type of a vector of elements T is:
 *    struct {
 *      size_t size;
 *      T* xs;
 *    };
 *  
 *  Often these vectors are constructed without knowing the size ahead of
 *  time. The macros defined in this file are used to help construct these
 *  vectors, regardless of the element type.
 *  
 *  @warning 
 *   If you want to pass around references to elements of a vector, use T**
 *   for the type of xs. Otherwise when the vector gets resized, any pointers
 *   into it will be invalidated, leading to hard-to-find bugs.
 */

#ifndef FBLE_VECTOR_H_
#define FBLE_VECTOR_H_

#include <sys/types.h>    // for size_t

#include "fble-alloc.h"

/**
 * @func[FbleVectorInit] Initializes a new vector.
 *  @arg[FbleVector<T>][vector] A reference to an uninitialized vector.
 *
 *  @sideeffects
 *   The vector is initialized to an array containing 0 elements.
 */
// Implementation Note:
// The array initially has size 0 and capacity 1.
#define FbleVectorInit(vector) \
  (vector).size = 0; \
  (vector).xs = FbleRawAlloc(sizeof(*((vector).xs)))

/**
 * @func[FbleFreeVector] Frees an Fble vector.
 *  This function does not free individual vector elements. It is the
 *  responsibility of the caller to free individual elements as they wish.
 *  This function only frees internal resources allocated for the vector.
 *
 *  @arg[FbleVector<T>] vector
 *   The vector whose resources to free.
 *
 *  @sideeffects
 *   Frees resources of the vector.
 */
#define FbleFreeVector(vector) \
  FbleFree((vector).xs)

/**
 * @func[FbleVectorExtend] Appends an uninitialized element.
 *  @arg[FbleVector<T>] vector
 *   A vector that was initialized using FbleVectorInit.
 *
 *  @returns T*
 *   A pointer to the newly appended uninitialized element.
 *
 *  @sideeffects
 *   A new uninitialized element is appended to the array and the size is
 *   incremented. If necessary, the array is re-allocated to make space for
 *   the new element.
 */
#define FbleVectorExtend(vector) \
  (FbleVectorIncrSize(sizeof(*((vector).xs)), &(vector).size, (void**)&(vector).xs), (vector).xs + (vector).size - 1)

/**
 * @func[FbleVectorAppend] Appends an element.
 *  @arg[FbleVector<T>] vector
 *   A vector that was initialized using FbleVectorInit.
 *  @arg[T] elem
 *   An element of type T to append to the array.
 *
 *  @sideeffects
 *   The given element is appended to the array and the size is incremented.
 *   If necessary, the array is re-allocated to make space for the new
 *   element.
 */
#define FbleVectorAppend(vector, elem) \
  (*FbleVectorExtend(vector) = elem)

/**
 * @func[FbleVectorIncrSize] Increases the size of a vector.
 *  Increase the size of an fble vector by a single element.
 *
 *  This is an internal function used for implementing the fble vector macros.
 *  This function should not be called directly because because it does not
 *  provide the same level of type safety the macros provide.
 *
 *  @arg[size_t ][elem_size] The sizeof the element type in bytes.
 *  @arg[size_t*][size     ] A pointer to the size field of the vector.
 *  @arg[void** ][xs       ] A pointer to the xs field of the vector.
 *
 *  @sideeffects
 *   A new uninitialized element is appended to the vector and the size is
 *   incremented. If necessary, the array is re-allocated to make space for
 *   the new element.
 */
void FbleVectorIncrSize(size_t elem_size, size_t* size, void** xs);

#endif // FBLE_VECTOR_H_
