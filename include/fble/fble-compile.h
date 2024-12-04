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
 *  @arg[FbleLoadedProgram*][program] The program to compile.
 *
 *  @returns FbleCompiledModule*
 *   The compiled module, or NULL if the program is not well typed.
 *
 *  @sideeffects
 *   @i Prints warning messages to stderr.
 *   @i Prints a message to stderr if the program fails to compile.
 *   @item
 *    The caller should call FbleFreeCompiledModule to release resources
 *    associated with the returned module when it is no longer needed.
 */
FbleCompiledModule* FbleCompileModule(FbleLoadedProgram* program);

/**
 * @func[FbleCompileProgram] Compiles a program.
 *  Type check and compile all modules of the given program.
 *
 *  @arg[FbleLoadedProgram*][program] The program to compile.
 *
 *  @returns FbleCompiledProgram
 *   The compiled program, or NULL if the program is not well typed.
 *
 *  @sideeffects
 *   @i Prints warning messages to stderr.
 *   @i Prints a message to stderr if the program fails to compile.
 *   @item
 *    The caller should call FbleFreeCompiledProgram to release resources
 *    associated with the returned program when it is no longer needed.
 */
FbleCompiledProgram* FbleCompileProgram(FbleLoadedProgram* program);

/**
 * @func[FbleDisassemble] Dissassembles a compiled module.
 *  Writes a disassembled version of the given module in human readable format
 *  to the given file. This is for debugging purposes only. The outupt is
 *  implementation dependant and subject to change.
 *
 *  @arg[FILE*] fout
 *   The file to write the disassembled program to.
 *  @arg[FbleCompiledModule*] module
 *   The module to disassemble.
 *
 *  @sideeffects
 *   A disassembled version of the module is printed to fout.
 */
void FbleDisassemble(FILE* fout, FbleCompiledModule* module);

#endif // FBLE_COMPILE_H_
