/**
 * @file fble-name.h
 * FbleName API.
 */

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

/**
 * Copies an FbleName.
 *
 * @param name  The name to copy.
 *
 * @returns
 *   A (possibly shared) copy of the name.
 *
 * @sideeffects
 *   The user should call FbleFreeName on the returned name when it is no
 *   longer needed.
 */
FbleName FbleCopyName(FbleName name);

/**
 * Frees an FbleName.
 *
 * @param name  The name to free resources of.
 *
 * @sideeffects
 *   Frees resources associated with the name.
 */
void FbleFreeName(FbleName name);

/**
 * Tests if two names are equals.
 *
 * Two names are considered equal if they have the same name and belong to the
 * same namespace. Location is not relevant for this check.
 *
 * @param a   The first name.
 * @param b   The second name.
 *
 * @returns
 *   true if the first name equals the second, false otherwise.
 *
 * @sideeffects
 *   None.
 */
bool FbleNamesEqual(FbleName a, FbleName b);

/**
 * Prints an FbleName. In human readable form.
 *
 * @param stream  The stream to print to.
 * @param name    The name to print.
 *
 * @sideeffects
 *   Outputs the name to the given file.
 */
void FblePrintName(FILE* stream, FbleName name);

#endif // FBLE_NAME_H_
