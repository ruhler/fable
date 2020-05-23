// typecheck.h --
//   Header file for fble type checking.

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include "fble-alloc.h"
#include "syntax.h"

// FbleTypeCheck -- 
//   Run typecheck on the given program.
//
// Inputs:
//   arena - arena to use for allocations
//   program - the program to typecheck
//
// Result:
//   true if the program type checks successfully, false otherwise.
//
// Side effects:
// * Resolves field tags and MISC_*_EXPRs in the program.
// * Prints messages to stderr in case of failure to type check.
bool FbleTypeCheck(FbleArena* arena, FbleProgram* program);

#endif // FBLE_INTERNAL_TYPECHECK_H_
