/**
 * @file fble-loc.h
 *  Fble source locations API.
 */

#ifndef FBLE_LOC_H_
#define FBLE_LOC_H_

#include <sys/types.h>    // for size_t

#include "fble-string.h"  // for FbleString

/**
 * @struct[FbleLoc] Source file location
 *  Pass by value. Explicit copy and free required.
 *
 *  @field[FbleString*][source]
 *   Source file name. Or other description of the source of the program text.
 *   Owned by this FbleLoc.
 *  @field[size_t][line] The line number.
 *  @field[size_t][col] The column number.
 */
typedef struct {
  FbleString* source;
  size_t line;
  size_t col;
} FbleLoc;

/**
 * @struct[FbleLocV] Vector of FbleLoc.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleLoc*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleLoc* xs;
} FbleLocV;

/**
 * @func[FbleNewLoc] Creates a new FbleLoc.
 *  Convenience function for creating an FbleLoc from a char* source name.
 *
 *  @arg[const char*] source
 *   The name of the source file for the location. Borrowed.
 *  @arg[size_t][line] The line within the file.
 *  @arg[size_t][col ] The column within the line.
 *
 *  @returns FbleLoc
 *   An FbleLoc with given source, line, and col. A copy of the source string
 *   is made.
 *
 *  @sideeffects
 *   Allocates resources for an FbleLoc that should be freed using FbleFreeLoc
 *   when no longer needed.
 */
FbleLoc FbleNewLoc(const char* source, size_t line, size_t col);

/**
 * @func[FbleCopyLoc] Copies an Fble Loc
 *  @arg[FbleLoc][loc] The loc to copy.
 *
 *  @returns FbleLoc
 *   A (possibly shared) copy of the loc.
 *
 *  @sideeffects
 *   The user should call FbleFreeLoc on the returned loc when it is no longer
 *   needed.
 */
FbleLoc FbleCopyLoc(FbleLoc loc);

/**
 * @func[FbleFreeLoc] Frees an FbleLoc.
 *  @arg[FbleLoc][loc] The location to free resources of.
 *
 *  @sideeffects
 *   Frees resources associated with the given loc.
 */
void FbleFreeLoc(FbleLoc loc);

/**
 * @func[FbleReportWarning] Outputs a compiler warning.
 *  Reports a warning message associated with a location in a source file.
 *
 *  @arg[const char*] format
 *   A printf format string for the warning message.
 *  @arg[FbleLoc] loc
 *   The location of the warning message to report.
 *  @arg[...] 
 *   Format arguments as specified by the format string.
 *  
 *  @sideeffects
 *   Prints a warning message to stderr with error location.
 */
void FbleReportWarning(const char* format, FbleLoc loc, ...);

/**
 * @func[FbleReportError] Outputs a compiler error.
 *  Reports an error message associated with a location in a source file.
 *
 *  @arg[const char*] format
 *   A printf format string for the error message.
 *  @arg[FbleLoc] loc
 *   The location of the error message to report.
 *  @arg[...] 
 *   Format arguments as specified by the format string.
 *
 *  @sideeffects
 *   Prints an error message to stderr with error location.
 */
void FbleReportError(const char* format, FbleLoc loc, ...);

#endif // FBLE_LOC_H_
