/**
 * @file var.h
 *  Data types for describing variables.
 */

#ifndef FBLE_INTERNAL_VAR_H_
#define FBLE_INTERNAL_VAR_H_

#include <sys/types.h>    // for size_t

/**
 * Tag used to distinguish different kinds of variables.
 */
typedef enum {
  /**
   * For variables captured by a function from the scope where the function is
   * defined.
   */
  FBLE_STATIC_VAR,

  /**
   * For arguments passed to a function.
   */
  FBLE_ARG_VAR,

  /**
   * For local variables.
   */
  FBLE_LOCAL_VAR,
} FbleVarTag;

/**
 * Identifies a variable in scope.
 */
typedef struct {
  FbleVarTag tag;   /**< The kind of variable. */
  size_t index;     /**< Where to find the variable on the stack frame. */
} FbleVar;

/** Vector of FbleVar */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleVar* xs;      /**< The elements. */
} FbleVarV;

/** Identifies a local variable in scope.  */
typedef size_t FbleLocalIndex;

/** Vector of FbleLocalIndex */
typedef struct {
  size_t size;          /**< Number of elements. */
  FbleLocalIndex* xs;   /**< The elements. */
} FbleLocalIndexV;

#endif // FBLE_INTERNAL_VAR_H_
