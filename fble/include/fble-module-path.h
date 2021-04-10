// fble-module-path.h --
//   Header file for the FbleModulePath type.

#ifndef FBLE_MODULE_PATH_H_
#define FBLE_MODULE_PATH_H_

#include "fble-loc.h"    // for FbleLoc
#include "fble-name.h"    // for FbleName

// FbleModulePath --
//   A module path, such as /Foo/Bar%.
//
// Pass by pointer. Explicitly copy and free required.
//
// By convention, all names in the path belong to the FBLE_NORMAL_NAME_SPACE.
// 
// The magic field is set to FBLE_MODULE_PATH_MAGIC and is used to detect
// double frees of FbleModulePath.
#define FBLE_MODULE_PATH_MAGIC 0x77806584
typedef struct {
  size_t refcount;
  size_t magic;
  FbleLoc loc;
  FbleNameV path;
} FbleModulePath;

// FbleModulePathV -- A vector of FbleModulePath.
typedef struct {
  size_t size;
  FbleModulePath** xs;
} FbleModulePathV;

// FbleNewModulePath --
//   Allocate a new, empty module path.
//
// Inputs:
//   loc - the location of the module path. Borrowed.
//
// Results:
//   A newly allocated empty module path.
//
// Side effects:
//   Allocates a new module path that the user should free with
//   FbleFreeModulePath when no longer needed.
FbleModulePath* FbleNewModulePath(FbleLoc loc);

// FbleModulePathName --
//   Construct an FbleName describing a module path.
//
// Inputs:
//   path - the path to construct an FbleName for.
//
// Results:
//   An FbleName describing the full module path. For example: "/Foo/Bar%".
//
// Side effects:
//   The caller should call FbleFreeName on the returned name when no longer
//   needed.
FbleName FbleModulePathName(FbleModulePath* path);

// FblePrintModulePath --
//   Print a module path in human readable form to the given stream.
//
// Inputs:
//   stream - the stream to print to
//   path - the path to print
//
// Results:
//   none.
//
// Side effects:
//   Prints the given path to the given stream.
void FblePrintModulePath(FILE* stream, FbleModulePath* path);

// FbleModulePathsEqual --
//   Test whether two paths are equal. Two paths are considered equal if they
//   have the same sequence of module names. Locations are not relevant for
//   this check.
//
// Inputs:
//   a - The first path.
//   b - The second path.
//
// Results:
//   true if the first path equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbleModulePathsEqual(FbleModulePath* a, FbleModulePath* b);

// FbleParseModulePath --
//   Parse an FbleModulePath from a string.
//
// Inputs:
//   string - The string to parse the path from.
//
// Results:
//   The parsed path, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the path cannot be parsed.
FbleModulePath* FbleParseModulePath(const char* string);

// FbleCopyModulePath -- 
//   Make a (possibly shared) copy of the given module path.
//
// Inputs:
//   path - the path to copy.
// 
// Results:
//   The new (possibly shared) copy of the path.
//
// Side effects:
//   The user should arrange for FbleFreeModulePath to be called on this path
//   copy when it is no longer needed.
FbleModulePath* FbleCopyModulePath(FbleModulePath* path);

// FbleFreeModulePath --
//   Free resource associated with a module path.
//
// Inputs:
//   path - the path to free.
//
// Side effects:
//   Frees resources associated with the path and its contents.
void FbleFreeModulePath(FbleModulePath* path);

#endif // FBLE_MODULE_PATH_H_
