// fble-deps.c --
//   The implementation of the fble-deps program, which generates gcc -MD
//   compatible dependencies for an .fble file.

#include <string.h>   // for strcmp, strlen
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

  FbleProgram* prgm = FbleLoad(arena, path, include_path);
  if (prgm == NULL) {
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  int cols = 1 + strlen(target);
  printf("%s:", target);
  if (prgm->modules.size > 1) {
    for (size_t i = 0; i < prgm->modules.size - 1; ++i) {
      FbleModulePath* mpath = prgm->modules.xs[i].path;
      if (cols > 80) {
        printf(" \\\n");
        cols = 1;
      }

      cols += 1 + strlen(include_path);
      printf(" %s", include_path);
      for (size_t j = 0; j < mpath->path.size; ++j) {
        cols += 1 + strlen(mpath->path.xs[j].name->str);
        printf("/%s", mpath->path.xs[j].name->str);
      }
      cols += 5;
      printf(".fble");
    }
  }
  printf("\n");

  FbleFreeProgram(arena, prgm);
  FbleFreeArena(arena);
  return EX_SUCCESS;
}
