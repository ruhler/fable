/**
 * @file interpret.h
 *  Header for fble interpreter related functions.
 */

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include <fble/fble-compile.h>    // For FbleCompiledProgram
#include <fble/fble-function.h>   // for FbleFunction, etc.
#include <fble/fble-link.h>       // For FbleExecutableModule

/**
 * An FbleRunFunction for interpreting FbleCode fble bytecode.
 * See documentation of FbleRunFunction in fble-function.h.
 */
FbleValue* FbleInterpreterRunFunction(
    FbleValueHeap* heap,
    FbleProfileThread* profile,
    FbleValue** tail_call_buffer,
    FbleFunction* function,
    FbleValue** args);

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

#endif // FBLE_INTERNAL_INTERPRET_H_

