// fble-native.c --
//   This file implements the main entry point for the fble-native program,
//   which compiles *.fble code to *.c code.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-compile.h"       // for FbleCompile, etc.
#include "fble-module-path.h"   // for FbleParseModulePath

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
      "Usage: fble-native [--export NAME] MODULE_PATH [SEARCH_PATH]\n"
      "Compile an fble module to assembly code.\n"
      "  MODULE_PATH - the fble module path associated with FILE. For example: /Foo/Bar%\n"
      "  SEARCH_PATH - the directory to search for .fble files in.\n"
      "Options:\n"
      "  --export NAME\n"
      "    Generates a function with the given NAME to export the module\n"
      "At least one of [--export NAME] or [SEARCH_PATH] must be provided.\n"
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

  const char* export = NULL;
  if (argc > 1 && strcmp("--export", argv[0]) == 0) {
    export = argv[1];
    argc -= 2;
    argv += 2;
  }

  if (argc < 1) {
    fprintf(stderr, "no path.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  const char* mpath_string = *argv;
  argc--;
  argv++;

  const char* search_path = argc < 1 ? NULL : argv[0];
  if (export == NULL && search_path == NULL) {
    fprintf(stderr, "one of --export NAME or SEARCH_PATH must be specified.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    return EX_FAIL;
  }

  if (export != NULL) {
    FbleGenerateAArch64Export(stdout, export, mpath);
  }

  if (search_path != NULL) {
    FbleLoadedProgram* prgm = FbleLoad(search_path, mpath);
    if (prgm == NULL) {
      FbleFreeModulePath(mpath);
      return EX_FAIL;
    }

    FbleCompiledModule* module = FbleCompileModule(prgm);
    FbleFreeLoadedProgram(prgm);

    if (module == NULL) {
      FbleFreeModulePath(mpath);
      return EX_FAIL;
    }

    FbleFreeModulePath(module->path);
    module->path = FbleCopyModulePath(mpath);

    FbleGenerateAArch64(stdout, module);

    FbleFreeCompiledModule(module);
  }

  FbleFreeModulePath(mpath);
  return EX_SUCCESS;
}
