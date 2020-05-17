// fble.h --
//   This header file describes the externally visible interface to the
//   fble library.

#ifndef FBLE_H_
#define FBLE_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"
#include "fble-profile.h"
#include "fble-value.h"
#include "fble-vector.h"

// TODO: Move fble-syntax.h to internal.
#include "fble-syntax.h"


// FbleCompiledProgram --
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

// FbleEval --
//   Evaluate a compiled program.
//
// Inputs:
//   heap - The heap to use for allocating values.
//   program - The program to evaluate.
//   profile - the profile to update. May be NULL to disable profiling.
//
// Results:
//   The value of the evaluated program, or NULL in case of a runtime error in
//   the program.
//
// Side effects:
//   The returned value must be freed with FbleReleaseValue when no longer in
//   use.
//   Prints an error message to stderr in case of a runtime error.
//   Updates profiling information in profile based on the execution of the
//   program.
FbleValue* FbleEval(FbleValueHeap* heap, FbleCompiledProgram* program, FbleProfile* profile);

// FbleApply --
//   Apply a function to the given arguments.
//
// Inputs:
//   heap - the heap to use for allocating values.
//   func - the function to apply.
//   args - the arguments to apply the function to. length == func->argc.
//   profile - the profile to update. May be NULL to disable profiling.
//
// Results:
//   The result of applying the function to the given arguments.
//
// Side effects:
//   The returned value must be freed with FbleReleaseValue when no longer in
//   use.
//   Does not take ownership of the func. Does not take ownership of the args.
//   Prints warning messages to stderr.
//   Prints an error message to stderr in case of error.
//   Updates the profile with stats from the evaluation.
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);

// FbleIO --
//   An interface for reading or writing values over external ports.
typedef struct FbleIO {
  // io --
  //   Read or write values over the external ports. The caller will already
  //   have provided FbleValue** for the ports. The behavior of the function
  //   depends on the port type and whether the respective FbleValue* is NULL
  //   or non-NULL as follows:
  //
  //   Input Ports:
  //     NULL: the io function may, at its option, read the next input
  //       value and replace NULL with the newly read value.
  //     non-NULL: the io function should do nothing for this port.
  //
  //   Output Ports:
  //     NULL: the io function should do nothing for this port.
  //     non-NULL: the io function may, at its option, output the value.
  //       If the io function chooses to output the value, it should
  //       FbleReleaseValue the value and replace it with NULL. Otherwise the
  //       io function should leave the existing value as is.
  //
  //   io may be blocking or non-blocking depending on the 'block' argument.
  //   For non-blocking io, the io function should return immediately without
  //   blocking for inputs to be ready. For blocking io, the io function
  //   should block until there is an input available for one of the NULL
  //   input ports.
  //
  //   The function should return true if any of the ports have been read or
  //   written, false otherwise.
  //
  // Inputs:
  //   io - The FbleIO associated with this io.
  //   heap - The heap to use for allocating values.
  //   block - true if io should be blocking, false if it should be
  //           non-blocking.
  //
  // Result:
  //   true if one or more ports have been read or written, false otherwise.
  //
  // Side effects:
  //   Reads or writes values to external ports depending on the provided
  //   arguments and may block.
  bool (*io)(struct FbleIO* io, FbleValueHeap* heap, bool block);
} FbleIO;

// FbleNoIO --
//   An IO function that does no IO.
//
// See documentation of FbleIO.io.
bool FbleNoIO(FbleIO* io, FbleValueHeap* heap, bool block);

// FbleExec --
//   Execute a process.
//
// Inputs:
//   heap - The heap to use for allocating values.
//   io - The io to use for external ports.
//   proc - The process to execute.
//   profile - the profile to update. May be NULL to disable profiling.
//
// Results:
//   The result of executing the process, or NULL in case of error. The
//   error could be an undefined union field access.
//
// Side effects:
//   Prints warning messages to stderr.
//   Prints an error message to stderr in case of error.
//   Updates the profile with stats from the evaluation.
FbleValue* FbleExec(FbleValueHeap* heap, FbleIO* io, FbleValue* proc, FbleProfile* profile);

#endif // FBLE_H_
