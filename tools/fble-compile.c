// fble-native.c --
//   This file implements the main entry point for the fble-native program,
//   which compiles *.fble code to *.c code.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"         // for FbleFree.
#include "fble-arg-parse.h"     // for FbleParseBoolArg, etc.
#include "fble-compile.h"       // for FbleCompile, etc.
#include "fble-module-path.h"   // for FbleParseModulePath.
#include "fble-vector.h"        // for FbleVectorInit.

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
      "Usage: fble-compile [OPTIONS...] -m MODULE_PATH\n"
      "\n"
      "Description:\n"
      "  Compiles an fble module to assembly code.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "    Print this help message and exit.\n"
      "  -I DIR\n"
      "    Add DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n"
      "  -c, --compile\n"
      "    Compile the module to assembly.\n"
      "  -e, --export NAME\n"
      "    Generate a function with the given NAME to export the module.\n"
      "  --main NAME\n"
      "    Generate a main function using NAME as the wrapper function.\n"
      "\n"
      "  At least one of --compile, --export, or --main must be provided.\n"
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on failure.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n"
      "  fble-compile -c -I prgms -m /Foo% > Foo.fble.s\n"
      "  fble-compile -e FbleCompiledMain -m /Foo% > Foo.fble.main.s\n"
      "  fble-compile --main FbleStdioMain -m /Foo% > Foo.fble.main.s\n"
  );
}

// main --
//   The main entry point for the fble-compile program.
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
  bool compile = false;
  const char* export = NULL;
  const char* main_ = NULL;
  const char* mpath_string = NULL;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseSearchPathArg("-I", &search_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-m", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--module", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-c", &compile, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--compile", &compile, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-e", &export, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--export", &export, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--main", &main_, &argc, &argv, &error)) continue;
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

  if (!compile && export == NULL && main_ == NULL) {
    fprintf(stderr, "one of --export NAME, --compile, or --main NAME must be specified.\n");
    PrintUsage(stderr);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFree(search_path.xs);
    return EX_FAIL;
  }

  if (export != NULL) {
    FbleGenerateAArch64Export(stdout, export, mpath);
  }

  if (main_ != NULL) {
    FbleGenerateAArch64Main(stdout, main_, mpath);
  }

  if (compile) {
    FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, NULL);
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

  FbleFree(search_path.xs);
  FbleFreeModulePath(mpath);
  return EX_SUCCESS;
}
