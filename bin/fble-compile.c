// fble-native.c --
//   This file implements the main entry point for the fble-native program,
//   which compiles *.fble code to *.c code.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>         // for FbleFree.
#include <fble/fble-arg-parse.h>     // for FbleParseBoolArg, etc.
#include <fble/fble-compile.h>       // for FbleCompile, etc.
#include <fble/fble-module-path.h>   // for FbleParseModulePath.
#include <fble/fble-vector.h>        // for FbleVectorInit.
#include <fble/fble-version.h>       // for FBLE_VERSION

#include "fble-compile.usage.h"      // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

extern const char* BUILDSTAMP;

static void PrintVersion(FILE* stream);
static void PrintHelp(FILE* stream);

typedef enum {
  TARGET_AARCH64,
  TARGET_C
} Target;

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
  fprintf(stream, "fble-compile %s+%s\n", FBLE_VERSION, BUILDSTAMP);
}

// PrintHelp --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintHelp(FILE* stream)
{
  fprintf(stream, "%s", fbldUsageHelpText);
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
  FbleSearchPath* search_path = FbleNewSearchPath();
  bool compile = false;
  const char* export = NULL;
  const char* main_ = NULL;
  const char* mpath_string = NULL;
  const char* target_string = NULL;
  bool help = false;
  bool error = false;
  bool version = false;

  argc--;
  argv++;
  while (!(help || error || version)  && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (FbleParseSearchPathArg(search_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-m", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--module", &mpath_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-t", &target_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--target", &target_string, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-c", &compile, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--compile", &compile, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-e", &export, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--export", &export, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--main", &main_, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    PrintVersion(stdout);
    FbleFreeSearchPath(search_path);
    return EX_SUCCESS;
  }

  if (help) {
    PrintHelp(stdout);
    FbleFreeSearchPath(search_path);
    return EX_SUCCESS;
  }

  if (error) {
    PrintHelp(stderr);
    FbleFreeSearchPath(search_path);
    return EX_USAGE;
  }

  Target target;
  if (target_string == NULL) {
    target = TARGET_AARCH64;
  } else if (strcmp(target_string, "aarch64") == 0) {
    target = TARGET_AARCH64;
  } else if (strcmp(target_string, "c") == 0) {
    target = TARGET_C;
  } else {
    fprintf(stderr, "unsupported target '%s'\n", target_string);
    PrintHelp(stderr);
    FbleFreeSearchPath(search_path);
    return EX_USAGE;
  }

  if (!compile && export == NULL && main_ == NULL) {
    fprintf(stderr, "one of --export NAME, --compile, or --main NAME must be specified.\n");
    PrintHelp(stderr);
    FbleFreeSearchPath(search_path);
    return EX_USAGE;
  }

  FbleModulePath* mpath = FbleParseModulePath(mpath_string);
  if (mpath == NULL) {
    FbleFreeSearchPath(search_path);
    return EX_FAIL;
  }

  if (export != NULL) {
    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64Export(stdout, export, mpath); break;
      case TARGET_C: FbleGenerateCExport(stdout, export, mpath); break;
    }
  }

  if (main_ != NULL) {
    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64Main(stdout, main_, mpath); break;
      case TARGET_C: FbleGenerateCMain(stdout, main_, mpath); break;
    }
  }

  if (compile) {
    FbleLoadedProgram* prgm = FbleLoad(search_path, mpath, NULL);
    if (prgm == NULL) {
      FbleFreeModulePath(mpath);
      FbleFreeSearchPath(search_path);
      return EX_FAIL;
    }

    FbleCompiledModule* module = FbleCompileModule(prgm);
    FbleFreeLoadedProgram(prgm);

    if (module == NULL) {
      FbleFreeModulePath(mpath);
      FbleFreeSearchPath(search_path);
      return EX_FAIL;
    }

    FbleFreeModulePath(module->path);
    module->path = FbleCopyModulePath(mpath);

    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64(stdout, module); break;
      case TARGET_C: FbleGenerateC(stdout, module); break;
    }

    FbleFreeCompiledModule(module);
  }

  FbleFreeSearchPath(search_path);
  FbleFreeModulePath(mpath);
  return EX_SUCCESS;
}
