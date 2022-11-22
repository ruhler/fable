/**
 * @file fble-loc.h
 * Header for FbleLoc type.
 */

#ifndef FBLE_LOC_H_
#define FBLE_LOC_H_

#include <sys/types.h>    // for size_t

#include "fble-string.h"  // for FbleString

/**
 * A location in a source file.
 *
 * Pass by value. Explicit copy and free required.
 *
 */
typedef struct {
  /**
   * Source file name. Or other description of the source of the program text.
   * Owned by this FbleLoc.
   */
  FbleString* source;

  /**
   * Line number for the location.
   */
  int line;

  /**
   * Column number of the location.
   */
  int col;
} FbleLoc;

// FbleNewLoc --
//   Convenience function for creating an FbleLoc from a char* source name.
//
// Inputs:
//   source - the name of the source file for the location. Borrowed.
//   line - the line within the file.
//   col - the column within the line.
//
// Results:
//   An FbleLoc with given source, line, and col. Copy of the source string is
//   made.
//
// Side effects:
//   Allocates resources for an FbleLoc that should be freed using FbleFreeLoc
//   when no longer needed.
FbleLoc FbleNewLoc(const char* source, int line, int col);

// FbleCopyLoc --
//   Make a (possibly shared) copy of a location.
//
// Inputs:
//   loc - the loc to copy.
//
// Result:
//   A (possibly shared) copy of the loc.
//
// Side effects:
//   The user should call FbleFreeLoc on the returned loc when it is no longer
//   needed.
FbleLoc FbleCopyLoc(FbleLoc loc);

// FbleFreeLoc --
//   Free resources associated with the given loc.
//
// Inputs:
//   loc - the location to free resources of.
//
// Side effects
//   Frees resources associated with the given loc.
void FbleFreeLoc(FbleLoc loc);

// FbleReportWarning --
//   Report a warning message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the warning message.
//   loc - The location of the warning message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints a warning message to stderr with error location.
void FbleReportWarning(const char* format, FbleLoc loc, ...);

// FbleReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
void FbleReportError(const char* format, FbleLoc loc, ...);

#endif // FBLE_LOC_H_
