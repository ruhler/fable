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
#include <fble/fble-version.h>   // for FBLE_VERSION

#include "fble-disassemble.usage.h"  // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

extern const char* BUILDSTAMP;

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
  fprintf(stream, "%s", fbldUsageHelpText);
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
  FbleModuleArg module_arg = FbleNewModuleArg();
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
    if (FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    printf("fble-disassemble %s (%s)\n", FBLE_VERSION, FbleBuildStamp);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    PrintUsage(stdout);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr);
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr);
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  FbleLoadedProgram* prgm = FbleLoad(module_arg.search_path, module_arg.module_path, NULL);
  FbleFreeModuleArg(module_arg);
  if (prgm == NULL) {
    return EX_FAIL;
  }

  FbleCompiledModule* compiled = FbleCompileModule(prgm);
  FbleFreeLoadedProgram(prgm);

  if (compiled == NULL) {
    return EX_FAIL;
  }

  FbleDisassemble(stdout, compiled);

  FbleFreeCompiledModule(compiled);
  return EX_SUCCESS;
}
