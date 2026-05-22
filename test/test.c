/**
 * @file test.c
 *  Implementation of FbleTestMain function.
 */

#include "test.h"

#include <fble/fble-main.h>        // for FbleMain, etc.
#include <fble/fble-profile.h>     // for FbleNewProfile, etc.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector, etc.

#include "fble-test.usage.h"       // for fbldUsageHelpText

extern FblePreloadedModule _Fble_2f_SpecTests_2f_Builtin_25_;

// Defined in foreign.c
void FbleTestRegisterForeignValues(FbleValueHeap* heap);


// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  const char* profile_output_file = NULL;
  uint64_t profile_sample_period = 0;
  FbleValue* result = NULL;

  FbleTestRegisterForeignValues(heap);

  argv[argc++] = "--";
  FbleMainStatus status = FbleMain(NULL, NULL, "fble-test", fbldUsageHelpText,
      &argc, &argv, preloaded, heap, profile, &profile_output_file, &profile_sample_period, &result);

  FbleFreeValueHeap(heap);

  if (profile_output_file != NULL) {
    FbleOutputProfile(profile_output_file, profile, 0);
  }
  FbleFreeProfile(profile);
  return status;
}
