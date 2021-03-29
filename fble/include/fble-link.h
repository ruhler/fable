// fble-link.h --
//   Public header file for fble link related functions.

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-compile.h"
#include "fble-profile.h"
#include "fble-value.h"

// FbleLink --
//   Link the modules of a compiled program together into a single FbleValue
//   representing a zero-argument function that can be used to compute the
//   value of the program.
//
// Inputs:
//   heap - heap to use for allocations.
//   program - the compiled program to link.
//
// Results:
//   An FbleValue representing a zero-argument function that can be used to
//   compute the value of the program.
//
// Side effects:
//   Allocates an FbleValue* that should be freed using FbleReleaseValue when
//   no longer needed.
FbleValue* FbleLink(FbleValueHeap* heap, FbleCompiledProgram* program);

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
