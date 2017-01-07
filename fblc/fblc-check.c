// fblc-check.c
//
//   The file implements the main entry point for the fblc-check command.

#include <stdio.h>      // for FILE
#include <string.h>     // for strcmp

#include "fblct.h"
#include "gc.h"

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* fout);

// PrintUsage --
//   
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
      "Usage: fblc-check [--error] FILE \n"
      "Check whether FILE is a well formed text fblc program.\n"
      "Exit status is 0 if the program is well formed, 1 otherwise.\n"
      "With --error, exit status is 0 if the program is not well formed, 0 otherwise.\n"
  );
}

// main --
//
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

  stderr = expect_error ? stdout : stderr;
  int exit_success = expect_error ? EX_FAIL : EX_SUCCESS;
  int exit_fail = expect_error ? EX_SUCCESS : EX_FAIL;

  GcInit();
  FblcArena* gc_arena = CreateGcArena();
  FblcArena* bulk_arena = CreateBulkFreeArena(gc_arena);
  SProgram* sprog = ParseProgram(bulk_arena, filename);
  bool error = sprog == NULL || !CheckProgram(sprog);
  FreeBulkFreeArena(bulk_arena);
  FreeGcArena(gc_arena);
  GcFinish();
  return error ? exit_fail : exit_success;
}
