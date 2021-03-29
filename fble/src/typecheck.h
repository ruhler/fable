// typecheck.h --
//   Header file for fble type checking.

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include "fble-load.h"
#include "tc.h"

// FbleTypeCheck -- 
//   Run typecheck on the given program.
//
// An FbleTc is produced for each module in the program. The FbleTc
// produced is a type checked expression suitable for use in the body of a
// function that takes the computed module values for each module listed in
// module->deps as arguments to the function.
//
// Inputs:
//   arena - arena to use for allocations
//   program - the program to typecheck
//   result - output variable assumed to be a pre-initialized vector.
//
// Result:
//   true if successfull, false in case of failure to type check.
//
// Side effects:
// * Fills in result with type checked values in the same order as the modules
//   in the program.
// * Prints messages to stderr in case of failure to type check.
// * The user is responsible for freeing any values added to the result vector
//   when they are no longer needed, including in the case when FbleTypeCheck
//   fails.
bool FbleTypeCheck(FbleArena* arena, FbleLoadedProgram* program, FbleTcV* result);

#endif // FBLE_INTERNAL_TYPECHECK_H_
