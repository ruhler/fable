// fble-compile.h --
//   Header file for fble compiled programs.

#ifndef FBLE_COMPILE_H_
#define FBLE_COMPILE_H_

#include <stdio.h>         // for FILE

#include "fble-alloc.h"    // for FbleArena
#include "fble-profile.h"  // for FbleProfile
#include "fble-syntax.h"   // for FbleProgram

// CompiledProgram --
//   Abstract type representing a compiled fble program.
typedef struct FbleCompiledProgram FbleCompiledProgram;

// FbleCompile --
//   Type check and compile the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//   prgm - the program to compile.
//   profile - profile to populate with blocks. May be NULL.
//
// Results:
//   The compiled program, or NULL if the program is not well typed.
//
// Side effects:
//   Prints warning messages to stderr.
//   Prints a message to stderr if the program fails to compile. Allocates
//   memory for the instructions which must be freed with FbleFreeInstrBlock when
//   it is no longer needed.
//   Adds blocks to the given profile.
//   The caller should call FbleFreeCompiledProgram to release resources
//   associated with the returned program when it is no longer needed.
FbleCompiledProgram* FbleCompile(FbleArena* arena, FbleProgram* program, FbleProfile* profile);

// FbleFreeCompiledProgram --
//   Free resources associated with a compiled program.
//
// Inputs:
//   arena - the arena to use for allocations
//   program - the program to free.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeCompiledProgram(FbleArena* arena, FbleCompiledProgram* program);

// FbleDisassemble --
//   Writing a disassembled version of a compiled program in human readable
//   format to the given file. For debugging purposes.
//
// Inputs:
//   fout - the file to write the disassembled program to.
//   program - the program to decompile.
//   profile - profile to use for profile block information.
//
// Results:
//   True if the program compiled successfully, false otherwise.
//
// Side effects:
//   A disassembled version of the file is printed to fout.
void FbleDisassemble(FILE* fout, FbleCompiledProgram* program, FbleProfile* profile);

#endif // FBLE_COMPILE_H_
