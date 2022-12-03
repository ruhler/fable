// fble-deps.c --
//   The implementation of the fble-deps program, which generates gcc -MD
//   compatible dependencies for an .fble file.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp, strlen
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>     // for FbleFree.
#include <fble/fble-arg-parse.h> // for FbleParseBoolArg, FbleParseStringArg
#include <fble/fble-load.h>      // for FbleLoad.
#include <fble/fble-vector.h>    // for FbleVectorInit.
#include <fble/fble-version.h>   // for FBLE_VERSION

#include "fble-deps.usage.h"     // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintVersion(FILE* stream);
static void PrintHelp(FILE* stream);
int main(int argc, const char* argv[]);

// PrintVersion --
//   Prints version info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the version information to.
//
// Side effects:
//   Outputs version information to the given stream.
static void PrintVersion(FILE* stream)
{
  fprintf(stream, "fble-deps %s\n", FBLE_VERSION);
}

// PrintHelp --
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
static void PrintHelp(FILE* stream)
{
  fprintf(stream, "%s", fbldUsageHelpText);
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
  bool version = false;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!(help || version || error) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (FbleParseSearchPathArg(&search_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-t", &target, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--target", &target, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-m", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--module", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    PrintVersion(stdout);
    FbleVectorFree(search_path);
    return EX_SUCCESS;
  }

  if (help) {
    PrintHelp(stdout);
    FbleVectorFree(search_path);
    return EX_SUCCESS;
  }

  if (error) {
    PrintHelp(stderr);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  if (target == NULL) {
    fprintf(stderr, "missing required --target option.\n");
    PrintHelp(stderr);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  if (mpath_string == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintHelp(stderr);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleVectorFree(search_path);
    return EX_FAIL;
  }

  FbleStringV deps;
  FbleVectorInit(deps);
  FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, &deps);
  FbleVectorFree(search_path);
  FbleFreeModulePath(mpath);
  FbleFreeLoadedProgram(prgm);

  FbleSaveBuildDeps(stdout, target, deps);

  for (size_t i = 0; i < deps.size; ++i) {
    FbleFreeString(deps.xs[i]);
  }
  FbleVectorFree(deps);
  return EX_SUCCESS;
}
