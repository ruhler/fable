/**
 * @file kind.h
 *  Header for FbleKind.
 */

#ifndef FBLE_INTERNAL_KIND_H_
#define FBLE_INTERNAL_KIND_H_

#include <stddef.h>    // for size_t

#include <fble/fble-loc.h>     // for FbleLoc

/**
 * Different kinds of fble kinds.
 */
typedef enum {
  FBLE_BASIC_KIND,
  FBLE_POLY_KIND
} FbleKindTag;

/**
 * @struct[FbleKind] FbleKind base class.
 *  A tagged union of kind types. All kinds have the same initial layout as
 *  FbleKind. The tag can be used to determine what kind of kind this is to
 *  get access to additional fields of the kind by first casting to that
 *  specific type of kind.
 *
 *  Kinds are non-cyclcically reference counted. Use FbleCopyKind and
 *  FbleFreeKind to manage the reference count and memory associated with the
 *  kind.
 *
 *  @field[FbleKindTag][tag] The kind of this kind.
 *  @field[FbleLoc][loc] Location for error reporting.
 *  @field[int][refcount] Reference count.
 */
typedef struct {
  FbleKindTag tag;
  FbleLoc loc;
  int refcount;
} FbleKind;

/**
 * @struct[FbleKindV] Vector of FbleKind.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleKind**][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleKind** xs;
} FbleKindV;

/**
 * @struct[FbleBasicKind] FBLE_BASIC_KIND: A basic kind.
 *  @field[FbleKind][_base] FbleKind base class.
 *  @field[size_t][level]
 *   The level of the kind.
 *   
 *   0: A normal, non-type value.
 *   1: A normal type. A type of a level 0.
 *   2: A type of a type of a value.
 *   3: A type of a type of a type of a value.
 *   etc.
 */
typedef struct {
  FbleKind _base;
  size_t level;
} FbleBasicKind;

/**
 * @struct[FblePolyKind] FBLE_POLY_KIND: A polymorphic kind.
 *  @field[FbleKind][_base] FbleKind base class.
 *  @field[FbleKind*][arg] The kind argument.
 *  @field[FbleKind*][rkind] The result kind.
 */
typedef struct {
  FbleKind _base;
  FbleKind* arg;
  FbleKind* rkind;
} FblePolyKind;

/**
 * @func[FbleNewBasicKind] Creates a basic kind of given level.
 *  @arg[FbleLoc][loc] Location for the kind.
 *  @arg[size_t][level] The level to use for the kind.
 * 
 *  @returns[FbleKind*]
 *   A basic kind of the given level.
 *
 *  @sideeffects
 *   Allocates a kind that should be freed using FbleFreeKind when no longer
 *   needed.
 */
FbleKind* FbleNewBasicKind(FbleLoc loc, size_t level);

/**
 * @func[FbleCopyKind] Makes a (refcount) copy of a kind.
 *  @arg[FbleKind*][kind] The kind to copy.
 *
 *  @returns[FbleKind*]
 *   The copied kind.
 *
 *  @sideeffects
 *   The returned kind must be freed using FbleFreeKind when no longer in
 *   use.
 */
FbleKind* FbleCopyKind(FbleKind* kind);

/**
 * @func[FbleFreeKind] Frees a (refcount) copy of a compiled kind.
 *  @arg[FbleKind*][kind] The kind to free. May be NULL.
 *
 *  @sideeffects
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
