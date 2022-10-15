// fble-compile.h --
//   Public header file for fble compilation related functions.

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE

#include "fble-load.h"
#include "fble-module-path.h"
#include "fble-profile.h"

// FbleCode
//   Abstract type representing compiled code.
typedef struct FbleCode FbleCode;

// FbleCompiledModule --
//   Represents a compiled module.
// 
// Fields:
//   path - the path to the module.
//   deps - a list of distinct modules this module depends on.
//   code - code to compute the value of the module, suitable for use in the
//          body of a function takes the computed module values for each
//          module listed in module->deps as arguments to the function.
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleCode* code;
} FbleCompiledModule;

// FbleCompiledModuleV -- A vector of compiled modules.
typedef struct {
  size_t size;
  FbleCompiledModule** xs;
} FbleCompiledModuleV;

// FbleCompiledProgram --
//   A compiled program.
//
// The program is represented as a list of compiled modules in topological
// dependency order. Later modules in the list may depend on earlier modules
// in the list, but not the other way around.
//
// The last module in the list is the main program. The module path for the
// main module is /%.
typedef struct {
  FbleCompiledModuleV modules;
} FbleCompiledProgram;

// FbleFreeCompiledModule --
//   Free resources associated with the given module.
//
// Inputs:
//   module - the module to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeCompiledModule(FbleCompiledModule* module);

// FbleFreeCompiledProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeCompiledProgram(FbleCompiledProgram* program);

// FbleCompileModule --
//   Type check and compile the main module of the given program.
//
// Inputs:
//   program - the program to compile.
//
// Results:
//   The compiled module, or NULL if the program is not well typed.
//
// Side effects:
// * Prints warning messages to stderr.
// * Prints a message to stderr if the program fails to compile.
// * The caller should call FbleFreeCompiledModule to release resources
//   associated with the returned module when it is no longer needed.
FbleCompiledModule* FbleCompileModule(FbleLoadedProgram* program);

// FbleCompileProgram --
//   Type check and compile all modules of the given program.
//
// Inputs:
//   program - the program to compile.
//
// Results:
//   The compiled program, or NULL if the program is not well typed.
//
// Side effects:
// * Prints warning messages to stderr.
// * Prints a message to stderr if the program fails to compile.
// * The caller should call FbleFreeCompiledProgram to release resources
//   associated with the returned program when it is no longer needed.
FbleCompiledProgram* FbleCompileProgram(FbleLoadedProgram* program);

// FbleDisassemble --
//   Write a disassembled version of an instruction block in human readable
//   format to the given file. For debugging purposes.
//
// Inputs:
//   fout - the file to write the disassembled program to.
//   code - the code to disassemble.
//
// Side effects:
//   A disassembled version of the code is printed to fout.
void FbleDisassemble(FILE* fout, FbleCode* code);

// FbleGenerateAArch64 --
//   Generate 64-bit ARM code for an fble compiled module.
//
// The generated code will export a single function named based on the
// module path with the following signature:
//  
//   void <name>(FbleCompiledProgram* program);
//
// Calling this function will append this module to the given program if it
// does not already belong to the given program.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   module - the module to generate code for.
//
// Side effects:
// * Generates arm64 code for the given code.
void FbleGenerateAArch64(FILE* fout, FbleCompiledModule* module);

// FbleGenerateAArch64Export --
//   Generate aarch64 code to export the code for a compiled module.
//
// The generated code will export a single function with the given name with
// the following signature
//  
//   void <name>(FbleExecutableProgram* program);
//
// Calling this function add the module and any dependencies to the given
// executable program.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   name - the name of the function to generate.
//   path - the path to the module to export.
//
// Side effects:
// * Generates aarch64 code for the given code.
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path);

// FbleGenerateAArch64Main --
//   Generate aarch64 code for a main function that invokes a compiled module
//   with the given wrapper function.
//
// The generated code will export a main function of the following form:
//  
// int main(int argc, const char** argv) {
//   return <main>(argc, argv, <compiled module>);
// }
//
// Where <compiled module> is the FbleCompiledModuleFunction* corresponding to
// the given module path.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   main - the name of the wrapper function to invoke.
//   path - the path to the module to pass to the wrapper function.
//
// Side effects:
// * Generates aarch64 code for the given code.
void FbleGenerateAArch64Main(FILE* fout, const char* main, FbleModulePath* path);

// FbleGenerateC --
//   Generate C code for an fble compiled module.
//
// The generated code will export a single function named based on the
// module path with the following signature:
//  
//   void <name>(FbleCompiledProgram* program);
//
// Calling this function will append this module to the given program if it
// does not already belong to the given program.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   module - the module to generate code for.
//
// Side effects:
// * Generates C code for the given code.
void FbleGenerateC(FILE* fout, FbleCompiledModule* module);

// FbleGenerateCExport --
//   Generate C code to export the code for a compiled module.
//
// The generated code will export a single function with the given name with
// the following signature
//  
//   void <name>(FbleExecutableProgram* program);
//
// Calling this function add the module and any dependencies to the given
// executable program.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   name - the name of the function to generate.
//   path - the path to the module to export.
//
// Side effects:
// * Generates C code for the given code.
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path);

// FbleGenerateCMain --
//   Generate C code for a main function that invokes a compiled module
//   with the given wrapper function.
//
// The generated code will export a main function of the following form:
//  
// int main(int argc, const char** argv) {
//   return <main>(argc, argv, <compiled module>);
// }
//
// Where <compiled module> is the FbleCompiledModuleFunction* corresponding to
// the given module path.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   main - the name of the wrapper function to invoke.
//   path - the path to the module to pass to the wrapper function.
//
// Side effects:
// * Generates C code for the given code.
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path);

#endif // FBLE_COMPILE_H_
