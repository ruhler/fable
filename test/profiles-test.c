/**
 * @file profiles-test.c
 *  Implementation of FbleProfilesTestMain function.
 */

#include "profiles-test.h"

#include <assert.h>   // for assert
#include <inttypes.h> // for PRIu64 
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

static FbleBlockId LookupBlockId(FbleProfile* profile, const char* name);
static void OutputQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples);
static void Output(FbleProfile* profile);

/**
 * @struct[CountQueryData] User data for CountQuery.
 *  @field[FbleBlockId][id] Id of the block to count.
 *  @field[size_t][calls] Count of calls.
 */
typedef struct {
  FbleBlockId id;
  size_t calls;
} CountQueryData;

static void CountQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples);
static size_t Count(FbleProfile* profile, const char* name);

/**
 * @struct[CallsQueryData] User data for CallsQuery.
 *  @field[FbleBlockId][caller] Id of the caller block.
 *  @field[FbleBlockId][callee] Id of the callee block.
 *  @field[size_t][calls] Count of calls.
 */
typedef struct {
  FbleBlockId caller;
  FbleBlockId callee;
  size_t calls;
} CallsQueryData;

static void CallsQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples);
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee);

/**
 * @func[LookupBlockId] Looks up the block id of a named block.
 *  @arg[FbleProfile*][profile] The profile to look in.
 *  @arg[const char*][name] The name of the block to look up.
 *  @returns[FbleBlockId] The id of the block, 0 if not found.
 *  @sideeffects None
 */
FbleBlockId LookupBlockId(FbleProfile* profile, const char* name)
{
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    if (strcmp(name, profile->blocks.xs[i].name->str) == 0) {
      return i;
    }
  }
  return 0;
}

/**
 * @func[OutputQuery] Query to use for outputting samples for debug.
 *  See documentation of FbleProfileQuery in fble-profile.h
 */
static void OutputQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples)
{
  printf("%" PRIu64, calls);
  for (size_t i = 0; i < seq.size; ++i) {
    FbleName name = profile->blocks.xs[seq.xs[i]];
    printf(" %s", name.name->str);
  }
  printf("\n");
}

/**
 * @func[Output] Outputs a profile for debug purposes.
 *  @arg[FbleProfile*][profile] The profile to output.
 *  @sideeffects Outputs the profile in text form to stdout.
 */
static void Output(FbleProfile* profile)
{
  FbleQueryProfile(profile, &OutputQuery, NULL);
}

/**
 * @func[CountQuery] FbleProfileQuery for the Count function.
 *  See documentation of FbleProfileQuery in fble-profile.h.
 */
static void CountQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples)
{
  CountQueryData* data = (CountQueryData*)userdata;
  if (seq.size > 0 && seq.xs[seq.size-1] == data->id) {
    data->calls += calls;
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
  data.id = LookupBlockId(profile, name);
  assert(data.id != 0 && "Block id not found?");

  data.calls = 0;
  FbleQueryProfile(profile, &CountQuery, &data);
  return data.calls;
}

/**
 * @func[CallsQuery] FbleProfileQuery for the Calls function.
 *  See documentation of FbleProfileQuery in fble-profile.h.
 */
static void CallsQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples)
{
  CallsQueryData* data = (CallsQueryData*)userdata;
  if (seq.size > 1
      && seq.xs[seq.size-1] == data->callee
      && seq.xs[seq.size-2] == data->caller) {
    data->calls += calls;
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
  data.caller = LookupBlockId(profile, caller);
  assert(data.caller != 0 && "Caller id not found");

  data.callee = LookupBlockId(profile, callee);
  assert(data.callee != 0 && "Callee id not found");

  data.calls = 0;
  FbleQueryProfile(profile, &CallsQuery, &data);
  return data.calls;
}

// FbleProfilesTestMain -- see documentation in profiles-test.h
int FbleProfilesTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  FbleProfile* profile = FbleNewProfile();
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

  assert(profile->enabled && "--profile must be passed for this test");

  // Output the profile to stdout to help with debug.
  Output(profile);

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
    FbleBlockId block = LookupBlockId(profile, "/ProfilesTest%");
    assert(block != 0);

    FbleName name = profile->blocks.xs[block];
    assert(strstr(name.loc.source->str, "test/ProfilesTest.fble"));
  }

  FbleFreeProfile(profile);
  return FBLE_MAIN_SUCCESS;
}
