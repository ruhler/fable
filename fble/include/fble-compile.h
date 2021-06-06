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
  FbleCompiledModule* xs;
} FbleCompiledModuleV;

// FbleCompiledProgram --
//   A compiled program.
//
// The program is represented as a list of compiled modules in topological
// dependancy order. Later modules in the list may depend on earlier modules
// in the list, but not the other way around.
//
// The last module in the list is the main program. The module path for the
// main module is /%.
typedef struct {
  FbleCompiledModuleV modules;
} FbleCompiledProgram;

// FbleFreeCompiledProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeCompiledProgram(FbleCompiledProgram* program);

// FbleCompile --
//   Type check and compile the given program.
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
FbleCompiledProgram* FbleCompile(FbleLoadedProgram* program);

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

// FbleGenerateC --
//   Generate C code for an fble compiled module.
//
// The generated C code will export a single function named based on the
// module path with the following signature:
//  
//   void <name>(FbleCompiledProgram* program);
//
// Calling this function will append this module to the given program if it
// does not already belong to the given program.
//
// TODO: Document the flags needed to compile the generated C code, including
// what header files are expected to be available. Document restrictions on
// what names can be used in the generated C code to avoid name conflicts.
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
// The generated C code will export a single function with the given name with
// the following signature
//  
//   FbleValue* <name>(FbleValueHeap* heap);
//
// Calling this function will allocate an FbleValue representing a zero
// argument function that can be executed to compute the value of the given
// module.
//
// TODO: Document the flags needed to compile the generated C code, including
// what header files are expected to be available. Document restrictions on
// what names can be used in the generated C code to avoid name conflicts.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   name - the name of the function to generate.
//   path - the path to the module to export.
//
// Side effects:
// * Generates C code for the given code.
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path);

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
//   FbleValue* <name>(FbleValueHeap* heap);
//
// Calling this function will allocate an FbleValue representing a zero
// argument function that can be executed to compute the value of the given
// module.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   name - the name of the function to generate.
//   path - the path to the module to export.
//
// Side effects:
// * Generates aarch64 code for the given code.
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path);

#endif // FBLE_COMPILE_H_
