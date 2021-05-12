// fble-test.c --
//   This file implements the main entry point for the fble-test program.

#include <assert.h>   // for assert
#include <stdbool.h>   // for bool
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-link.h"      // for FbleLinkFromSource.
#include "fble-profile.h"   // for FbleNewProfile, etc.
#include "fble-value.h"     // for FbleValue, etc.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintUsage(FILE* stream);
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
      "Usage: fble-test [--compile-error | --runtime-error] [--profile] FILE [PATH]\n"
      "Type check and evaluate the fble program from FILE.\n"
      "PATH is an optional include search path.\n"
      "If the result is a process, run the process.\n"
      "Exit status is 0 if the program produced no type or runtime errors, 1 otherwise.\n"
      "With --compile-error, exit status is 0 if the program produced a compilation error, 0 otherwise.\n"
      "With --runtime-error, exit status is 0 if the program produced a runtime error, 0 otherwise.\n"
      "With --profile, a profiling report is given after executing the program.\n"
  );
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
FbleValue* FbleCompiledMain(FbleValueHeap* heap, FbleProfile* profile);

static FbleValue* Main(FbleValueHeap* heap, const char* file, const char* dir, FbleProfile* profile)
{
  return FbleCompiledMain(heap, profile);
}
#else
static FbleValue* Main(FbleValueHeap* heap, const char* file, const char* dir, FbleProfile* profile)
{
  return FbleLinkFromSource(heap, file, dir, profile);
}
#endif // FbleCompiledMain

// main --
//   The main entry point for the fble-test program.
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

  bool expectCompileError = false;
  bool expectRuntimeError = false;
  if (argc > 0 && strcmp("--compile-error", *argv) == 0) {
    expectCompileError = true;
    argc--;
    argv++;
  } else if (argc > 0 && strcmp("--runtime-error", *argv) == 0) {
    expectRuntimeError = true;
    argc--;
    argv++;
  }

  bool report_profile = false;
  if (argc > 0 && strcmp("--profile", *argv) == 0) {
    report_profile = true;
    argc--;
    argv++;
  }

  const char* path = argc < 1 ? NULL : argv[0];
  const char* include_path = argc < 2 ? NULL : argv[1];

  FbleProfile* profile = report_profile ? FbleNewProfile() : NULL;
  FbleValueHeap* heap = FbleNewValueHeap();

  FILE* original_stderr = stderr;
  stderr = expectCompileError ? stdout : original_stderr;

  FbleValue* linked = Main(heap, path, include_path, profile);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return expectCompileError ? EX_SUCCESS : EX_FAIL;
  } else if (expectCompileError) {
    fprintf(original_stderr, "expected compile error, but none encountered.\n");
    FbleReleaseValue(heap, linked);
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  stderr = expectRuntimeError ? stdout : original_stderr;

  FbleValue* result = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  // As a special case, if the result of evaluation is a process, execute
  // the process. This allows us to test process execution.
  if (result != NULL && FbleIsProcValue(result)) {
    FbleIO io = { .io = &FbleNoIO, };
    FbleValue* exec_result = FbleExec(heap, &io, result, profile);
    FbleReleaseValue(heap, result);
    result = exec_result;
  }

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);

  if (report_profile) {
    FbleProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  if (result == NULL) {
    return expectRuntimeError ? EX_SUCCESS : EX_FAIL;
  } else if (expectRuntimeError) {
    fprintf(original_stderr, "expected runtime error, but none encountered.\n");
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
