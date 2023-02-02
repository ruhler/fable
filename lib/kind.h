/**
 * @file kind.h
 * Header for FbleKind.
 */

#ifndef FBLE_INTERNAL_KIND_H_
#define FBLE_INTERNAL_KIND_H_

#include <sys/types.h>    // for size_t

#include <fble/fble-loc.h>     // for FbleLoc

/**
 * Different kinds of fble kinds.
 */
typedef enum {
  FBLE_BASIC_KIND,
  FBLE_POLY_KIND
} FbleKindTag;

/**
 * FbleKind base class.
 *
 * A tagged union of kind types. All kinds have the same initial layout as
 * FbleKind. The tag can be used to determine what kind of kind this is to get
 * access to additional fields of the kind by first casting to that specific
 * type of kind.
 *
 * Kinds are non-cyclcically reference counted. Use FbleCopyKind and
 * FbleFreeKind to manage the reference count and memory associated with
 * the kind.
 */
typedef struct {
  FbleKindTag tag;    /**< The kind of this kind. */
  FbleLoc loc;        /**< Location for error reporting. */
  int refcount;       /**< Reference count. */
} FbleKind;

/** Vector of FbleKind. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleKind** xs;    /**< The elements. */
} FbleKindV;

/**
 * FBLE_BASIC_KIND: A basic kind.
 */
typedef struct {
  FbleKind _base;   /**< FbleKind base class. */

  /**
   * The level of the kind.
   *
   * 0: A normal, non-type value.
   * 1: A normal type. A type of a level 0.
   * 2: A type of a type of a value.
   * 3: A type of a type of a type of a value.
   * etc.
   */
  size_t level;
} FbleBasicKind;

/**
 * FBLE_POLY_KIND: A polymorphic kind.
 */
typedef struct {
  FbleKind _base;     /**< FbleKind base class. */
  FbleKind* arg;      /**< The kind argument. */
  FbleKind* rkind;    /**< The result kind. */
} FblePolyKind;

/**
 * Creates a basic kind of given level.
 *
 * @param loc  Location for the kind.
 * @param level  The level to use for the kind.
 * @returns  A basic kind of the given level.
 * @sideeffects
 *   Allocates a kind that should be freed using FbleFreeKind when no longer
 *   needed.
 */
FbleKind* FbleNewBasicKind(FbleLoc loc, size_t level);

/**
 * Makes a (refcount) copy of a kind.
 *
 * @param kind  The kind to copy.
 *
 * @returns
 *   The copied kind.
 *
 * @sideeffects
 *   The returned kind must be freed using FbleFreeKind when no longer in
 *   use.
 */
FbleKind* FbleCopyKind(FbleKind* kind);

/**
 * Frees a (refcount) copy of a compiled kind.
 *
 * @param kind  The kind to free. May be NULL.
 *
 * @sideeffects
 *   Decrements the refcount for the kind and frees it if there are no
 *   more references to it.
 */
void FbleFreeKind(FbleKind* kind);

/**
 * Distinguishes between struct versus union datatype.
 */
typedef enum {
  FBLE_STRUCT_DATATYPE,
  FBLE_UNION_DATATYPE
} FbleDataTypeTag;

#endif // FBLE_INTERNAL_KIND_H_
