// fble-profiles-test.c --
//   A program that runs tests for profiling instrumentation of .fble code.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-main.h"      // for FbleMain.
#include "fble-name.h"      // for FbleName.
#include "fble-profile.h"   // for FbleNewProfile, etc.
#include "fble-value.h"     // for FbleValue, etc.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);
static FbleBlockProfile* Block(FbleProfile* profile, const char* name);
static size_t Count(FbleProfile* profile, const char* name);
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee);

int main(int argc, char* argv[]);

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
  fprintf(stream,
      "Usage: fble-profiles-test " FBLE_MAIN_USAGE_SUMMARY "\n"
      "Run the fble-profiles-test on the ProfilesTest.fble program.\n"
      FBLE_MAIN_USAGE_DETAIL
      "Exit status is 0 on success, non-zero on test failure.\n"
  );
}
// Block --
//   Look up information for the given named block.
//
// Inputs:
//   profile - the profile to get the block info for.
//   name - the name of the block to get.
//
// Results:
//   The block.
//
// Side effects:
//   Asserts with failure if there are no blocks with the given name or there
//   is more than one block with the given name.
static FbleBlockProfile* Block(FbleProfile* profile, const char* name)
{
  FbleBlockProfile* result = NULL;
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];
    if (strcmp(block->name.name->str, name) == 0) {
      assert(result == NULL && "duplicate entries with name");
      result = block;
    }
  }
  assert(result != NULL && "no block found with given name");
  return result;
}

// Count --
//   Return the total number of times the profiling block with the given name
//   was called in the profile.
//
// Inputs:
//   profile - the profile to get the count from.
//   name - the name of the block to get the count for.
//
// Results:
//   The number of times the block was called.
//
// Side effects:
//   Asserts with failure if there are no blocks with the given name or there
//   is more than one block with the given name.
static size_t Count(FbleProfile* profile, const char* name)
{
  return Block(profile,name)->block.count;
}

// Calls --
//   Return the total number of times the callee block called the caller
//   block.
//
// Inputs:
//   profile - the profile to get the count from.
//   caller - the name of the caller block.
//   callee - the name of the callee block.
//
// Results:
//   The number of times the callee directly called the caller.
//
// Side effects:
//   Asserts with failure if there are no blocks matching the names of callee
//   and caller or if there are multiple blocks matching the names.
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee)
{
  FbleBlockProfile* caller_block = Block(profile, caller);
  FbleBlockProfile* callee_block = Block(profile, callee);

  FbleBlockId callee_id = callee_block->block.id;
  for (size_t i = 0; i < caller_block->callees.size; ++i) {
    FbleCallData* call = caller_block->callees.xs[i];
    if (call->id == callee_id) {
      return call->count;
    }
  }
  return 0;
}

// main --
//   The main entry point for the fble-profiles-test program.
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
int main(int argc, char* argv[])
{
  argc--;
  argv++;
  if (argc > 0 && strcmp("--help", *argv) == 0) {
    PrintUsage(stdout);
    return EX_SUCCESS;
  }

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue linked = FbleMain(heap, profile, FbleCompiledMain, argc, argv);
  if (FbleValueIsNull(linked)) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  FbleValue result = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  if (!FbleValueIsNull(result) && FbleIsProcValue(result)) {
    FbleIO io = { .io = &FbleNoIO, };
    FbleValue exec_result = FbleExec(heap, &io, result, profile);
    FbleReleaseValue(heap, result);
    result = exec_result;
  }

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);

  if (FbleValueIsNull(result)) {
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  // Dump the profile to make it easier to develop and debug the tests that
  // follow.
  FbleProfileReport(stdout, profile);

  // Each of these top level let bindings were executed once when the main
  // program ran.
  assert(1 == Calls(profile, "/Fble/ProfilesTest%", "/Fble/ProfilesTest%.Not"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%", "/Fble/ProfilesTest%.t"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%", "/Fble/ProfilesTest%.f"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%", "/Fble/ProfilesTest%.f2"));

  // The Not function was executed three times, once from each of t, f, and
  // f2.
  assert(1 == Calls(profile, "/Fble/ProfilesTest%.t", "/Fble/ProfilesTest%.Not!"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%.f", "/Fble/ProfilesTest%.Not!"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%.f2", "/Fble/ProfilesTest%.Not!"));

  // In total, we created Not once and executed three times. 
  assert(1 == Count(profile, "/Fble/ProfilesTest%.Not"));
  assert(3 == Count(profile, "/Fble/ProfilesTest%.Not!"));

  // The true branch of Not was executed twice, the false branch once.
  assert(2 == Calls(profile, "/Fble/ProfilesTest%.Not!", "/Fble/ProfilesTest%.Not!.true"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%.Not!", "/Fble/ProfilesTest%.Not!.false"));

  // The Id function was executed three times, once from each of e1, e2, and
  // e3 execution.
  assert(3 == Count(profile, "/Fble/ProfilesTest%.Id!"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%!.e1!", "/Fble/ProfilesTest%.Id!"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%!.e2!", "/Fble/ProfilesTest%.Id!"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%!.e3!", "/Fble/ProfilesTest%.Id!"));

  // Regression test for a bug where had tail-calling the builtin put
  // function. The builtin put didn't do any Enter/Exit calls, and we were
  // using AutoExit to do the tail call. As a result, the profiler thought the
  // caller of the put was calling into whatever function was executed after
  // the caller returned, which is clearly wrong.
  assert(0 == Calls(profile, "/Fble/ProfilesTest%!.A!!.b", "/Fble/ProfilesTest%!.D!"));
  assert(1 == Calls(profile, "/Fble/ProfilesTest%!.A!!", "/Fble/ProfilesTest%!.D!"));

  // Regression test for a bug where the location for the top level profile
  // block was a module path instead of a file path.
  {
    FbleBlockProfile* block = Block(profile, "/Fble/ProfilesTest%");
    assert(strcmp(block->name.loc.source->str, "prgms/Fble/ProfilesTest.fble") == 0);
  }

  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
