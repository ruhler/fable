// fble-interpret.h
//   Public header for fble interpreter related types and functions.

#ifndef FBLE_INTERPRET_H_
#define FBLE_INTERPRET_H_

#include "fble-alloc.h"
#include "fble-compile.h"
#include "fble-link.h"
#include "fble-value.h"

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

// FbleNewInterpretedFuncValue -- see documentation in fble-interpret.h
//   Allocates a new FbleFuncValue that runs the given code with the
//   interpreter.
//
// Inputs:
//   heap - heap to use for allocations.
//   argc - the number of arguments to the function.
//   code - the code to run. Borrowed.
//
// Results:
//   A newly allocated FbleFuncValue that is fully initialized except for
//   statics.
//
// Side effects:
//   Allocates a new FbleFuncValue that should be freed using FbleReleaseValue
//   when it is no longer needed.
FbleFuncValue* FbleNewInterpretedFuncValue(FbleValueHeap* heap, size_t argc, FbleCode* code);

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

