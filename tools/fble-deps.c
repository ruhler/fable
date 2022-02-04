// fble-deps.c --
//   The implementation of the fble-deps program, which generates gcc -MD
//   compatible dependencies for an .fble file.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp, strlen
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"     // for FbleFree.
#include "fble-arg-parse.h" // for FbleParseBoolArg, FbleParseStringArg
#include "fble-load.h"      // for FbleLoad.
#include "fble-vector.h"    // for FbleVectorInit.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);
int main(int argc, const char* argv[]);

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
      "Usage: fble-deps [OPTION...] -t TARGET -m MODULE_PATH\n"
      "\n"
      "Description:\n"
      "  Outputs a depfile suitable for use with ninja build specifying the\n"
      "  .fble files the given module depends on.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n"
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -t, --target TARGET\n"
      "     Specifies the name of the target to use in the generated depfile.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n"
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on failure.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n"
      "  fble-deps -I prgms -t Foo.fble.d -m /Foo% > Foo.fble.d\n"
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
int main(int argc, const char* argv[])
{
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* target = NULL;
  const char* mpath_string = NULL;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseSearchPathArg("-I", &search_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-t", &target, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--target", &target, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-m", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--module", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (help) {
    PrintUsage(stdout);
    FbleFree(search_path.xs);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (target == NULL) {
    fprintf(stderr, "missing required --target option.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (mpath_string == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

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
