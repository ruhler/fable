// fble-compile.h --
//   Public header file for fble compilation related functions.

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE

#include "fble-load.h"
#include "fble-module-path.h"
#include "fble-profile.h"
#include "fble-value.h"

// FbleInstrBlock --
//   Abstract type representing compiled code.
typedef struct FbleInstrBlock FbleInstrBlock;

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
  FbleInstrBlock* code;
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
// main module is NULL.
typedef struct {
  FbleCompiledModuleV modules;
} FbleCompiledProgram;

// FbleFreeCompiledProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeCompiledProgram(FbleArena* arena, FbleCompiledProgram* program);

// FbleCompile --
//   Type check and compile the given program.
//
// Inputs:
//   arena - arena used for allocations.
//   program - the program to compile.
//   profile - profile to populate with blocks. May be NULL.
//
// Results:
//   The compiled program, or NULL if the program is not well typed.
//
// Side effects:
// * Prints warning messages to stderr.
// * Prints a message to stderr if the program fails to compile.
// * Adds blocks to the given profile.
// * The caller should call FbleFreeCompiledProgram to release resources
//   associated with the returned program when it is no longer needed.
FbleCompiledProgram* FbleCompile(FbleArena* arena, FbleLoadedProgram* program, FbleProfile* profile);

// FbleDisassemble --
//   Write a disassembled version of an instruction block in human readable
//   format to the given file. For debugging purposes.
//
// Inputs:
//   fout - the file to write the disassembled program to.
//   code - the code to disassemble.
//   profile - profile to use for profile block information.
//
// Side effects:
//   A disassembled version of the code is printed to fout.
void FbleDisassemble(FILE* fout, FbleInstrBlock* code, FbleProfile* profile);

// FbleGenerateC --
//   Generate C code for an fble compiled module.
//
// The generated C code will export a single function named based on the
// module path with the following signature:
//  
//   void <name>(FbleArena* arena, FbleCompiledProgram* program);
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
// Results:
//   true on success, false on error.
//
// Side effects:
// * Generates C code for the given code.
// * An error message is printed to stderr in case of error.
bool FbleGenerateC(FILE* fout, FbleCompiledModule* module);

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

#endif // FBLE_COMPILE_H_
