/**
 * @file fble-compile.h
 *  Fble Compiler API.
 */

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdio.h>        // for FILE

#include "fble-load.h"
#include "fble-module-path.h"
#include "fble-profile.h"

/** Compiled Fble bytecode. */
typedef struct FbleCode FbleCode;

/**
 * A compiled module.
 */
typedef struct {
  /**
   * Path to the module.
   */
  FbleModulePath* path;

  /**
   * List of distinct modules this module depends on.
   */
  FbleModulePathV deps;

  /**
   * Code to compute the module's value.
   *
   * The code should be suitable for use in the body of a function that takes
   * the computed module values for each module listed in 'deps' as
   * arguments to the function
   */
  FbleCode* code;

  /**
   * Profile blocks used by functions in the module.
   *
   * This FbleCompiledModule owns the names and the vector.
   */
  FbleNameV profile_blocks;
} FbleCompiledModule;

/** Vector of compiled modules. */
typedef struct {
  size_t size;              /**< Number of elements. */
  FbleCompiledModule** xs;  /**< Elements. */
} FbleCompiledModuleV;

/**
 * A compiled program.
 *
 * The program is represented as a list of compiled modules in topological
 * dependency order. Later modules in the list may depend on earlier modules
 * in the list, but not the other way around.
 *
 * The last module in the list is the main program. The module path for the
 * main module is /%.
 */
typedef struct {
  FbleCompiledModuleV modules;  /**< List of compiled modules. */
} FbleCompiledProgram;

/**
 * @func[FbleFreeCompiledModule] Frees an FbleCompiledModule.
 *  @arg[FbleCopmiledModule*][module] The module to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeCompiledModule(FbleCompiledModule* module);

/**
 * @func[FbleFreeCompiledProgram] Frees an FbleCompiledProgram.
 *  @arg[FbleCompiledProgram*][program] The program to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeCompiledProgram(FbleCompiledProgram* program);

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
