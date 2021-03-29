// fble-link.h --
//   Public header file for fble link related types and functions.

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-module-path.h"
#include "fble-profile.h"
#include "fble-value.h"

// FbleExecutable --
//   Abstract type representing executable code.
typedef struct FbleExecutable FbleExecutable;

// FbleExecutableModule --
//   Represents an executable module.
// 
// Fields:
//   path - the path to the module.
//   deps - a list of distinct modules this module depends on.
//   executable - code to compute the value of the module, suitable for use in
//          the body of a that function takes the computed module values for
//          each module listed in module->deps as arguments to the function.
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExecutable* executable;
} FbleExecutableModule;

// FbleExecutableModuleV -- A vector of FbleExecutableModule.
typedef struct {
  size_t size;
  FbleExecutableModule* xs;
} FbleExecutableModuleV;

// FbleExecutableProgram --
//   An executable program.
//
// The program is represented as a list of executable modules in topological
// dependancy order. Later modules in the list may depend on earlier modules
// in the list, but not the other way around.
//
// The last module in the list is the main program. The module path for the
// main module is /%.
typedef struct {
  FbleExecutableModuleV modules;
} FbleExecutableProgram;

// FbleFreeExecutableProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeExecutableProgram(FbleArena* arena, FbleExecutableProgram* program);

// FbleLink --
//   Link the modules of an executable program together into a single FbleValue
//   representing a zero-argument function that can be used to compute the
//   value of the program.
//
// Inputs:
//   heap - heap to use for allocations.
//   program - the executable program to link.
//
// Results:
//   An FbleValue representing a zero-argument function that can be used to
//   compute the value of the program.
//
// Side effects:
//   Allocates an FbleValue* that should be freed using FbleReleaseValue when
//   no longer needed.
FbleValue* FbleLink(FbleValueHeap* heap, FbleExecutableProgram* program);

// FbleLinkFromSource --
//   Load, compile, and link a full program from source.
//
// Inputs:
//   heap - heap to use for allocations.
//   filename - The name of the file to parse the program from.
//   root - The directory to search for modules in. May be NULL.
//   profile - profile to populate with blocks. May be NULL.
//
// Returns: 
//   A zero-argument function that computes the value of the program when
//   executed, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the program fails to load.
// * The user should call FbleReleaseValue on the returned value when it is no
//   longer needed.
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, const char* filename, const char* root, FbleProfile* profile);

#endif // FBLE_LINK_H_
