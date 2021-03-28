// fble-syntax.h --
//   Header file for the publically exposed fble abstract syntax.

#ifndef FBLE_SYNTAX_H_
#define FBLE_SYNTAX_H_

#include "fble-name.h"

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

// FbleParseModulePath --
//   Parse an FbleModulePath from a string.
//
// Inputs:
//   arena - The arena to use for allocating the parsed path.
//   string - The string to parse the path from.
//
// Results:
//   The parsed path, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the path cannot be parsed.
FbleModulePath* FbleParseModulePath(FbleArena* arena, const char* string);

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
//   arena - the arena to use for allocations.
//   path - the path to free.
//
// Side effects:
//   Frees resources associated with the path and its contents.
void FbleFreeModulePath(FbleArena* arena, FbleModulePath* path);

// FbleExpr --
//   Abstract type representing an fble expression.
typedef struct FbleExpr FbleExpr;

// FbleModule --
//   Represents an individual module.
// 
// Fields:
//   path - the path to the module.
//   deps - a list of distinct modules this module depends on.
//   value - the value of the module
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExpr* value;
} FbleModule;

// FbleModuleV -- A vector of modules
typedef struct {
  size_t size;
  FbleModule* xs;
} FbleModuleV;

// FbleProgram --
//   Represents a complete parsed and loaded fble program.
//
// The program is represented as a list of dependant module in topological
// dependancy order. Later modules in the list may depend on earlier modules
// in the list, but not the other way around.
//
// The last module in the list is the main program. The module path for the
// main module is the empty path /%.
typedef struct {
  FbleModuleV modules;
} FbleProgram;

// FbleLoad --
//   Load an fble program.
//
// Inputs:
//   arena - The arena to use for allocating the parsed program.
//   filename - The name of the file to parse the program from.
//   root - The directory to search for modules in. May be NULL.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
// * If deps is not NULL, fills in deps with the list of files read while
//   loading the program. This can be used to get dependencies for use with
//   build systems.
// * Prints an error message to stderr if the program cannot be parsed.
//
// Allocations:
// * The user should call FbleFreeProgram to free resources associated with
//   the given program when it is no longer needed.
//
// Note:
//   A copy of the filename will be made for use in locations. The user need
//   not ensure that filename remains valid for the duration of the lifetime
//   of the program.
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root);

// FbleFreeProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeProgram(FbleArena* arena, FbleProgram* program);

#endif // FBLE_SYNTAX_H_
