// fble-load.h --
//   Header file for FbleLoad.

#ifndef FBLE_LOAD_H_
#define FBLE_LOAD_H_

#include <stdio.h>  // for FILE

#include "fble-module-path.h"
#include "fble-string.h"

// FbleExpr --
//   Type used to represent an fble abstract syntax tree.
typedef struct FbleExpr FbleExpr;

// FbleLoadedModule --
//   Describes the abstract syntax for a particular module.
//
// Either one or both of 'type' and 'value' fields may be supplied. The
// 'value' field is required to run or generate code for the module. The type
// of the module can be determined either from the 'type' field or the type of
// the 'value' field. If both 'type' and 'value' are supplied, the typechecker
// will check that they describe the same type for the module.
// 
// Fields:
//   path - the path to the module.
//   deps - a list of distinct modules this module depends on.
//   type - the abstract syntax of the module type. May be NULL.
//   value - the abstract syntax of the module implementation. May be NULL.
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExpr* type;
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

// FbleSearchPath - 
//   A list of directories to use as the root of an fble file hierarchy for
//   locating .fble files corresponding to a module path.
//
// The directories are search in order for the first matching module.
//
// Note: FbleSearchPath is a vector of const char*.
typedef struct {
  size_t size;
  const char** xs;
} FbleSearchPath;

// FbleLoad --
//   Load an fble program.
//
// Inputs:
//   search_path - The search path to use for location .fble files. Borrowed.
//   module_path - The module path for the main module to load. Borrowed.
//   build_deps - Output to store list of files the load depended on.
//                This should be a preinitialized vector, or NULL.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the program cannot be parsed.
// * The user should call FbleFreeLoadedProgram to free resources associated
//   with the given program when it is no longer needed.
// * The user should free strings added to build_deps when no longer needed,
//   including in the case when program loading fails.
FbleLoadedProgram* FbleLoad(FbleSearchPath search_path, FbleModulePath* module_path, FbleStringV* build_deps);

// FbleFreeLoadedProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeLoadedProgram(FbleLoadedProgram* program);

// FbleSaveBuildDeps --
//   Save the list of build dependencies to a depfile suitable for use with
//   ninja or make.
//
// For example, it would generate something like:
//   target: build_deps1 build_deps2 build_deps3
//     build_deps4 build_deps5 ...
//
// Inputs:
//   fout - output stream to write the dependency file to.
//   target - the target of the dependencies to generate.
//   build_deps - the list of file dependencies.
//
// Side effects:
// * Creates a build dependency file.
// * Outputs an error message to stderr in case of error.
void FbleSaveBuildDeps(FILE* fout, const char* target, FbleStringV build_deps);

#endif // FBLE_LOAD_H_
