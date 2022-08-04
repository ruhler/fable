// test.c --
//   Implementation of FbleTestMain function.

#include "test.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"       // for FbleFree
#include "fble-arg-parse.h"   // for FbleParseBoolArg, etc.
#include "fble-link.h"        // for FbleCompiledModuleFunction.
#include "fble-profile.h"     // for FbleNewProfile, etc.
#include "fble-value.h"       // for FbleValue, etc.
#include "fble-vector.h"      // for FbleVectorInit.

#define EX_SUCCESS 0
#define EX_COMPILE_ERROR 1
#define EX_RUNTIME_ERROR 2
#define EX_USAGE_ERROR 3
#define EX_OTHER_ERROR 4

static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module);

// PrintUsage --
//   Prints help info for FbleTestMain the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module)
{
  fprintf(stream, "Usage: fble-test [OPTION...]%s\n",
      module == NULL ? " -m MODULE_PATH" : "");
  fprintf(stream, "%s",
      "\n"
      "Description:\n"
      "  Evaluates an fble program. If the result is a process, executes the\n"
      "  the process too.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n");
  if (module == NULL) {
    fprintf(stream, "%s",
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n");
  }
  fprintf(stream, "%s",
      "  --profile FILE\n"
      "    Writes a profile of the test run to FILE\n"
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on compile error.\n"
      "  2 on runtime error.\n"
      "  3 on usage error.\n"
      "  4 on other error.\n"
      "\n"
      "Example:\n");
  fprintf(stream, "%s%s\n",
      "  fble-test --profile test.prof ",
      module == NULL ? "-I foo -m /Foo% " : "");
}

// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FbleCompiledModuleFunction* module)
{
  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* module_path = NULL;
  const char* profile_file = NULL;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (!module && FbleParseSearchPathArg(&search_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("-m", &module_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("--module", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleFree(search_path.xs);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE_ERROR;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE_ERROR;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFree(search_path.xs);
      return EX_OTHER_ERROR;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleFree(search_path.xs);
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
