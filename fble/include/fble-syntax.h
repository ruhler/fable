// fble-syntax.h --
//   Header file for the publically exposed fble abstract syntax.

#ifndef FBLE_SYNTAX_H_
#define FBLE_SYNTAX_H_

// FbleExpr --
//   Abstract type representing an fble expression.
typedef struct FbleExpr FbleExpr;

// FbleModule --
//   Represents an individual module.
// 
// Fields:
//   name - the canonical name of the module. This is the resolved path to the
//          module with "/" used as a separator. For example, the module Foo/Bar% has
//          name "Foo/Bar" in the MODULE name space.
//   value - the value of the module
typedef struct {
  FbleName name;
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
// The last module in the list is the main program. The name of the main
// module is undefined.
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
