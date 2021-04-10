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
      "Usage: fble-native path [--export NAME] [FILE [PATH]]\n"
      "Compile an fble module to C code.\n"
      "  path - the fble module path associated with FILE. For example: /Foo/Bar%\n"
      "  FILE - the name of the .fble file to compile.\n"
      "  PATH - an optional include search path.\n"
      "Options:\n"
      "  --export NAME\n"
      "    Generates a C function with the given NAME to export the module\n"
      "At least one of [--export NAME] or [FILE [PATH]] must be provided.\n"
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

  const char* export = NULL;
  if (argc > 1 && strcmp("--export", argv[0]) == 0) {
    export = argv[1];
    argc -= 2;
    argv += 2;
  }

  const char* path = NULL;
  if (export == NULL && argc < 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }

  if (argc > 0) {
    path = *argv;
    argc--;
    argv++;
  }

  const char* include_path = NULL;
  if (argc > 0) {
    include_path = *argv;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    return EX_FAIL;
  }

  if (export != NULL) {
    FbleGenerateCExport(stdout, export, mpath);
  }

  if (path != NULL) {
    FbleLoadedProgram* prgm = FbleLoad(path, include_path);
    if (prgm == NULL) {
      FbleFreeModulePath(mpath);
      return EX_FAIL;
    }

    FbleCompiledProgram* compiled = FbleCompile(prgm, NULL);
    FbleFreeLoadedProgram(prgm);

    if (compiled == NULL) {
      FbleFreeModulePath(mpath);
      return EX_FAIL;
    }

    FbleCompiledModule* module = compiled->modules.xs + compiled->modules.size - 1;
    FbleFreeModulePath(module->path);
    module->path = FbleCopyModulePath(mpath);

    FbleGenerateC(stdout, module);

    FbleFreeCompiledProgram(compiled);
  }

  FbleFreeModulePath(mpath);
  return EX_SUCCESS;
}
