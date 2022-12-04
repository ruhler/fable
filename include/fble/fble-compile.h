/**
 * @file fble-compile.h
 * Fble Compiler API.
 */

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdbool.h>      // for bool
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
 * Frees an FbleCompiledModule.
 *
 * @param module    The module to free. May be NULL.
 *
 * @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeCompiledModule(FbleCompiledModule* module);

/**
 * Frees an FbleCompiledProgram.
 *
 * @param program   The program to free. May be NULL.
 *
 * @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeCompiledProgram(FbleCompiledProgram* program);

/**
 * Compiles a module.
 *
 * @param program   The program to compile.
 *
 * @returns
 *   The compiled module, or NULL if the program is not well typed.
 *
 * @sideeffects
 * * Prints warning messages to stderr.
 * * Prints a message to stderr if the program fails to compile.
 * * The caller should call FbleFreeCompiledModule to release resources
 *   associated with the returned module when it is no longer needed.
 */
FbleCompiledModule* FbleCompileModule(FbleLoadedProgram* program);

/**
 * Compiles a program.
 *
 * Type check and compile all modules of the given program.
 *
 * @param program   The program to compile.
 *
 * @returns
 *   The compiled program, or NULL if the program is not well typed.
 *
 * @sideeffects
 * * Prints warning messages to stderr.
 * * Prints a message to stderr if the program fails to compile.
 * * The caller should call FbleFreeCompiledProgram to release resources
 *   associated with the returned program when it is no longer needed.
 */
FbleCompiledProgram* FbleCompileProgram(FbleLoadedProgram* program);

/**
 * Dissassembles a compiled program.
 *
 * Writes a disassembled version of an instruction block in human readable
 * format to the given file. For debugging purposes.
 *
 * @param fout
 *   The file to write the disassembled program to.
 * @param profile_blocks
 *   Profiling blocks referenced by the code.
 * @param code
 *   The code to disassemble.
 *
 * @sideeffects
 *   A disassembled version of the code is printed to fout.
 */
void FbleDisassemble(FILE* fout, FbleNameV profile_blocks, FbleCode* code);

/**
 * Generates aarch64 for a compiled module.
 *
 * The generated code will export a single function named based on the
 * module path with the following signature:
 *  
 *   void _compiled_(FbleCompiledProgram* program);
 *
 * Calling this function will append this module to the given program if it
 * does not already belong to the given program.
 *
 * @param fout    The output stream to write the C code to.
 * @param module  The module to generate code for.
 *
 * @sideeffects
 * * Generates aarch64 code for the given module.
 */
void FbleGenerateAArch64(FILE* fout, FbleCompiledModule* module);

/**
 * Generates aarch64 to export a compiled module.
 *
 * The generated code will export a single function with the given name with
 * the following signature
 *  
 *   void _name_(FbleExecutableProgram* program);
 *
 * Calling this function add the module and any dependencies to the given
 * executable program.
 *
 * @param fout   The output stream to write the C code to.
 * @param name   The name of the function to generate.
 * @param path   The path to the module to export.
 *
 * @sideeffects
 * * Outputs aarch64 code to the given file.
 */
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path);

/**
 * Generates aarch64 code for main.
 *
 * Generate aarch64 code for a main function that invokes a compiled module
 * with the given wrapper function.
 *
 * The generated code will export a main function of the following form:
 *  
 *   int main(int argc, const char** argv) {
 *     return _main_(argc, argv, _compiled_);
 *   }
 *
 * Where _compiled_ is the FbleCompiledModuleFunction* corresponding to the
 * given module path.
 *
 * @param fout   The output stream to write the C code to.
 * @param main   The name of the wrapper function to invoke.
 * @param path   The path to the module to pass to the wrapper function.
 *
 * @sideeffects
 * * Generates aarch64 code for the given code.
 */
void FbleGenerateAArch64Main(FILE* fout, const char* main, FbleModulePath* path);

/**
 * Generates C code for a compiled module.
 *
 * The generated code will export a single function named based on the
 * module path with the following signature:
 *  
 *   void _compiled_(FbleCompiledProgram* program);
 *
 * Calling this function will append this module to the given program if it
 * does not already belong to the given program.
 *
 * @param fout    The output stream to write the C code to.
 * @param module  The module to generate code for.
 *
 * @sideeffects
 * * Outputs C code to the given file.
 */
void FbleGenerateC(FILE* fout, FbleCompiledModule* module);

/**
 * Generates C code to export a compiled module.
 *
 * The generated code will export a single function with the given name with
 * the following signature
 *  
 *   void _name_(FbleExecutableProgram* program);
 *
 * Calling this function add the module and any dependencies to the given
 * executable program.
 *
 * @param fout  The output stream to write the C code to.
 * @param name  The name of the function to generate.
 * @param path  The path to the module to export.
 *
 * @sideeffects
 * * Outputs C code to the given file.
 */
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path);

/**
 * Generates C code for main.
 *
 * Generate C code for a main function that invokes a compiled module with the
 * given wrapper function.
 *
 * The generated code will export a main function of the following form:
 *  
 *   int main(int argc, const char** argv) {
 *     return _main_(argc, argv, _compiled_);
 *   }
 *
 * Where _compiled_ is the FbleCompiledModuleFunction* corresponding to
 * the given module path.
 *
 * @param fout  The output stream to write the C code to.
 * @param main  The name of the wrapper function to invoke.
 * @param path  The path to the module to pass to the wrapper function.
 *
 * @sideeffects
 * * Outputs C code to the given file.
 */
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path);

#endif // FBLE_COMPILE_H_
