// fble-deps.c --
//   The implementation of the fble-deps program, which generates gcc -MD
//   compatible dependencies for an .fble file.

#include <string.h>   // for strcmp, strlen
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

#define EX_SUCCESS 0
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
      "Usage: fble-deps target FILE [PATH]\n"
      "  target - the name of the target of the output deps.\n"
      "  FILE - the .fble file to get dependencies for.\n"
      "  PATH - an optional include search path.\n"
  );
}

// main --
//   The main entry point for the fble-deps program.
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

  if (argc < 1) {
    fprintf(stderr, "no target specified.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  const char* target = *argv;
  argc--;
  argv++;

  if (argc < 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  
  const char* path = *argv;
  argc--;
  argv++;

  const char* include_path = NULL;
  if (argc > 0) {
    include_path = *argv;
  }

  FbleArena* arena = FbleNewArena();

  FbleProgram* prgm = FbleLoad(arena, path, include_path, NULL);

  size_t cols = 0;
  cols += strlen(target);
  printf("%s:", target);
  for (size_t i = 0; i < prgm->modules.size; ++i) {
    const char* name = prgm->modules.xs[i].name.name->str;
    size_t len = 1 + strlen(name);
    if (cols + len > 78) {
      printf(" \\\n");
      cols = 0;
    }
    cols += len;

    printf(" %s", name);
  }
  printf("\n");

  FbleFreeProgram(arena, prgm);
  FbleFreeArena(arena);
  return EX_SUCCESS;
}
