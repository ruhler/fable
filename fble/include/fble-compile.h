// fble-compile.h --
//   Public header file for fble compilation related functions.

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE

#include "fble-profile.h"
#include "fble-syntax.h"
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
FbleCompiledProgram* FbleCompile(FbleArena* arena, FbleProgram* program, FbleProfile* profile);

// FbleLink --
//   Link the modules of a compiled program together into a single FbleValue
//   representing a zero-argument function that can be used to compute the
//   value of the program.
//
// Inputs:
//   heap - heap to use for allocations.
//   program - the compiled program to link.
//
// Results:
//   An FbleValue representing a zero-argument function that can be used to
//   compute the value of the program.
//
// Side effects:
//   Allocates an FbleValue* that should be freed using FbleReleaseValue when
//   no longer needed.
FbleValue* FbleLink(FbleValueHeap* heap, FbleCompiledProgram* program);

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
//   Generate C code for an fble instruction block.
//
// TODO: Compile code to something meaningful and update this documentation
// appropriately.
//
// The generated C code will export a single function named entry with the
// following signature:
//  
//   FbleValue* Entry(FbleValueHeap* heap);
//
// Calling this function will allocate a new value on the heap that is
// the same as the value provided to FbleGenerateC, except with function values
// optimized for better performance where possible.
//
// TODO: Document the flags needed to compile the generated C code, including
// what header files are expected to be available. Document restrictions on
// what names can be used in the generated C code to avoid name conflicts.
//
// Inputs:
//   fout - the output stream to write the C code to.
//   entry - the name of the C function to export.
//   code - the code to compile.
//
// Results:
//   true on success, false on error.
//
// Side effects:
// * Generates C code for the given value.
// * An error message is printed to stderr in case of error.
bool FbleGenerateC(FILE* fout, const char* entry, FbleInstrBlock* code);

#endif // FBLE_COMPILE_H_
