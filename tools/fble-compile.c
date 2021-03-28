// fble-native.c --
//   This file implements the main entry point for the fble-native program,
//   which compiles *.fble code to *.c code.

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
  fprintf(stream, "%s",
      "Usage: fble-native path FILE [PATH]\n"
      "Compile the fble program to C code.\n"
      "  path - the fble module path associated with FILE. For example: /Foo/Bar%\n"
      "  FILE - the name of the .fble file to compile.\n"
      "  PATH - an optional include search path.\n"
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
    fprintf(stderr, "no path.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  const char* mpath_string = *argv;
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

  FbleModulePath* mpath = FbleParseModulePath(arena, mpath_string);
  if (mpath == NULL) {
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  FbleProgram* prgm = FbleLoad(arena, path, include_path);
  if (prgm == NULL) {
    FbleFreeModulePath(arena, mpath);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  FbleCompiledProgram* compiled = FbleCompile(arena, prgm, NULL);
  FbleFreeProgram(arena, prgm);

  if (compiled == NULL) {
    FbleFreeModulePath(arena, mpath);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  FbleCompiledModule* module = compiled->modules.xs + compiled->modules.size - 1;
  FbleFreeModulePath(arena, module->path);
  module->path = mpath;

  FbleGenerateC(stdout, module);

  FbleFreeCompiledProgram(arena, compiled);
  FbleFreeArena(arena);
  return EX_SUCCESS;
}
