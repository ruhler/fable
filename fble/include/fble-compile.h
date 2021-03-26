// fble-compile.h --
//   Public header file for fble compilation related functions.

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE

#include "fble-profile.h"
#include "fble-syntax.h"
#include "fble-value.h"

// FbleCompile --
//   Type check and compile the given program.
//
// Inputs:
//   heap - heap used for allocations.
//   program - the program to compile.
//   profile - profile to populate with blocks. May be NULL.
//
// Results:
//   A zero-argument function representing the compiled program. To run the
//   program, apply the function. Returns NULL if the program is not well
//   typed.
//
// Side effects:
// * Prints warning messages to stderr.
// * Prints a message to stderr if the program fails to compile.
// * Adds blocks to the given profile.
// * The caller should call FbleReleaseValue to release resources
//   associated with the returned program when it is no longer needed.
FbleValue* FbleCompile(FbleValueHeap* heap, FbleProgram* program, FbleProfile* profile);

// FbleDisassemble --
//   Write a disassembled version of a compiled program in human readable
//   format to the given file. For debugging purposes.
//
// Inputs:
//   fout - the file to write the disassembled program to.
//   program - the program to decompile.
//   profile - profile to use for profile block information.
//
// Side effects:
//   A disassembled version of the file is printed to fout.
void FbleDisassemble(FILE* fout, FbleValue* program, FbleProfile* profile);

// FbleGenerateC --
//   Generate C code for an fble value.
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
//   value - the value to compile.
//
// Results:
//   true on success, false on error.
//
// Side effects:
// * Generates C code for the given value.
// * An error message is printed to stderr in case of error.
bool FbleGenerateC(FILE* fout, const char* entry, FbleValue* value);

#endif // FBLE_COMPILE_H_
