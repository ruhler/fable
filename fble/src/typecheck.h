// typecheck.h --
//   Header file for fble type checking.

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include "fble-value.h"
#include "fble-syntax.h"

// FbleTypeCheck -- 
//   Run typecheck on the given program.
//
// Inputs:
//   arena - arena to use for allocations
//   program - the program to typecheck
//
// Result:
//   The type checked program, or NULL if the program failed to type check.
//
// Side effects:
// * Prints messages to stderr in case of failure to type check.
FbleValue* FbleTypeCheck(FbleValueHeap* heap, FbleProgram* program);

#endif // FBLE_INTERNAL_TYPECHECK_H_
