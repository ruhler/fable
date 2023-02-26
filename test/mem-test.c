// mem-test.c --
//   Implementation of FbleMemTestMain function.

#include "mem-test.h"

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>       // for FbleFree, FbleMaxTotalBytesAllocated.
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleLinkFromCompiledOrSource.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleVectorInit.

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

size_t Run(FbleValueHeap* heap, FbleValue* func, FbleProfile* profile, size_t use_n, size_t alloc_n);
static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module);

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
size_t Run(FbleValueHeap* heap, FbleValue* func, FbleProfile* profile, size_t use_n, size_t alloc_n)
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
  FbleValue* result = FbleApply(heap, func, &tail, profile);

  FbleReleaseValue(heap, result);
  FbleReleaseValue(heap, tail);
  return FbleMaxTotalBytesAllocated();
}

// PrintUsage --
//   Prints help info for FbleMemTestMain to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module)
{
  fprintf(stream, "Usage: fble-mem-test [OPTION...]%s\n",
      module == NULL ? " -m MODULE_PATH" : "");
  fprintf(stream, "%s",
      "\n"
      "Description:\n"
      "  Tests for memory behavior of an fble function. The function should\n"
      "  take a /SpecTests/Nat%.Nat@ as an argument. The function is evaluated\n"
      "  with various values to see how memory use depends on the input value.\n"
      "  If the function returns a process, the resulting process is executed\n"
      "  with its memory use included for the purpose of memory testing.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n");
  if (module == NULL) {
    fprintf(stream, "%s",
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n");
  }
  fprintf(stream, "%s",
      "  --growth\n"
      "     Expect the function to use greater than O(1) memory. Exits with\n"
      "     status 0 if that is the case, non-zero otherwise.\n"
      "  --debug\n"
      "     Output graph of memory as a function of N, to help with debugging.\n"
      "\n"
      "Exit Status:\n"
      "  0 if memory usage is O(1) (except in case where --growth is specified).\n"
      "  1 if memory usage is greater than O(1) (except in case where --growth\n"
      "    is specified.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n");
  fprintf(stream, "%s%s",
      "  fble-mem-test ",
      module == NULL ? "-I foo -m /Foo% " : "");
}

// FbleMemTestMain -- see documentation in mem-test.h.
int FbleMemTestMain(int argc, const char** argv, FbleCompiledModuleFunction* module)
{
  FbleModuleArg module_arg = FbleNewModuleArg();
  bool help = false;
  bool error = false;
  bool growth = false;
  bool debug = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (!module && FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--growth", &growth, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--debug", &debug, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (!module && module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  // Use a profile during tests to ensure memory behavior works properly with
  // profiling turned on.
  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  FbleValue* func = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);
  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

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
  // Hopefully small memory growth will not be able to hide within a small
  // percent of the small heap size.
  size_t noise = 4 * max_small_n / 100;
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
