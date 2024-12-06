/**
 * @file fble-compile.h
 *  Fble Compiler API.
 */

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdio.h>        // for FILE

#include "fble-program.h"

/**
 * @func[FbleCompileModule] Compiles a module.
 *  @arg[FbleProgram*][program] The program whose main module to compile.
 *
 *  @returns bool
 *   True on success, false if the program is not well typed.
 *
 *  @sideeffects
 *   @i Updates code and profile_blocks fields of the main module.
 *   @i Prints warning messages to stderr.
 *   @i Prints a message to stderr if the program fails to compile.
 */
bool FbleCompileModule(FbleProgram* program);

/**
 * @func[FbleCompileProgram] Compiles a program.
 *  Type check and compile all modules of the given program.
 *
 *  @arg[FbleProgram*][program] The program to compile.
 *
 *  @returns bool
 *   True on success, false if the program is not well typed.
 *
 *  @sideeffects
 *   @i Updates code and profile_blocks fields of the program modules.
 *   @i Prints warning messages to stderr.
 *   @i Prints a message to stderr if the program fails to compile.
 */
bool FbleCompileProgram(FbleProgram* program);

/**
 * @func[FbleDisassemble] Dissassembles a compiled module.
 *  Writes a disassembled version of the given module in human readable format
 *  to the given file. This is for debugging purposes only. The outupt is
 *  implementation dependant and subject to change.
 *
 *  @arg[FILE*] fout
 *   The file to write the disassembled program to.
 *  @arg[FbleModule*] module
 *   The module to disassemble.
 *
 *  @sideeffects
 *   @i A disassembled version of the module is printed to fout.
 *   @i Behavior is undefined if the module hasn't been compiled yet.
 */
void FbleDisassemble(FILE* fout, FbleModule* module);

#endif // FBLE_COMPILE_H_
