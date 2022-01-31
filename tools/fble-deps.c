// fble-deps.c --
//   The implementation of the fble-deps program, which generates gcc -MD
//   compatible dependencies for an .fble file.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp, strlen
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"   // for FbleFree.
#include "fble-load.h"    // for FbleLoad.
#include "fble-vector.h"  // for FbleVectorInit.

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
  fprintf(stream, "%s",
      "Usage: fble-deps target SEARCH_PATH MODULE_PATH\n"
      "  target - the name of the target of the output deps.\n"
      "  MODULE_PATH - the fble module path to the module to get dependencies for.\n"
      "  SEARCH_PATH - a directory with the .fble files.\n"
      "Example: fble-deps out/prgms/Foo/Bar.fble.d prgms /Foo/Bar%.\n"
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
    fprintf(stderr, "no SEARCH_PATH specified.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  FbleVectorAppend(search_path, *argv);
  argc--;
  argv++;

  if (argc < 1) {
    fprintf(stderr, "no MODULE_PATH specified.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }
  const char* mpath_string = *argv;
  argc--;
  argv++;

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFree(search_path.xs);
    return EX_FAIL;
  }

  FbleStringV deps;
  FbleVectorInit(deps);
  FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, &deps);
  FbleFree(search_path.xs);
  FbleFreeModulePath(mpath);
  FbleFreeLoadedProgram(prgm);

  if (prgm != NULL) {
    FbleSaveBuildDeps(stdout, target, deps);
  }

  for (size_t i = 0; i < deps.size; ++i) {
    FbleFreeString(deps.xs[i]);
  }
  FbleFree(deps.xs);

  return prgm == NULL ? EX_FAIL : EX_SUCCESS;
}
