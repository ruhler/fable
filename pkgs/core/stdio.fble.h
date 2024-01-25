#ifndef FBLE_CORE_STDIO_FBLE_H_
#define FBLE_CORE_STDIO_FBLE_H_

#include <fble/fble-generate.h>   // for FbleGeneratedModule
#include <fble/fble-profile.h>
#include <fble/fble-value.h>

// FbleNewStdioIO --
//   Allocates a /Core/Stdio/IO%.StdioIO@ instance.
//
// Inputs:
//   heap - The value heap.
//   profile - Profile to store execution results to.
//
// Results:
//   The newly allocated StdioIO@ instance.
//
// Side effects:
// * Adds blocks to the profile.
// * Allocates a value on the heap.
FbleValue* FbleNewStdioIO(FbleValueHeap* heap, FbleProfile* profile);

// FbleStdio -- 
//   Execute a /Core/Stdio/IO%.Run(/Core/Stdio%.Main@) function.
//
// Inputs:
//   heap - The value heap.
//   profile - Profile to store execution results to.
//   stdio - The stdio process to execute. Borrowed.
//   argc - The number of string arguments to the stdio function. Borrowed.
//   argv - The string arguments to the stdio function. Borrowed.
//
// Results:
//   The Bool@ result of executing the stdio process, or NULL in case of
//   error.
//
// Side effects:
// * Updates the profile.
// * Allocates a value on the heap.
// * Performs input and output on stdin, stdout, and stderr according to the
//   execution of the Stdio@ process.
FbleValue* FbleStdio(FbleValueHeap* heap, FbleProfile* profile, FbleValue* stdio, size_t argc, FbleValue** argv);

// FbleStdioMain -- 
//   A main function for running a Stdio@ program with standard command line
//   options.
//
// Inputs:
//   argc - The number of args.
//   argv - The args.
//   module - The compiled module to use as the Stdio@ program to run, or NULL
//            to determine the module based on command line options.
//
// Result:
//   0 for True, 1 for False, 2 for usage error, 3 for other failure.
//
// Side effects:
// * Runs the Stdio@ program, which may interact with stdin, stdout, and
//   stderr.
// * Writes to a profile if specified by the command line options.
int FbleStdioMain(int argc, const char** argv, FbleGeneratedModule* module);

#endif // FBLE_CORE_STDIO_FBLE_H_
