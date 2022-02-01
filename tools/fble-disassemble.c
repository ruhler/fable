// fble-disassemble.c --
//   This file implements the main entry point for the fble-disassemble program.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"     // for FbleFree.
#include "fble-compile.h"   // for FbleCompile, FbleDisassemble.
#include "fble-load.h"      // for FbleLoad.
#include "fble-profile.h"   // for FbleNewProfile, etc.
#include "fble-vector.h"    // for FbleVectorInit.

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
      "Usage: fble-disassemble [-I DIR ...] MODULE_PATH\n"
      "Disassemble an fble program.\n"
      "  -I DIR - adds DIR to the module search path.\n"
      "  MODULE_PATH - the fble module path associated with FILE. For example: /Foo/Bar%\n"
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

  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  while (argc > 1 && strcmp(argv[0], "-I") == 0) {
    FbleVectorAppend(search_path, argv[1]);
    argc -= 2;
    argv += 2;
  }

  if (argc < 1) {
    fprintf(stderr, "no MODULE_PATH provided.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  } else if (argc > 1) {
    fprintf(stderr, "too many arguments.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }
  
  const char* mpath_string = argv[0];

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFree(search_path.xs);
    return EX_FAIL;
  }

  FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, NULL);
  FbleFree(search_path.xs);
  FbleFreeModulePath(mpath);
  if (prgm == NULL) {
    return EX_FAIL;
  }

  FbleCompiledModule* compiled = FbleCompileModule(prgm);
  FbleFreeLoadedProgram(prgm);

  if (compiled == NULL) {
    return EX_FAIL;
  }

  FbleDisassemble(stdout, compiled->code);

  FbleFreeCompiledModule(compiled);
  return EX_SUCCESS;
}
