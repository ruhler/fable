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

// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  const char* profile_output_file = NULL;
  FbleValue* result = NULL;

  FblePreloadedModuleV builtins;
  FbleInitVector(builtins);
  FbleAppendToVector(builtins, &_Fble_2f_SpecTests_2f_Builtin_25_);

  FbleMainStatus status = FbleMain(NULL, NULL, "fble-test", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &result);

  FbleFreeVector(builtins);
  FbleFreeValueHeap(heap);

  if (profile_output_file != NULL) {
    FbleOutputProfile(profile_output_file, profile);
  }
  FbleFreeProfile(profile);
  return status;
}
