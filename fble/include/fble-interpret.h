// fble-interpret.h
//   Public header for fble interpreter related types and functions.

#ifndef FBLE_INTERPRET_H_
#define FBLE_INTERPRET_H_

#include "fble-alloc.h"
#include "fble-compile.h"
#include "fble-link.h"

// FbleInterpretCode --
//   Turn code into an executable based on use of an interpreter to interpret
//   the compiled code.
//
// Inputs:
//   arena - arena to use for allocations.
//   code - the compiled code to interpret.
//
// Results:
//   An executable for the code.
//
// Side effects:
//   Allocates an FbleExecutable that should be freed using (TODO) when no
//   longer needed.
FbleExecutable* FbleInterpretCode(FbleArena* arena, FbleCode* code);

// FbleInterpret --
//   Turn a compiled program into an executable program based on use of an
//   interpreter to interpret the compiled code.
//
// Inputs:
//   arena - arena to use for allocations.
//   program - the compiled program to interpret.
//
// Results:
//   An executable program.
//
// Side effects:
//   Allocates an FbleExecutableProgram that should be freed using
//   FbleFreeExecutableProgram when no longer needed.
FbleExecutableProgram* FbleInterpret(FbleArena* arena, FbleCompiledProgram* program);

#endif // FBLE_INTERPRET_H_

