/**
 * @file fble-loc.h
 * Fble source locations API.
 */

#ifndef FBLE_LOC_H_
#define FBLE_LOC_H_

#include <sys/types.h>    // for size_t

#include "fble-string.h"  // for FbleString

/**
 * Source file location.
 *
 * Pass by value. Explicit copy and free required.
 */
typedef struct {
  /**
   * Source file name. Or other description of the source of the program text.
   * Owned by this FbleLoc.
   */
  FbleString* source;

  /**
   * The line number.
   */
  int line;

  /**
   * The column number.
   */
  int col;
} FbleLoc;

/**
 * Creates a new FbleLoc.
 *
 * Convenience function for creating an FbleLoc from a char* source name.
 *
 * @param source  The name of the source file for the location. Borrowed.
 * @param line    The line within the file.
 * @param col     The column within the line.
 *
 * @returns
 *   An FbleLoc with given source, line, and col. A copy of the source string
 *   is made.
 *
 * @sideeffects
 *   Allocates resources for an FbleLoc that should be freed using FbleFreeLoc
 *   when no longer needed.
 */
FbleLoc FbleNewLoc(const char* source, int line, int col);

/**
 * Copies an Fble Loc
 *
 * @param loc  The loc to copy.
 *
 * @returns
 *   A (possibly shared) copy of the loc.
 *
 * @sideeffects
 *   The user should call FbleFreeLoc on the returned loc when it is no longer
 *   needed.
 */
FbleLoc FbleCopyLoc(FbleLoc loc);

/**
 * Frees an FbleLoc.
 *
 * @param loc   The location to free resources of.
 *
 * @sideeffects
 *   Frees resources associated with the given loc.
 */
void FbleFreeLoc(FbleLoc loc);

/**
 * Outputs a compiler warning.
 *
 * Reports a warning message associated with a location in a source file.
 *
 * @param format  A printf format string for the warning message.
 * @param loc     The location of the warning message to report.
 * @param ...     Format arguments as specified by the format string.
 *
 * @sideeffects
 *   Prints a warning message to stderr with error location.
 */
void FbleReportWarning(const char* format, FbleLoc loc, ...);

/**
 * Outputs a compiler error.
 *
 * Reports an error message associated with a location in a source file.
 *
 * @param format  A printf format string for the error message.
 * @param loc     The location of the error message to report.
 * @param ...     Format arguments as specified by the format string.
 *
 * @sideeffects
 *   Prints an error message to stderr with error location.
 */
void FbleReportError(const char* format, FbleLoc loc, ...);

#endif // FBLE_LOC_H_
