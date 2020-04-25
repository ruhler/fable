// fble.h --
//   This header file describes the externally visible interface to the
//   fble library.

#ifndef FBLE_H_
#define FBLE_H_

#include <stdbool.h>      // for bool
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"
#include "fble-vector.h"
#include "fble-heap.h"
#include "fble-syntax.h"
#include "fble-value.h"
#include "fble-profile.h"


// FbleDecompile --
//   Decompile the given program, writing a disassembled version of the program
//   in human readable format to the given file. For debugging purposes.
//
// Inputs:
//   fout - the file to write the disassembled program to.
//   program - the program to decompile.
//
// Results:
//   True if the program compiled successfully, false otherwise.
//
// Side effects:
//   A disassembled version of the file is printed to fout. In case of error,
//   an error message is printed to stderr.
bool FbleDecompile(FILE* fout, FbleProgram* program);

// FbleEval --
//   Type check and evaluate a program.
//
// Inputs:
//   heap - The heap to use for allocating values.
//   program - The program to evaluate.
//   profile - Output profile for the evaluation.
//
// Results:
//   The value of the evaluated program, or NULL in case of error. The
//   error could be a type error or an undefined union field access.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use.
//   Prints warning messages to stderr.
//   Prints an error message to stderr in case of error.
//   Sets profile to the profile for the evaluation. This must be freed with
//   FbleFreeProfile when no longer in use, regardless of whether evaluation
//   succeeded or failed.
FbleValue* FbleEval(FbleValueHeap* heap, FbleProgram* program, FbleProfile* profile);

// FbleApply --
//   Apply a function to the given arguments.
//
// Inputs:
//   heap - the heap to use for allocating values.
//   func - the function to apply.
//   args - the arguments to apply the function to.
//   profile - the profile to update.
//
// Results:
//   The result of applying the function to the given arguments.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use.
//   Prints warning messages to stderr.
//   Prints an error message to stderr in case of error.
//   Updates the profile with stats from the evaluation.
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValueV args, FbleProfile* profile);

// FbleIO --
//   An interface for reading or writing values over external ports.
typedef struct FbleIO {
  // io --
  //   Read or write values over the external ports. An FbleValue* is supplied
  //   for each port. The behavior of the function depends on the port type and
  //   whether the provided FbleValue* is NULL or non-NULL as follows:
  //
  //   Get Ports:
  //     NULL: the io function may, at its option, read the next input
  //       value and replace NULL with the newly read value.
  //     non-NULL: the io function should do nothing for this port.
  //
  //   Put Ports:
  //     NULL: the io function should do nothing for this port.
  //     non-NULL: the io function may, at its option, output the value.
  //       If the io function chooses to output the value, it should
  //       FbleValueRelease the value and replace it with NULL. Otherwise the
  //       io function should leave the existing value as is.
  //
  //   io may blocking or non-blocking depending on the 'block' input. For
  //   non-blocking io, the io function should return immediately without
  //   blocking for inputs to be ready. For blocking io, the io function
  //   should block until there is an input available on one of the NULL get
  //   ports.
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
  //   
  bool (*io)(struct FbleIO* io, FbleValueHeap* heap, bool block);

  // Vector of port values to read from and write to. The caller must
  // initialize and clear this vector.
  FbleValueV ports;
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
//   profile - the profile to update.
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
