/**
 * @file tag.h
 *  Header for FbleTagV.
 */

#ifndef FBLE_INTERNAL_TAG_H_
#define FBLE_INTERNAL_TAG_H_

#include <stddef.h>    // for size_t

/**
 * Vector of tags used in union values.
 */
typedef struct {
  size_t size;    /**< Number of elements. */
  size_t* xs;     /**< The elements of the vector. */
} FbleTagV;

#endif // FBLE_INTERNAL_TAG_H_
