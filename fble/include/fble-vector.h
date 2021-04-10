// fble-vector.h --
//   Header file for vector operations in fble.

#ifndef FBLE_VECTOR_H_
#define FBLE_VECTOR_H_

#include <sys/types.h>    // for size_t

#include "fble-alloc.h"

// FbleVector --
//   A common data structure in fble is an array of elements with a size. By
//   convention, fble uses the same data structure layout and naming for all
//   of these vector data structures. The type of a vector of elements T is:
//   struct {
//     size_t size;
//     T* xs;
//   };
//
//   Often these vectors are constructed without knowing the size ahead of
//   time. The following macros are used to help construct these vectors,
//   regardless of the element type.

// FbleVectorInit --
//   Initialize a vector for construction.
//
// Inputs:
//   vector - A reference to an uninitialized vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized to an array containing 0 elements.
//
// Implementation notes:
//   The array initially has size 0 and capacity 1.
#define FbleVectorInit(vector) \
  (vector).size = 0; \
  (vector).xs = FbleRawAlloc(sizeof(*((vector).xs)), FbleAllocMsg(__FILE__, __LINE__))

// FbleVectorExtend --
//   Append an uninitialized element to the vector.
//
// Inputs:
//   vector - A reference to a vector that was initialized using FbleVectorInit.
//
// Results:
//   A pointer to the newly appended uninitialized element.
//
// Side effects:
//   A new uninitialized element is appended to the array and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
#define FbleVectorExtend(vector) \
  (FbleVectorIncrSize(sizeof(*((vector).xs)), &(vector).size, (void**)&(vector).xs), (vector).xs + (vector).size - 1)

// FbleVectorAppend --
//   Append an element to a vector.
//
// Inputs:
//   vector - A reference to a vector that was initialized using FbleVectorInit.
//   elem - An element of type T to append to the array.
//
// Results:
//   None.
//
// Side effects:
//   The given element is appended to the array and the size is incremented.
//   If necessary, the array is re-allocated to make space for the new
//   element.
#define FbleVectorAppend(vector, elem) \
  (*FbleVectorExtend(vector) = elem)

// FbleVectorIncrSize --
//   Increase the size of an fble vector by a single element.
//
//   This is an internal function used for implementing the fble vector macros.
//   This function should not be called directly because because it does not
//   provide the same level of type safety the macros provide.
//
// Inputs:
//   elem_size - The sizeof the element type in bytes.
//   size - A pointer to the size field of the vector.
//   xs - A pointer to the xs field of the vector.
//
// Results:
//   None.
//
// Side effects:
// * A new uninitialized element is appended to the vector and the size is
//   incremented. If necessary, the array is re-allocated to make space for
//   the new element.
void FbleVectorIncrSize(size_t elem_size, size_t* size, void** xs);

#endif // FBLE_VECTOR_H_
