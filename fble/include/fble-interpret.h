// fble-interpret.h
//   Public header for fble interpreter related types and functions.

#ifndef FBLE_INTERPRET_H_
#define FBLE_INTERPRET_H_

#include "fble-alloc.h"
#include "fble-compile.h"
#include "fble-link.h"
#include "fble-value.h"

// FbleInterpret --
//   Turn a compiled program into an executable program based on use of an
//   interpreter to interpret the compiled code.
//
// Inputs:
//   program - the compiled program to interpret.
//
// Results:
//   An executable program.
//
// Side effects:
//   Allocates an FbleExecutableProgram that should be freed using
//   FbleFreeExecutableProgram when no longer needed.
FbleExecutableProgram* FbleInterpret(FbleCompiledProgram* program);

#endif // FBLE_INTERPRET_H_

