// fble-load.h --
//   Header file for FbleLoad.

#ifndef FBLE_LOAD_H_
#define FBLE_LOAD_H_

#include "fble-module-path.h"

// FbleExpr --
//   Type used to represent an fble abstract syntax tree.
typedef struct FbleExpr FbleExpr;

// FbleLoadedModule --
//   Describes the abstract syntax for a particular module.
// 
// Fields:
//   path - the path to the module.
//   deps - a list of distinct modules this module depends on.
//   expr - the abstract syntax of the module.
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExpr* value;
} FbleLoadedModule;

// FbleLoadedModuleV -- A vector of FbleLoadedModule
typedef struct {
  size_t size;
  FbleLoadedModule* xs;
} FbleLoadedModuleV;

// FbleLoadedProgram --
//   Describes the abstract syntax for a full fble program.
//
// The program is represented as a list of dependant module in topological
// dependancy order. Later modules in the list may depend on earlier modules
// in the list, but not the other way around.
//
// The last module in the list is the main program. The module path for the
// main module is the empty path /%.
typedef struct {
  FbleLoadedModuleV modules;
} FbleLoadedProgram;

// FbleParse --
//   Parse an expression from a file.
//
// Inputs:
//   filename - The name of the file to parse the program from.
//   deps - Output param: A list of the modules that the parsed expression
//          references. Modules will appear at most once in the list.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the program cannot be parsed.
// * Appends module paths in the parsed expression to deps, which
//   is assumed to be a pre-initialized vector. The caller is responsible for
//   calling FbleFreeModulePath on each path when it is no longer needed.
FbleExpr* FbleParse(FbleString* filename, FbleModulePathV* deps);

// FbleLoad --
//   Load an fble program.
//
// Inputs:
//   filename - The name of the file to parse the program from.
//   root - The directory to search for modules in. May be NULL.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the program cannot be parsed.
//
// Allocations:
// * The user should call FbleFreeLoadedProgram to free resources associated
//   with the given program when it is no longer needed.
FbleLoadedProgram* FbleLoad(const char* filename, const char* root);

// FbleFreeLoadedProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeLoadedProgram(FbleLoadedProgram* program);

#endif // FBLE_LOAD_H_
