/**
 * @file var.h
 *  Data types for describing variables.
 */

#ifndef FBLE_INTERNAL_VAR_H_
#define FBLE_INTERNAL_VAR_H_

#include <stddef.h>    // for size_t

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
 * @struct[FbleVar] Identifies a variable in scope.
 *  @field[FbleVarTag][tag] The kind of variable.
 *  @field[size_t][index] Where to find the variable on the stack frame.
 */
typedef struct {
  FbleVarTag tag;
  size_t index;
} FbleVar;

/**
 * @struct[FbleVarV] Vector of FbleVar
 *  @field[size_t][size] Number of elements.
 *  @field[FbleVar*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleVar* xs;
} FbleVarV;

/** Identifies a local variable in scope.  */
typedef size_t FbleLocalIndex;

/**
 * @struct[FbleLocalIndexV] Vector of FbleLocalIndex
 *  @field[size_t][size] Number of elements.
 *  @field[FbleLocalIndex*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleLocalIndex* xs;
} FbleLocalIndexV;

#endif // FBLE_INTERNAL_VAR_H_
