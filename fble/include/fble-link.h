// fble-link.h --
//   Public header file for fble link related types and functions.

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-execute.h"
#include "fble-load.h"
#include "fble-profile.h"
#include "fble-value.h"

// FbleLink --
//   Link the modules of an executable program together into a single FbleValue
//   representing a zero-argument function that can be used to compute the
//   value of the program.
//
// Inputs:
//   heap - heap to use for allocations.
//   program - the executable program to link.
//   profile - profile to construct for the linked program. May be NULL.
//
// Results:
//   An FbleValue representing a zero-argument function that can be used to
//   compute the value of the program.
//
// Side effects:
//   Allocates an FbleValue that should be freed using FbleReleaseValue when
//   no longer needed.
FbleValue* FbleLink(FbleValueHeap* heap, FbleExecutableProgram* program, FbleProfile* profile);

// FbleLinkFromSource --
//   Load, compile, and link a full program from source.
//
// Inputs:
//   heap - heap to use for allocations.
//   search_path - The search path to use for locating .fble files.
//   module_path - The module path for the main module to load. Borrowed.
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
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, FbleSearchPath search_path, FbleModulePath* module_path, FbleProfile* profile);

// FbleCompiledModuleFunction --
//   The type of a module function generated for compiled .fble code.
//
// Inputs:
//   program - modules loaded into the program so far.
//
// Side effects:
//   Adds this module to the given program if it is not already in the
//   program.
typedef void FbleCompiledModuleFunction(FbleExecutableProgram* program);

// FbleLoadFromCompiled --
//   Load modules for a precompiled fble module.
//
// TODO: Should this be an internal, private function?
//
// Inputs:
//   program - the program to add the module and its dependencies to.
//   module - the compiled module to load. Borrowed.
//   depc - the number of other modules this module immediately depends on.
//   deps - the immediate dependencies of this module.
//
// Side effects:
// * Adds this module and any modules it depends on to the given program.
void FbleLoadFromCompiled(FbleExecutableProgram* program, FbleExecutableModule* module, size_t depc, FbleCompiledModuleFunction** deps);

// FbleLinkFromCompiled --
//   Load and link a precompiled fble program.
//
// Inputs:
//   module - the compiled main module function.
//   heap - heap to use for allocations.
//   profile - profile to populate with blocks. May be NULL.
//
// Returns: 
//   A zero-argument fble function that computes the value of the program when
//   executed.
//
// Side effects:
// * The user should call FbleReleaseValue on the returned value when it is no
//   longer needed.
FbleValue* FbleLinkFromCompiled(FbleCompiledModuleFunction* module, FbleValueHeap* heap, FbleProfile* profile);

// FbleLinkFromCompiledOrSource --
//   Load and link an fble program that is compiled or from source.
//
// This is a convenience function that attempts to load a compiled program
// if available, and if not, attempts to load from source.
//
// If module is non-NULL, loads from compiled. Otherwise loads from
// module_path.
//
// Inputs:
//   heap - heap to use for allocations.
//   profile - profile to populate with blocks. May be NULL.
//   module - the compiled main module function. May be NULL.
//   search_path - The search path to use for locating .fble files.
//   module_path - The module path for the main module to load.
//
// Returns: 
//   A zero-argument fble function that computes the value of the program when
//   executed, or NULL in case of error.
//
// Side effects:
// * The user should call FbleReleaseValue on the returned value when it is no
//   longer needed.
FbleValue* FbleLinkFromCompiledOrSource(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* module, FbleSearchPath search_path, const char* module_path);

#endif // FBLE_LINK_H_
