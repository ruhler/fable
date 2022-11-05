// kind.h --
//   Header file for FbleKind.

#ifndef FBLE_INTERNAL_KIND_H_
#define FBLE_INTERNAL_KIND_H_

#include <sys/types.h>    // for size_t

#include <fble/fble-loc.h>     // for FbleLoc

// FbleKindTag --
//   A tag used to distinguish between the two kinds of kinds.
typedef enum {
  FBLE_BASIC_KIND,
  FBLE_POLY_KIND
} FbleKindTag;

// FbleKind --
//   A tagged union of kind types. All kinds have the same initial
//   layout as FbleKind. The tag can be used to determine what kind of
//   kind this is to get access to additional fields of the kind
//   by first casting to that specific type of kind.
//
// Kinds are non-cyclcically reference counted. Use FbleCopyKind and
// FbleFreeKind to manage the reference count and memory associated with
// the kind.
typedef struct {
  FbleKindTag tag;
  FbleLoc loc;
  int refcount;
} FbleKind;

// FbleKindV --
//   A vector of FbleKind.
typedef struct {
  size_t size;
  FbleKind** xs;
} FbleKindV;

// FbleBasicKind --
//   FBLE_BASIC_KIND (level :: size_t)
//
// levels
//   0: A normal, non-type value.
//   1: A normal type. A type of a level 0.
//   2: A type of a type of a value.
//   3: A type of a type of a type of a value.
//   etc.
typedef struct {
  FbleKind _base;
  size_t level;
} FbleBasicKind;

// FblePolyKind --
//   FBLE_POLY_KIND (arg :: Kind) (return :: Kind)
typedef struct {
  FbleKind _base;
  FbleKind* arg;
  FbleKind* rkind;
} FblePolyKind;

// FbleCopyKind --
//   Makes a (refcount) copy of a kind.
//
// Inputs:
//   kind - the kind to copy.
//
// Results:
//   The copied kind.
//
// Side effects:
//   The returned kind must be freed using FbleFreeKind when no longer in
//   use.
FbleKind* FbleCopyKind(FbleKind* kind);

// FbleFreeKind --
//   Frees a (refcount) copy of a compiled kind.
//
// Inputs:
//   kind - the kind to free. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the refcount for the kind and frees it if there are no
//   more references to it.
void FbleFreeKind(FbleKind* kind);

// FbleDataTypeTag --
//   Enum used to distinguish between different kinds of data types.
typedef enum {
  FBLE_STRUCT_DATATYPE,
  FBLE_UNION_DATATYPE
} FbleDataTypeTag;

#endif // FBLE_INTERNAL_KIND_H_
