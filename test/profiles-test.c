/**
 * @file profiles-test.c
 *  Implementation of FbleProfilesTestMain function.
 */

#include "profiles-test.h"

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-name.h>        // for FbleName.
#include <fble/fble-profile.h>     // for FbleNewProfile, etc.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.

#include "fble-profiles-test.usage.h"    // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

/**
 * @struct[CountQueryData] User data for CountQuery.
 *  @field[FbleBlockId][id] Id of the block to count.
 *  @field[size_t][count] Count.
 */
typedef struct {
  FbleBlockId id;
  size_t count;
} CountQueryData;

static void CountQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time);
static size_t Count(FbleProfile* profile, const char* name);

/**
 * @struct[CallsQueryData] User data for CallsQuery.
 *  @field[FbleBlockId][caller] Id of the caller block.
 *  @field[FbleBlockId][callee] Id of the callee block.
 *  @field[size_t][count] Count of calls.
 */
typedef struct {
  FbleBlockId caller;
  FbleBlockId callee;
  size_t count;
} CallsQueryData;

static void CallsQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time);
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee);

/**
 * @func[CountQuery] FbleProfileQuery for the Count function.
 *  See documentation of FbleProfileQuery in fble-profile.h.
 */
static void CountQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time)
{
  CountQueryData* data = (CountQueryData*)userdata;
  if (seq.size > 0 && seq.xs[seq.size-1] == data->id) {
    data->count += count;
  }
}

/**
 * @func[Count] Gets the count for a profile block.
 *  Return the total number of times the profiling block with the given name
 *  was called in the profile.
 *
 *  @arg[FbleProfile*][profile] The profile to get the count from.
 *  @arg[const char*][name] The name of the block to get the count for.
 *
 *  @returns[size_t]
 *   The number of times the block was called.
 *
 *  @sideeffects
 *   Asserts with failure if there are no blocks with the given name or there
 *   is more than one block with the given name.
 */
static size_t Count(FbleProfile* profile, const char* name)
{
  CountQueryData data;
  data.id = FbleLookupProfileBlockId(profile, name);
  assert(data.id != 0 && "Block id not found?");

  data.count = 0;
  FbleQueryProfile(profile, &CountQuery, &data);
  return data.count;
}

/**
 * @func[CallsQuery] FbleProfileQuery for the Calls function.
 *  See documentation of FbleProfileQuery in fble-profile.h.
 */
static void CallsQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time)
{
  CallsQueryData* data = (CallsQueryData*)userdata;
  if (seq.size > 1
      && seq.xs[seq.size-1] == data->callee
      && seq.xs[seq.size-2] == data->caller) {
    data->count += count;
  }
}

/**
 * @func[Calls] Gets the number of times a caller calls a callee.
 *  @arg[FbleProfile*][profile] The profile to get the count from.
 *  @arg[const char*][caller] The name of the caller block.
 *  @arg[const char*][callee] The name of the callee block.
 *
 *  @returns[size_t]
 *   The number of times the caller directly called the callee.
 *
 *  @sideeffects
 *   Asserts with failure if there are no blocks matching the names of callee
 *   and caller.
 */
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee)
{
  CallsQueryData data;
  data.caller = FbleLookupProfileBlockId(profile, caller);
  assert(data.caller != 0 && "Caller id not found");

  data.callee = FbleLookupProfileBlockId(profile, callee);
  assert(data.callee != 0 && "Callee id not found");

  data.count = 0;
  FbleQueryProfile(profile, &CallsQuery, &data);
  return data.count;
}

// FbleProfilesTestMain -- see documentation in profiles-test.h
int FbleProfilesTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  FbleProfile* profile = FbleNewProfile(true);
  FbleValueHeap* heap = FbleNewValueHeap();
  FILE* profile_output_file = NULL;
  FbleValue* result = NULL;
  FblePreloadedModuleV builtins = { .size = 0, .xs = NULL };

  FbleMainStatus status = FbleMain(NULL, NULL, "fble-profiles-test", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &result);

  FbleFreeValueHeap(heap);

  if (result == NULL) {
    FbleFreeProfile(profile);
    return status;
  }

  // Dump the profile to make it easier to develop and debug the tests that
  // follow.
  if (profile_output_file == NULL) {
    profile_output_file = stdout;
  }
  FbleOutputProfile(profile_output_file, profile);

  // Each of these top level let bindings were executed once when the main
  // program ran.
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.Not"));
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.t"));
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.f"));
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.f2"));

  // The Not function was executed three times, once from each of t, f, and
  // f2.
  assert(1 == Calls(profile, "/ProfilesTest%.t", "/ProfilesTest%.Not!"));
  assert(1 == Calls(profile, "/ProfilesTest%.f", "/ProfilesTest%.Not!"));
  assert(1 == Calls(profile, "/ProfilesTest%.f2", "/ProfilesTest%.Not!"));

  // In total, we created Not once and executed three times. 
  assert(1 == Count(profile, "/ProfilesTest%.Not"));
  assert(3 == Count(profile, "/ProfilesTest%.Not!"));

  // The true branch of Not was executed twice, the false branch once.
  assert(2 == Calls(profile, "/ProfilesTest%.Not!", "/ProfilesTest%.Not!.true"));
  assert(1 == Calls(profile, "/ProfilesTest%.Not!", "/ProfilesTest%.Not!.false"));

  // Regression test for a bug where the location for the top level profile
  // block was a module path instead of a file path.
  {
    FbleBlockId block = FbleLookupProfileBlockId(profile, "/ProfilesTest%");
    assert(block != 0);

    FbleName* name = FbleProfileBlockName(profile, block);
    assert(strstr(name->loc.source->str, "test/ProfilesTest.fble"));
  }

  FbleFreeProfile(profile);
  return FBLE_MAIN_SUCCESS;
}
