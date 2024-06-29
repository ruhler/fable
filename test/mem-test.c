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
#include <fble/fble-generate.h>    // for FbleGeneratedModule
#include <fble/fble-link.h>        // for FbleLink.
#include <fble/fble-usage.h>       // for FblePrintUsageDoc
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-version.h>     // for FBLE_VERSION, FbleBuildStamp.

#include "memory.h"   // for FbleGetMaxMemoryUsageKB

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static size_t Run(FbleValueHeap* heap, FbleValue* func, FbleProfile* profile, size_t use_n, size_t alloc_n);

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
  FbleValue* zero = FbleNewEnumValue(heap, 0);
  FbleValue* one = FbleNewEnumValue(heap, 1);
  FbleValue* tail = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < num_bits; ++i) {
    FbleValue* bit = (use_n % 2 == 0) ? zero : one;
    use_n /= 2;
    FbleValue* cons = FbleNewStructValue_(heap, 2, bit, tail);
    tail = FbleNewUnionValue(heap, 0, cons);
  }

  FbleValueFullGc(heap);
  FbleResetMaxTotalBytesAllocated();
  FbleApply(heap, func, 1, &tail, profile);
  return FbleMaxTotalBytesAllocated();
}

// FbleMemTestMain -- see documentation in mem-test.h.
int FbleMemTestMain(int argc, const char** argv, FbleGeneratedModule* module)
{
  const char* arg0 = argv[0];

  FbleModuleArg module_arg = FbleNewModuleArg();
  bool help = false;
  bool error = false;
  bool version = false;
  bool growth = false;
  bool debug = false;

  argc--;
  argv++;
  while (!(help || error || version) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (!module && FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--growth", &growth, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--debug", &debug, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    FblePrintCompiledHeaderLine(stdout, "fble-mem-test", arg0, module);
    FblePrintVersion(stdout, "fble-mem-test");
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    FblePrintCompiledHeaderLine(stdout, "fble-mem-test", arg0, module);
    FblePrintUsageDoc(arg0, "fble-mem-test.usage.txt");
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (!module && module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  // Use a profile during tests to ensure memory behavior works properly with
  // profiling turned on.
  FbleProfile* profile = FbleNewProfile(true);
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLink(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  FbleValue* func = FbleEval(heap, linked, profile);
  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  // Pick sufficiently large values for n to overcome any constant memory
  // overheads for setting up the process.
  size_t small_n = 10000;
  size_t large_n = 20000;

  if (debug) {
    for (size_t i = small_n; i <= large_n; i += 100) {
      size_t max_n = Run(heap, func, profile, i, large_n);
      fprintf(stderr, "% 4zi: %zi\n", i, max_n);
    }
  }


  size_t max_small_n = Run(heap, func, profile, small_n, large_n);
  size_t max_large_n = Run(heap, func, profile, large_n, large_n);

  FbleFreeValueHeap(heap);
  FbleFreeProfile(profile);

  // Be a little lenient with memory usage.
  size_t margin = 0;

  if (!growth && max_large_n > max_small_n + margin) {
    fprintf(stderr, "memory growth of %zi (%zi -> %zi)\n",
        max_large_n - max_small_n, max_small_n, max_large_n);
    return EX_FAIL;
  }

  if (growth && max_large_n <= max_small_n + margin) {
    fprintf(stderr, "memory constant: M(%zi) = %zi, M(%zi) = %zi\n",
        small_n, max_small_n, large_n, max_large_n);
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
