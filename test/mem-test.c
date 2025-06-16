/**
 * @file mem-test.c
 *  Implementation of FbleMemTestMain function.
 */

#include "mem-test.h"

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>       // for FbleMaxTotalBytesAllocated, etc.
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.

#include "fble-mem-test.usage.h"   // for fbldUsageHelpText

typedef struct {
  bool growth;
  bool debug;
} Args;

static bool ParseArg(void* dest, int* argc, const char*** argv, bool* error);
static size_t Run(FbleValueHeap* heap, FbleValue* func, FbleProfile* profile, size_t use_n, size_t alloc_n);

/**
 * @func[ParseArg] Arg parser for mem-tests.
 *  See documentation of FbleArgParser in fble-arg-parse.h for more info.
 */
static bool ParseArg(void* dest, int* argc, const char*** argv, bool* error)
{
  Args* args = (Args*)dest;
  if (FbleParseBoolArg("--growth", &args->growth, argc, argv, error)) return true;
  if (FbleParseBoolArg("--debug", &args->debug, argc, argv, error)) return true;

  return false;
}

/**
 * @func[Run] Runs the program for f[n].
 *  @arg[FbleValueHeap*][heap] Heap to use for allocations.
 *  @arg[FbleValue*][func] The function f to run.
 *  @arg[FbleProfile*][profile] The profile to run with.
 *  @arg[size_t][use_n] The value of n to run for.
 *  @arg[size_t][alloc_n]
 *   The value of n to allocate, which should match on all runs if we want a
 *   fair memory comparison.
 *
 *  @returns[size_t] The number of max bytes allocated during the run.
 *
 *  @sideeffects
 *   Resets the number of max bytes allocated.
 */
static size_t Run(FbleValueHeap* heap, FbleValue* func, FbleProfile* profile, size_t use_n, size_t alloc_n)
{
  FbleValueFullGc(heap);
  FblePushFrame(heap);
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
  FbleValue* zero = FbleNewEnumValue(heap, 1, 0);
  FbleValue* one = FbleNewEnumValue(heap, 1, 1);
  FbleValue* tail = FbleNewEnumValue(heap, 1, 1);
  for (size_t i = 0; i < num_bits; ++i) {
    FbleValue* bit = (use_n % 2 == 0) ? zero : one;
    use_n /= 2;
    FbleValue* cons = FbleNewStructValue_(heap, 2, bit, tail);
    tail = FbleNewUnionValue(heap, 1, 0, cons);
  }

  FbleResetMaxTotalBytesAllocated();
  FbleApply(heap, func, 1, &tail, profile);
  FblePopFrame(heap, NULL);
  return FbleMaxTotalBytesAllocated();
}

// FbleMemTestMain -- see documentation in mem-test.h.
int FbleMemTestMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  Args args = { .growth = false, .debug = false };

  // Use a profile during tests to ensure memory behavior works properly with
  // profiling turned on.
  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  const char* profile_output_file = NULL;
  FbleValue* func = NULL;
  FblePreloadedModuleV builtins = { .size = 0, .xs = NULL };

  FbleMainStatus status = FbleMain(&ParseArg, &args, "fble-mem-test", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &func);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return status;
  }

  // Pick sufficiently large values for n to overcome any small constant memory
  // variations from run to run.
  size_t small_n = 1000;
  size_t large_n = 2000;

  if (args.debug) {
    for (size_t i = 0; i <= large_n; i++) {
      size_t max_n = Run(heap, func, profile, i, large_n);
      fprintf(stderr, "% 4zi: %zi\n", i, max_n);
    }
  }

  size_t max_small_n = Run(heap, func, profile, small_n, large_n);
  size_t max_large_n = Run(heap, func, profile, large_n, large_n);

  FbleFreeValueHeap(heap);
  FbleFreeProfile(profile);

  // Be a little lenient with memory usage, because there can be small
  // variations from run to run. If the total number of bytes increased is
  // less than the increase in N, it's very unlikely to be a memory leak.
  size_t margin = large_n - small_n - 1;

  if (!args.growth && max_large_n > max_small_n + margin) {
    fprintf(stderr, "memory growth of %zi (%zi -> %zi)\n",
        max_large_n - max_small_n, max_small_n, max_large_n);
    return FBLE_MAIN_OTHER_ERROR;
  }

  if (args.growth && max_large_n <= max_small_n + margin) {
    fprintf(stderr, "memory constant: M(%zi) = %zi, M(%zi) = %zi\n",
        small_n, max_small_n, large_n, max_large_n);
    return FBLE_MAIN_OTHER_ERROR;
  }

  return FBLE_MAIN_SUCCESS;
}
