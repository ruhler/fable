// fbld-check.c --
//   This file implements the main entry point for the fbld-check program.

#include <stdio.h>        // for FILE, fprintf, stdout, stderr
#include <string.h>       // for strcmp

#include "fblc.h"
#include "fbld.h"

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);
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
      "Usage: fbld-check [--error] PATH MAIN\n"
      "Check whether the module MAIN is a well formed fbld program.\n"
      "Exit status is 0 if the program is well formed, 1 otherwise.\n"
      "With --error, exit status is 0 if the program is not well formed, 0 otherwise.\n"
  );
}

// main --
//   The main entry point for the fbld-check program.
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
    fprintf(stderr, "no program path.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  const char* path = *argv;
  argv++;
  argc--;
  
  if (argc < 1) {
    fprintf(stderr, "no main module.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  const char* main_module = *argv;
  argv++;
  argc--;

  if (argc > 0) {
    fprintf(stderr, "too many arguments.\n");
    return EX_USAGE;
  }

  FILE* old_stderr = stderr;
  stderr = expect_error ? stdout : stderr;

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena* arena = &FblcMallocArena;

  FbldStringV search_path;
  FblcVectorInit(arena, search_path);
  FblcVectorAppend(arena, search_path, path);

  FbldProgram* prgm = FBLC_ALLOC(arena, FbldProgram);
  FblcVectorInit(arena, prgm->interfv);
  FblcVectorInit(arena, prgm->mdeclv);
  FblcVectorInit(arena, prgm->mdefnv);

  if (FbldLoadMDefns(arena, &search_path, main_module, prgm)) {
    if (expect_error) {
      fprintf(old_stderr, "expected error, but no error encountered.\n");
      return EX_FAIL;
    }
    return EX_SUCCESS;
  }
  return expect_error ? EX_SUCCESS : EX_FAIL;
}
