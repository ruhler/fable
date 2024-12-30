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

#include "fble-test.usage.h"       // for fbldUsageHelpText

// FbleTestMain -- see documentation in test.h
int FbleTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  FbleProfile* profile = FbleNewProfile(false);
  FbleValueHeap* heap = FbleNewValueHeap();
  FILE* profile_output_file = NULL;
  FbleValue* result = NULL;

  FbleMainStatus status = FbleMain(NULL, NULL, "fble-test", fbldUsageHelpText,
      &argc, &argv, preloaded, heap, profile, &profile_output_file, &result);

  FbleFreeValueHeap(heap);

  FbleGenerateProfileReport(profile_output_file, profile);
  FbleFreeProfile(profile);
  return status;
}
