// test.c --
//   Implementation of FbleTestMain function.

#include "test.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleCompiledModuleFunction.
#include <fble/fble-profile.h>     // for FbleNewProfile, etc.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleVectorInit.
#include <fble/fble-version.h>     // for FBLE_VERSION

#include "test.usage.h"            // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_COMPILE_ERROR 1
#define EX_RUNTIME_ERROR 2
#define EX_USAGE_ERROR 3
#define EX_OTHER_ERROR 4

static void PrintHeader(FILE* stream, const char* arg0, FbleCompiledModuleFunction* module);
static void PrintVersion(FILE* stream);
static void PrintHelp(FILE* stream);

// PrintHeader 
//   Prints a header line to distinguish among different compiled fble-test
//   binaries when printing version and help text.
//
// @param stream  The output stream to write the header to.
// @param arg0  argv[0] of the main function, used to determine the binary name.
// @param module  The compiled module, or NULL
static void PrintHeader(FILE* stream, const char* arg0, FbleCompiledModuleFunction* module)
{
  if (module != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    // Load the module to figure out the path to it.
    FbleExecutableProgram* program = FbleAlloc(FbleExecutableProgram);
    FbleVectorInit(program->modules);
    module(program);
    FbleExecutableModule* mod = program->modules.xs[program->modules.size-1];

    fprintf(stream, "%s: fble-test -m ", binary_name);
    FblePrintModulePath(stream, mod->path);
    fprintf(stream, " (compiled)\n");

    FbleFreeExecutableProgram(program);
  }
}

// PrintVersion --
//   Prints version info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the version information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs version information to the given stream.
static void PrintVersion(FILE* stream)
{
  fprintf(stream, "fble-test %s (%s)\n", FBLE_VERSION, FbleBuildStamp);
}

// PrintHelp --
//   Prints help info for FbleTestMain the given output stream.
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

// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FbleCompiledModuleFunction* module)
{
  const char* arg0 = argv[0];

  FbleModuleArg module_arg = FbleNewModuleArg();
  const char* profile_file = NULL;
  bool help = false;
  bool error = false;
  bool version = false;

  argc--;
  argv++;
  while (!(help || error || version) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    PrintHeader(stdout, arg0, module);
    PrintVersion(stdout);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    PrintHeader(stdout, arg0, module);
    PrintHelp(stdout);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    PrintHeader(stderr, arg0, module);
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE_ERROR;
  }

  if (!module && !module_arg.module_path) {
    fprintf(stderr, "Error: missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE_ERROR;
  }

  if (module && module_arg.module_path) {
    fprintf(stderr, "Error: --module not allowed for fble-compiled binary.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE_ERROR;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "Error: unable to open %s for writing.\n", profile_file);
      FbleFreeModuleArg(module_arg);
      return EX_OTHER_ERROR;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_COMPILE_ERROR;
  }

  FbleValue* result = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);

  if (fprofile != NULL) {
    FbleProfileReport(fprofile, profile);
  }
  FbleFreeProfile(profile);

  if (result == NULL) {
    return EX_RUNTIME_ERROR;
  }
  return EX_SUCCESS;
}
