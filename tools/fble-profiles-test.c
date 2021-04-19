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

  if (argc < 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  
  const char* path = *argv;
  argc--;
  argv++;

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromSource(heap, path, NULL, profile);
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

  // TODO: Add tests here.
  // * t, f, f2 each called once from the main block.
  // * Not created once.
  // * Not executed three times, once from t, once from f, and once from f2
  // * Not.true branch executed twice, Not.false branch executed once.

  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
