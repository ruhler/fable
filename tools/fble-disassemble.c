// fble-disassemble.c --
//   This file implements the main entry point for the fble-disassemble program.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);

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
      "Usage: fble-disassemble FILE [PATH]\n"
      "Disassemble the fble program from FILE.\n"
      "PATH is an optional include search path.\n"
      "Exit status is 0 if the program compiled successfully, 1 otherwise.\n"
  );
}

// main --
//   The main entry point for the fble-disassemble program.
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
  FbleLoadedProgram* prgm = FbleLoad(arena, path, include_path);
  if (prgm == NULL) {
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  FbleProfile* profile = FbleNewProfile(arena);
  FbleCompiledProgram* compiled = FbleCompile(arena, prgm, profile);
  FbleFreeLoadedProgram(arena, prgm);

  if (compiled == NULL) {
    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  FbleDisassemble(stdout, compiled->modules.xs[compiled->modules.size - 1].code, profile);

  FbleFreeCompiledProgram(arena, compiled);
  FbleFreeProfile(arena, profile);
  FbleFreeArena(arena);
  return EX_SUCCESS;
}
