/**
 * @file test.c
 *  Implementation of FbleTestMain function.
 */

#include "test.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleLink
#include <fble/fble-main.h>        // for FbleMainArgs, etc.
#include <fble/fble-profile.h>     // for FbleNewProfile, etc.
#include <fble/fble-program.h>     // for FbleNativeModule
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-version.h>     // for FblePrintVersion

#include "fble-test.usage.h"       // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_COMPILE_ERROR 1
#define EX_RUNTIME_ERROR 2
#define EX_USAGE_ERROR 3
#define EX_OTHER_ERROR 4

// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FbleNativeModule* module)
{
  const char* arg0 = argv[0];

  FbleMainArgs main_args = FbleNewMainArgs();
  bool error = false;

  argc--;
  argv++;
  while (!(main_args.help || error || main_args.version) && argc > 0) {
    if (FbleParseMainArg(module != NULL, &main_args, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (main_args.version) {
    FblePrintCompiledHeaderLine(stdout, "fble-test", arg0, module);
    FblePrintVersion(stdout, "fble-test");
    FbleFreeMainArgs(main_args);
    return EX_SUCCESS;
  }

  if (main_args.help) {
    FblePrintCompiledHeaderLine(stdout, "fble-test", arg0, module);
    fprintf(stdout, "%s", fbldUsageHelpText);
    FbleFreeMainArgs(main_args);
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeMainArgs(main_args);
    return EX_USAGE_ERROR;
  }

  if (!module && !main_args.module.module_path) {
    fprintf(stderr, "Error: missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeMainArgs(main_args);
    return EX_USAGE_ERROR;
  }

  if (module && main_args.module.module_path) {
    fprintf(stderr, "Error: --module not allowed for fble-compiled binary.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeMainArgs(main_args);
    return EX_USAGE_ERROR;
  }

  FILE* fprofile = NULL;
  if (main_args.profile_file != NULL) {
    fprofile = fopen(main_args.profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "Error: unable to open %s for writing.\n", main_args.profile_file);
      FbleFreeMainArgs(main_args);
      return EX_OTHER_ERROR;
    }
  }

  FbleNativeModuleV native_search_path = { .xs = NULL, .size = 0 };
  if (module != NULL) {
    native_search_path.xs = &module;
    native_search_path.size = 1;
  }

  if (main_args.module.module_path == NULL) {
    main_args.module.module_path = FbleCopyModulePath(module->path);
  }

  FbleProfile* profile = FbleNewProfile(fprofile != NULL);
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = FbleLink(heap, profile, native_search_path, main_args.module.search_path, main_args.module.module_path, NULL);
  FbleFreeMainArgs(main_args);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_COMPILE_ERROR;
  }

  FbleValue* result = FbleEval(heap, linked, profile);
  FbleFreeValueHeap(heap);

  FbleGenerateProfileReport(fprofile, profile);
  FbleFreeProfile(profile);

  if (result == NULL) {
    return EX_RUNTIME_ERROR;
  }
  return EX_SUCCESS;
}
