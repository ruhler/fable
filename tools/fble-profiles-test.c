// fble-profiles-test.c --
//   A program that runs tests for profiling instrumentation of .fble code.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-link.h"      // for FbleLinkFromSource.
#include "fble-profile.h"   // for FbleNewProfile, etc.
#include "fble-value.h"     // for FbleValue, etc.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);
static size_t Count(FbleProfile* profile, const char* name);
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee);
static FbleValue* Main(FbleValueHeap* heap, const char* file, const char* dir, FbleProfile* profile);

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
      "Usage: fble-profiles-test prgms/Fble/ProfilesTest.fble\n"
      "Run the fble-profiles-test using the given ProfilesTest.fble file.\n"
      "Exit status is 0 on success, non-zero on test failure.\n"
  );
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
  FbleCallData* data = NULL;
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];
    if (strcmp(block->name.name->str, name) == 0) {
      assert(data == NULL && "duplicate entries with name");
      data = &block->block;
    }
  }
  assert(data != NULL && "no block found with given name");
  return data->count;
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
  FbleBlockProfile* caller_block = NULL;
  FbleBlockId caller_id = profile->blocks.size;
  FbleBlockId callee_id = profile->blocks.size;
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];
    if (strcmp(block->name.name->str, caller) == 0) {
      assert(caller_id == profile->blocks.size && "duplicate named caller");
      caller_block = block;
      caller_id = block->block.id;
      assert(caller_id != profile->blocks.size && "bad assumption for test");
    }
    if (strcmp(block->name.name->str, callee) == 0) {
      assert(callee_id == profile->blocks.size && "duplicate named callee");
      callee_id = block->block.id;
      assert(callee_id != profile->blocks.size && "bad assumption for test");
    }
  }
  assert(callee_id != profile->blocks.size && "no block matching callee name");
  assert(caller_id != profile->blocks.size && "no block matching caller name");

  for (size_t i = 0; i < caller_block->callees.size; ++i) {
    FbleCallData* call = caller_block->callees.xs[i];
    if (call->id == callee_id) {
      return call->count;
    }
  }
  return 0;
}

// Main --
//   Load the main fble program.
//
// See documentation of FbleLinkFromSource in fble-link.h
//
// #define FbleCompileMain to the exported name of the compiled fble code to
// load if you want to load compiled .fble code. Otherwise we'll load
// interpreted .fble code.
#ifdef FbleCompiledMain
FbleValue* FbleCompiledMain(FbleValueHeap* heap);

static FbleValue* Main(FbleValueHeap* heap, const char* file, const char* dir, FbleProfile* profile)
{
  return FbleCompiledMain(heap);
}
#else
static FbleValue* Main(FbleValueHeap* heap, const char* file, const char* dir, FbleProfile* profile)
{
  return FbleLinkFromSource(heap, file, dir, profile);
}
#endif // FbleCompiledMain

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

  const char* path = argc < 1 ? NULL : *argv;

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = Main(heap, path, NULL, profile);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  FbleValue* result = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  if (result != NULL && FbleIsProcValue(result)) {
    FbleIO io = { .io = &FbleNoIO, };
    FbleValue* exec_result = FbleExec(heap, &io, result, profile);
    FbleReleaseValue(heap, result);
    result = exec_result;
  }

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);

  if (result == NULL) {
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  // Dump the profile to make it easier to develop and debug the tests that
  // follow.
  FbleProfileReport(stdout, profile);

  // Each of these top level let bindings were executed once when the main
  // program ran.
  assert(1 == Calls(profile, "/%", "/%.Not"));
  assert(1 == Calls(profile, "/%", "/%.t"));
  assert(1 == Calls(profile, "/%", "/%.f"));
  assert(1 == Calls(profile, "/%", "/%.f2"));

  // The Not function was executed three times, once from each of t, f, and
  // f2.
  assert(1 == Calls(profile, "/%.t", "/%.Not!"));
  assert(1 == Calls(profile, "/%.f", "/%.Not!"));
  assert(1 == Calls(profile, "/%.f2", "/%.Not!"));

  // In total, we created Not once and executed three times. 
  assert(1 == Count(profile, "/%.Not"));
  assert(3 == Count(profile, "/%.Not!"));

  // The true branch of Not was executed twice, the false branch once.
  assert(2 == Calls(profile, "/%.Not!", "/%.Not!.true"));
  assert(1 == Calls(profile, "/%.Not!", "/%.Not!.false"));

  // The Id function was executed three times, once from each of e1, e2, and
  // e3 execution.
  assert(3 == Count(profile, "/%.Id!"));
  assert(1 == Calls(profile, "/%!.e1!", "/%.Id!"));
  assert(1 == Calls(profile, "/%!.e2!", "/%.Id!"));
  assert(1 == Calls(profile, "/%!.e3!", "/%.Id!"));

  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
