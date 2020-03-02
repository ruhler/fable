// fble.h --
//   This header file describes the externally visible interface to the
//   fble library.

#ifndef FBLE_H_
#define FBLE_H_

#include <stdbool.h>      // for bool
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"
#include "fble-vector.h"
#include "fble-syntax.h"
#include "fble-value.h"
#include "fble-profile.h"


// FbleEval --
//   Type check and evaluate a program.
//
// Inputs:
//   arena - The arena to use for allocating values.
//   program - The program to evaluate.
//   blocks - Output info about blocks in the program.
//   profile - Output profile for the evaluation.
//   dump_compiled - If true, dump the compiled program to stderr for
//                   debugging.
//
// Results:
//   The value of the evaluated program, or NULL in case of error. The
//   error could be a type error or an undefined union field access.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints an error message to stderr in case of error.
//   Sets blocks to the info about all blocks in the program. This must be
//   freed when no longer in use.
//   Sets profile to the profile for the evaluation. This must be freed with
//   FbleFreeProfile when no longer in use.
//   If dump_compiled is true, dumps the compiled program to stderr for
//   debugging purposes.
FbleValue* FbleEval(FbleValueArena* arena, FbleProgram* program, FbleNameV* blocks, FbleProfile** profile, bool dump_compiled);

// FbleApply --
//   Apply a function to the given argument.
//
// Inputs:
//   arena - the arena to use for allocating values.
//   func - the function to apply.
//   arg - the argument to apply the function to.
//   profile - the profile to update.
//
// Results:
//   The result of applying the function to the given argument.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints an error message to stderr in case of error.
//   Updates the profile with stats from the evaluation.
FbleValue* FbleApply(FbleValueArena* arena, FbleValue* func, FbleValue* arg, FbleProfile* profile);

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
  //   arena - The arena to use for allocating and freeing values.
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
  bool (*io)(struct FbleIO* io, FbleValueArena* arena, bool block);

  // Vector of port values to read from and write to. The caller must
  // initialize and clear this vector.
  FbleValueV ports;
} FbleIO;

// FbleExec --
//   Execute a process.
//
// Inputs:
//   arena - The arena to use for allocating values.
//   io - The io to use for external ports.
//   proc - The process to execute.
//   profile - the profile to update.
//
// Results:
//   The result of executing the process, or NULL in case of error. The
//   error could be an undefined union field access.
//
// Side effects:
//   Prints an error message to stderr in case of error.
//   Updates the profile with stats from the evaluation.
FbleValue* FbleExec(FbleValueArena* arena, FbleIO* io, FbleValue* proc, FbleProfile* profile);

#endif // FBLE_H_
