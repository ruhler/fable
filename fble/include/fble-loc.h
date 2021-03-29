// fble-loc.h --
//   Header file for FbleLoc type.

#ifndef FBLE_LOC_H_
#define FBLE_LOC_H_

#include <sys/types.h>    // for size_t

#include "fble-alloc.h"   // for FbleArena
#include "fble-string.h"  // for FbleString

// FbleLoc --
//   Represents a location in a source file.
//
// Pass by value. Explicit copy and free required.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text. Owned by this FbleLoc.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  FbleString* source;
  int line;
  int col;
} FbleLoc;

// FbleCopyLoc --
//   Make a (possibly shared) copy of a location.
//
// Inputs:
//   arena - arena to use for allocations.
//   loc - the loc to copy.
//
// Result:
//   A (possibly shared) copy of the loc.
//
// Side effects:
//   The user should call FbleFreeLoc on the returned loc when it is no longer
//   needed.
FbleLoc FbleCopyLoc(FbleArena* arena, FbleLoc loc);

// FbleFreeLoc --
//   Free resources associated with the given loc.
//
// Inputs:
//   arena - arena to use for allocations
//   loc - the location to free resources of.
//
// Side effects
//   Frees resources associated with the given loc.
void FbleFreeLoc(FbleArena* arena, FbleLoc loc);

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
