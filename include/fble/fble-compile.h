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
 * @func[FbleFreeCompiledModule] Frees an FbleCompiledModule.
 *
 *  @arg[FbleCopmiledModule*][module] The module to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeCompiledModule(FbleCompiledModule* module);

/**
 * @func[FbleFreeCompiledProgram] Frees an FbleCompiledProgram.
 *
 *  @arg[FbleCompiledProgram*][program] The program to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeCompiledProgram(FbleCompiledProgram* program);

/**
 * @func[FbleCompileModule] Compiles a module.
 *
 *  @arg[FbleLoadedProgram*][program] The program to compile.
 *
 *  @returns FbleCompiledModule*
 *   The compiled module, or NULL if the program is not well typed.
 *
 *  @sideeffects
 *   @i Prints warning messages to stderr.
 *   @i Prints a message to stderr if the program fails to compile.
 *   @i The caller should call FbleFreeCompiledModule to release resources
 *    associated with the returned module when it is no longer needed.
 */
FbleCompiledModule* FbleCompileModule(FbleLoadedProgram* program);

/**
 * @func[FbleCompileProgram] Compiles a program.
 *
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
 *   @i The caller should call FbleFreeCompiledProgram to release resources
 *    associated with the returned program when it is no longer needed.
 */
FbleCompiledProgram* FbleCompileProgram(FbleLoadedProgram* program);

/**
 * @func[FbleDisassemble] Dissassembles a compiled module.
 *
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

/**
 * @func[FbleGenerateAArch64] Generates aarch64 for a compiled module.
 *
 *  The generated code will export a single function named based on the module
 *  path with the following signature:
 *  
 *  @code[c]
 *   void _compiled_(FbleCompiledProgram* program);
 *
 *  Calling this function will append this module to the given program if it
 *  does not already belong to the given program.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[FbleCompiledModule*] module
 *   The module to generate code for.
 *
 *  @sideeffects
 *   @i Generates aarch64 code for the given module.
 */
void FbleGenerateAArch64(FILE* fout, FbleCompiledModule* module);

/**
 * @func[FbleGenerateAArch64Export] Generates aarch64 to export a compiled module.
 *
 *  The generated code will export a single function with the given name with
 *  the following signature
 *  
 *  @code[c]
 *   void _name_(FbleExecutableProgram* program);
 *
 *  Calling this function add the module and any dependencies to the given
 *  executable program.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] name
 *   The name of the function to generate.
 *  @arg[FbleMOdulePath*] path
 *   The path to the module to export.
 *
 *  @sideeffects
 *   @i Outputs aarch64 code to the given file.
 */
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path);

/**
 * @func[FbleGenerateAArch64Main] Generates aarch64 code for main.
 *
 *  Generate aarch64 code for a main function that invokes a compiled module
 *  with the given wrapper function.
 *
 *  The generated code will export a main function of the following form:
 *
 *  @code[c]
 *   int main(int argc, const char** argv) {
 *     return _main_(argc, argv, _compiled_);
 *   }
 *
 *  Where _compiled_ is the FbleCompiledModuleFunction* corresponding to the
 *  given module path.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] main
 *   The name of the wrapper function to invoke.
 *  @arg[FbleModulePath*] path
 *   The path to the module to pass to the wrapper function.
 *
 *  @sideeffects
 *   @i Generates aarch64 code for the given code.
 */
void FbleGenerateAArch64Main(FILE* fout, const char* main, FbleModulePath* path);

/**
 * @func[FbleGenerateC] Generates C code for a compiled module.
 *
 *  The generated code will export a single function named based on the module
 *  path with the following signature:
 *  
 *  @code[c]
 *   void _compiled_(FbleCompiledProgram* program);
 *
 *  Calling this function will append this module to the given program if it
 *  does not already belong to the given program.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[FbleCompiledModule*] module
 *   The module to generate code for.
 *
 *  @sideeffects
 *   @i Outputs C code to the given file.
 */
void FbleGenerateC(FILE* fout, FbleCompiledModule* module);

/**
 * @func[FbleGenerateCExport] Generates C code to export a compiled module.
 *
 *  The generated code will export a single function with the given name with
 *  the following signature
 *  
 *  @code[c]
 *   void _name_(FbleExecutableProgram* program);
 *
 *  Calling this function add the module and any dependencies to the given
 *  executable program.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] name
 *   The name of the function to generate.
 *  @arg[FbleModulePath*] path
 *   The path to the module to export.
 *  
 *  @sideeffects
 *   @i Outputs C code to the given file.
 */
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path);

/**
 * @func[FbleGenerateCMain] Generates C code for main.
 *
 *  Generate C code for a main function that invokes a compiled module with
 *  the given wrapper function.
 *
 *  The generated code will export a main function of the following form:
 *  
 *  @code[c]
 *   int main(int argc, const char** argv) {
 *     return _main_(argc, argv, _compiled_);
 *   }
 *
 *  Where _compiled_ is the FbleCompiledModuleFunction* corresponding to the
 *  given module path.
 *
 *  @arg[FILE*] fout
 *   The output stream to write the C code to.
 *  @arg[const char*] main
 *   The name of the wrapper function to invoke.
 *  @arg[FbleModulePath*] path
 *   The path to the module to pass to the wrapper function.
 *  
 *  @sideeffects
 *   @i Outputs C code to the given file.
 */
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path);

#endif // FBLE_COMPILE_H_
