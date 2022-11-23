// fble-name.h --
//   Header file for fble name and associated types.

#ifndef FBLE_NAME_H_
#define FBLE_NAME_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <sys/types.h>    // for size_t

#include "fble-loc.h"     // for FbleLoc
#include "fble-string.h"  // for FbleString

/** Enum used to distinguish among different name spaces. */
typedef enum {
  FBLE_NORMAL_NAME_SPACE,
  FBLE_TYPE_NAME_SPACE,
} FbleNameSpace;

/**
 * A type or variable name.
 *
 * Along with its associated location in a source file. The location is
 * typically used for error reporting purposes.
 *
 * Pass by value. Explicit copy and free required.
 *
 * The name and loc fields are owned by this FbleName.
 */
typedef struct {
  FbleString* name;     /**< The name. */
  FbleNameSpace space;  /**< Namespace of the name. */
  FbleLoc loc;          /**< Location of name's occurrence. */
} FbleName;

/** Vector of FbleNames. */
typedef struct {
  size_t size;    /**< Number of elements. */
  FbleName* xs;   /**< Elements. */
} FbleNameV;

// FbleCopyName --
//   Make a (possibly shared) copy of the name.
//
// Inputs:
//   name - the name to copy.
//
// Results:
//   A (possibly shared) copy of the name.
//
// Side effects:
//   The user should call FbleFreeName on the returned name when it is no
//   longer needed.
FbleName FbleCopyName(FbleName name);

// FbleFreeName --
//   Free resources associated with a name.
//
// Inputs:
//   name - the name to free resources of.
//
// Side effects:
//   Frees resources associated with the name.
void FbleFreeName(FbleName name);

// FbleNamesEqual --
//   Test whether two names are equal. Two names are considered equal if they
//   have the same name and belong to the same namespace. Location is not
//   relevant for this check.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the first name equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbleNamesEqual(FbleName a, FbleName b);

// FblePrintName --
//   Print a name in human readable form to the given stream.
//
// Inputs:
//   stream - the stream to print to
//   name - the name to print
//
// Results:
//   none.
//
// Side effects:
//   Prints the given name to the given stream.
void FblePrintName(FILE* stream, FbleName name);

#endif // FBLE_NAME_H_
