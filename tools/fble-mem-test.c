// fble-mem-test.c --
//   This file implements the main entry point for the fble-mem-test program.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble-alloc.h"   // for FbleMaxTotalBytesAllocated.
#include "fble-link.h"    // for FbleLinkFromSource.
#include "fble-main.h"    // for FbleMain
#include "fble-value.h"   // for FbleValue, etc.

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
      "Usage: fble-mem-test [--growth] " FBLE_MAIN_USAGE_SUMMARY "\n"
      FBLE_MAIN_USAGE_DETAIL
      "The fble program should evaluate to a function that takes a unary natural number.\n"
      "Exist status is 0 if running the function uses O(1) memory, 1 otherwise.\n"
      "Running the function means evaluating the function, and if it results in\n"
      "a process, executing the process.\n"
      "If --growth is specified, 0 if the function use > O(1) memory, 1 otherwise.\n"
  );
}

// Run --
//   Run the program, measuring maximum memory needed to evaluate f[n].
//
// Inputs:
//   heap - heap to use for allocations.
//   func - the function f to run.
//   profile - the profile to run with.
//   use_n - the value of n to run for.
//   alloc_n - the value of n to allocate, which should match on all runs if
//             we want a fair memory comparison.
//
// Returns:
//   The maximum number of bytes allocated while running the function.
//
// Side effects:
//   Resets the max total allocated bytes on the heap.
size_t Run(FbleValueHeap* heap, FbleValue func, FbleProfile* profile, size_t use_n, size_t alloc_n)
{
  assert(use_n <= alloc_n);

  size_t num_bits = 0;
  while (alloc_n > 0) {
    num_bits++;
    alloc_n /= 2;
  }

  // Number type is BitS@ from:
  // Unit@ Unit = Unit@();
  // @ Bit@ = +(Unit@ 0, Unit@ 1);
  // @ BitS@ = +(BitP@ cons, Unit@ nil),
  // @ BitP@ = *(Bit@ msb, BitS@ tail);
  FbleValue zero = FbleNewEnumValue(heap, 0);
  FbleValue one = FbleNewEnumValue(heap, 1);
  FbleValue tail = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < num_bits; ++i) {
    FbleValue bit = (use_n % 2 == 0) ? zero : one;
    use_n /= 2;
    FbleValue cons = FbleNewStructValue(heap, 2, bit, tail);
    FbleReleaseValue(heap, tail);
    tail = FbleNewUnionValue(heap, 0, cons);
    FbleReleaseValue(heap, cons);
  }
  FbleReleaseValue(heap, zero);
  FbleReleaseValue(heap, one);

  // Run an explicit full gc before applying the function so that garbage
  // collection during function application is not impacted by any
  // allocations made so far.
  FbleValueFullGc(heap);

  FbleResetMaxTotalBytesAllocated();
  FbleValue result = FbleApply(heap, func, &tail, profile);

  // As a special case, if the result of evaluation is a process, execute
  // the process. This allows us to test process execution.
  if (!FbleValueIsNull(result) && FbleIsProcValue(result)) {
    FbleIO io = { .io = &FbleNoIO };
    FbleValue exec_result = FbleExec(heap, &io, result, profile);
    FbleReleaseValue(heap, result);
    result = exec_result;
  }

  FbleReleaseValue(heap, result);
  FbleReleaseValue(heap, tail);
  return FbleMaxTotalBytesAllocated();
}

// main --
//   The main entry point for the fble-mem-test program.
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

  bool growth = false;
  if (argc > 0 && strcmp("--growth", *argv) == 0) {
    growth = true;
    argc--;
    argv++;
  }

  // Use a profile during tests to ensure memory behavior works properly with
  // profiling turned on.
  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue linked = FbleMain(heap, profile, FbleCompiledMain, argc, argv);
  if (FbleValueIsNull(linked)) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  FbleValue func = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);
  if (FbleValueIsNull(func)) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  bool debug = false;
  if (debug) {
    for (size_t i = 0; i <= 200; ++i) {
      size_t max_n = Run(heap, func, profile, i, 200);
      fprintf(stderr, "% 4zi: %zi\n", i, max_n);
    }
  }

  size_t small_n = 100;
  size_t large_n = 200;

  size_t max_small_n = Run(heap, func, profile, small_n, large_n);
  size_t max_large_n = Run(heap, func, profile, large_n, large_n);

  FbleReleaseValue(heap, func);
  FbleFreeValueHeap(heap);
  FbleFreeProfile(profile);

  // The memory samples can be a little bit noisy. Be lenient to that.
  // Hopefully small memory growth will not be able to hide within 10% of the
  // small heap size.
  size_t noise = 10 * max_small_n / 100;
  if (!growth && max_large_n > max_small_n + noise) {
    fprintf(stderr, "memory growth of %zi bytes\n", max_large_n - max_small_n);
    return EX_FAIL;
  }

  if (growth && max_large_n <= max_small_n + noise) {
    fprintf(stderr, "memory constant: M(%zi) = %zi, M(%zi) = %zi\n",
        small_n, max_small_n, large_n, max_large_n);
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
