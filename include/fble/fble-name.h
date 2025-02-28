/**
 * @file fble-name.h
 *  FbleName API.
 */

#ifndef FBLE_NAME_H_
#define FBLE_NAME_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <sys/types.h>    // for size_t

#include "fble-loc.h"     // for FbleLoc
#include "fble-string.h"  // for FbleString

/**
 * @enum[FbleNameSpace] Enum used to distinguish among different name spaces.
 *  @field[FBLE_NORMAL_NAME_SPACE] Namespace for normal values.
 *  @field[FBLE_TYPE_NAME_SPACE] Namespace for type values.
 */
typedef enum {
  FBLE_NORMAL_NAME_SPACE,
  FBLE_TYPE_NAME_SPACE,
} FbleNameSpace;

/**
 * @struct[FbleName] A type or variable name.
 *  Along with its associated location in a source file. The location is
 *  typically used for error reporting purposes.
 *
 *  Pass by value. Explicit copy and free required.
 *
 *  The name and loc fields are owned by this FbleName.
 *
 *  @field[FbleString*][name] The name.
 *  @field[FbleNameSpace][space] Namespace of the name.
 *  @field[FbleLoc][loc] Location of name's occurence.
 */
typedef struct {
  FbleString* name;
  FbleNameSpace space;
  FbleLoc loc;
} FbleName;

/**
 * @struct[FbleNameV] Vector of FbleNames.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleName*][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleName* xs;
} FbleNameV;

/**
 * @func[FbleCopyName] Copies an FbleName.
 *  @arg[FbleName][name] The name to copy.
 *
 *  @returns FbleName
 *   A (possibly shared) copy of the name.
 *
 *  @sideeffects
 *   The user should call FbleFreeName on the returned name when it is no
 *   longer needed.
 */
FbleName FbleCopyName(FbleName name);

/**
 * @func[FbleFreeName] Frees an FbleName.
 *  @arg[FbleName][name] The name to free resources of.
 *
 *  @sideeffects
 *   Frees resources associated with the name.
 */
void FbleFreeName(FbleName name);

/**
 * @func[FbleNamesEqual] Tests if two names are equals.
 *  Two names are considered equal if they have the same name and belong to
 *  the same namespace. Location is not relevant for this check.
 *
 *  @arg[FbleName][a] The first name.
 *  @arg[FbleName][b] The second name.
 *  
 *  @returns bool
 *   true if the first name equals the second, false otherwise.
 *  
 *  @sideeffects
 *   None.
 */
bool FbleNamesEqual(FbleName a, FbleName b);

/**
 * @func[FblePrintName] Prints an FbleName in human readable form.
 *  @arg[FILE*   ][stream] The stream to print to.
 *  @arg[FbleName][name  ] The name to print.
 *
 *  @sideeffects
 *   Outputs the name to the given file.
 */
void FblePrintName(FILE* stream, FbleName name);

#endif // FBLE_NAME_H_
