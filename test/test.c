/**
 * @file test.c
 *  Implementation of FbleTestMain function.
 */

#include "test.h"

#include <assert.h>   // for assert
#include <stdbool.h>  // for bool
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-main.h>        // for FbleMain, etc.
#include <fble/fble-profile.h>     // for FbleNewProfile, etc.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector, etc.

#include "fble-test.usage.h"       // for fbldUsageHelpText

extern FblePreloadedModule _Fble_2f_SpecTests_2f_Builtin_25_;

static FbleValue* Foreign1_Not_impl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);

static FbleExecutable Foreign1_Not = {
  .num_args = 1,
  .num_statics = 0,
  .max_call_args = 0,
  .run = &Foreign1_Not_impl
};

/**
 * @func[Foreign1_Not_impl] FbleRunFunction for 'Not' foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Bool@) { Bool@; }}.
 *
 *  @sideeffects None
 */
static FbleValue* Foreign1_Not_impl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  FbleValue* arg = args[0];
  switch (FbleUnionValueTag(arg, 1)) {
    case 0: return FbleNewEnumValue(heap, 1, 1);
    case 1: return FbleNewEnumValue(heap, 1, 0);
  }
  return NULL;
}

// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  const char* profile_output_file = NULL;
  uint64_t profile_sample_period = 0;
  FbleValue* result = NULL;

  FbleModulePath* foreign1 = FbleParseModulePath("/SpecTests/'10.1-ForeignFuncValue'/Basic/Basic%");
  FbleRegisterForeignFunction(heap, foreign1, "Not", Foreign1_Not);
  FbleFreeModulePath(foreign1);

  FblePreloadedModuleV builtins;
  FbleInitVector(builtins);
  FbleAppendToVector(builtins, &_Fble_2f_SpecTests_2f_Builtin_25_);

  argv[argc++] = "--";
  FbleMainStatus status = FbleMain(NULL, NULL, "fble-test", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &profile_sample_period, &result);

  FbleFreeVector(builtins);
  FbleFreeValueHeap(heap);

  if (profile_output_file != NULL) {
    FbleOutputProfile(profile_output_file, profile, 0);
  }
  FbleFreeProfile(profile);
  return status;
}
