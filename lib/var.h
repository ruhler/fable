/**
 * @file var.h
 * Header for FbleVarSource.
 */

#ifndef FBLE_INTERNAL_VAR_H_
#define FBLE_INTERNAL_VAR_H_

#include <sys/types.h>    // for size_t

/**
 *   Where to find a variable.
 */
typedef enum {
  /**
   * For variables captured by a function from the scope where the function is
   * defined.
   */
  FBLE_STATIC_VAR,

  /**
   * For arguments passed to a function.
   *
   * TODO
   */
  // FBLE_ARG_VAR,

  /**
   * For local variables.
   */
  FBLE_LOCAL_VAR,
} FbleVarSource;

/**
 * Identifies a variable in scope.
 */
typedef struct {
  FbleVarSource source;
  size_t index;
} FbleVarIndex;

/** Vector of FbleVarIndex */
typedef struct {
  size_t size;
  FbleVarIndex* xs;
} FbleVarIndexV;

/** Identifies a local variable in scope.  */
typedef size_t FbleLocalIndex;

/** Vector of FbleLocalIndex */
typedef struct {
  size_t size;
  FbleLocalIndex* xs;
} FbleLocalIndexV;

#endif // FBLE_INTERNAL_VAR_H_
