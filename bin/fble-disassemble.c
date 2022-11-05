// fble-disassemble.c --
//   This file implements the main entry point for the fble-disassemble program.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>     // for FbleFree.
#include <fble/fble-arg-parse.h> // for FbleParseBoolArg, etc.
#include <fble/fble-compile.h>   // for FbleCompile, FbleDisassemble.
#include <fble/fble-load.h>      // for FbleLoad.
#include <fble/fble-profile.h>   // for FbleNewProfile, etc.
#include <fble/fble-vector.h>    // for FbleVectorInit.

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
      "Usage: fble-disassemble [OPTION ...] -m MODULE_PATH\n"
      "\n"
      "Description:\n"
      "  Disassemble an fble program.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n"
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n"
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on failure.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n"
      "  fble-disassemble -I prgms -m /Foo%\n"
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
int main(int argc, const char* argv[])
{
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* mpath_string = NULL;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseSearchPathArg(&search_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-m", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--module", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (help) {
    PrintUsage(stdout);
    FbleVectorFree(search_path);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  if (mpath_string == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr);
    FbleVectorFree(search_path);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleVectorFree(search_path);
    return EX_FAIL;
  }

  FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, NULL);
  FbleVectorFree(search_path);
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
