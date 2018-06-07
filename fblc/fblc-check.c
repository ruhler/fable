// fblc-check.c --
//   The file implements the main entry point for the fblc-check command.

#include <stdio.h>      // for FILE
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for strcmp

#include "fblcs.h"

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* fout);
int main(int argc, char* argv[]);

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Result:
//   None.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream)
{
  fprintf(stream,
      "Usage: fblc-check [--error] FILE\n"
      "Check whether FILE is a well formed text fblc program.\n"
      "Exit status is 0 if the program is well formed, 1 otherwise.\n"
      "With --error, exit status is 0 if the program is not well formed, 0 otherwise.\n"
  );
}

// main --
//   The main entry point for fblc-check.
//   Checks whether FILE is a well formed text fblc program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   Exit status is 0 if the program is well formed, 1 otherwise.
//   With --error, exit status is 0 if the program is not well formed, 0 otherwise.
//
// Side effects:
//   Prints an error message to standard error if the program is not well formed.
//   With --error, prints the error message to standard out instead.
//
//   This function leaks memory under the assumption it is called as the main
//   entry point of the program and the OS will free the memory resources used
//   immediately after the function returns.
int main(int argc, char* argv[])
{
  argc--;
  argv++;
  if (argc > 0 && strcmp("--help", *argv) == 0) {
    PrintUsage(stdout);
    return EX_SUCCESS;
  }

  bool expect_error = false;
  if (argc > 0 && strcmp("--error", *argv) == 0) {
    expect_error = true;
    argc--;
    argv++;
  }

  if (argc < 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  
  if (argc > 1) {
    fprintf(stderr, "too many input files.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }

  const char* filename = argv[0];

  FILE* stderr_save = stderr;
  stderr = expect_error ? stdout : stderr;

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcsProgram* prog = FblcsParseProgram(&FblcMallocArena, filename);
  if (prog == NULL || !FblcsCheckProgram(prog)) {
    return expect_error ? EX_SUCCESS : EX_FAIL;
  }

  if (expect_error) {
    fprintf(stderr_save, "expected error, but none encountered.\n");
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
