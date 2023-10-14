/**
 * @file fble-interpret.h
 *  Fble interpreter API.
 */

#ifndef FBLE_INTERPRET_H_
#define FBLE_INTERPRET_H_

#include "fble-compile.h"
#include "fble-execute.h"

/**
 * @func[FbleInterpret] Creates an interpreter-based FbleExecutableProgram.
 *  Turns a compiled program into an executable program based on use of an
 *  interpreter to interpret the compiled code.
 *
 *  @arg[FbleCompiledProgram*][program] The compiled program to interpret.
 *
 *  @returns FbleExecutableProgram*
 *   An executable program.
 *
 *  @sideeffects
 *   Allocates an FbleExecutableProgram that should be freed using
 *   FbleFreeExecutableProgram when no longer needed.
 */
FbleExecutableProgram* FbleInterpret(FbleCompiledProgram* program);

#endif // FBLE_INTERPRET_H_
