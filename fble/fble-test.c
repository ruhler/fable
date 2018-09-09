// fble-test.c --
//   This file implements the main entry point for the fble-test program.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

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
      "Usage: fble-test [--error] FILE\n"
      "Type check and evaluate the fble program from FILE.\n"
      "If the result is a process, run the process.\n"
      "Exit status is 0 if the program produced no type or runtime errors, 1 otherwise.\n"
      "With --error, exit status is 0 if the program produced a type or runtime error, 0 otherwise.\n"
  );
}

// main --
//   The main entry point for the fble-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints an error to stderr and exits the program in the case of error.
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
  
  const char* path = *argv;

  FILE* stderr_save = stderr;
  stderr = expect_error ? stdout : stderr;

  FbleArena* arena = FbleNewArena(NULL);

  FbleExpr* prgm = FbleParse(arena, path);
  FbleValue* result = NULL;
  if (prgm != NULL) {
    FbleArena* eval_arena = FbleNewArena(arena);
    result = FbleEval(eval_arena, prgm);

    // TODO: If the result is a process, run the process.

    FbleDropStrongRef(eval_arena, result);
    FbleAssertEmptyArena(eval_arena);
    FbleDeleteArena(eval_arena);
  }

  FbleDeleteArena(arena);

  if (result == NULL) {
    return expect_error ? EX_SUCCESS : EX_FAIL;
  }

  if (expect_error) {
    fprintf(stderr_save, "expected error, but none encountered.\n");
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
