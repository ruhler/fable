// fble-test.c --
//   This file implements the main entry point for the fble-test program.

#include "fble.h"

#include <stdio.h>    // for FILE, fprintf
#include <string.h>   // for strcmp

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
      "Usage: fble-test FILE\n"
      "Parse the fble program from FILE.\n"
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
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }
  
  const char* path = argv[1];

  FbleArena* arena = FbleNewArena(NULL);

  FbleExpr* prgm = FbleParse(arena, path);
  if (prgm == NULL) {
    fprintf(stderr, "failed to parse program\n");
  }

  FbleDeleteArena(arena);
  return (prgm == NULL) ? 1 : 0;
}
