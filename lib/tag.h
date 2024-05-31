/**
 * @file tag.h
 *  Header for FbleTagV.
 */

#ifndef FBLE_INTERNAL_TAG_H_
#define FBLE_INTERNAL_TAG_H_

#include <stddef.h>    // for size_t

/**
 * @struct[FbleTagV] Vector of tags used in union values.
 *  @field[size_t][size] Number of elements.
 *  @field[size_t*][xs] The elements of the vector.
 */
typedef struct {
  size_t size;
  size_t* xs;
} FbleTagV;

#endif // FBLE_INTERNAL_TAG_H_
