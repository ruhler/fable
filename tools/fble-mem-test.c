// fble-mem-test.c --
//   This file implements the main entry point for the fble-mem-test program.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

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
      "Usage: fble-mem-test [--growth] FILE [PATH]\n"
      "FILE is an fble program evaluating to a function that takes a unary natural number.\n"
      "Exist status is 0 if running the function uses O(1) memory, 1 otherwise.\n"
      "Running the function means evaluating the function, and if it results in.\n"
      "a process, executing the process.\n"
      "If --growth is specified, 0 if the function use > O(1) memory, 1 otherwise.\n"
      "PATH is an optional include search path.\n"
  );
}

// Run --
//   Run the program, measuring maximum memory needed to evaluate f[n].
//
// Inputs:
//   prgm - the program to run.
//   use_n - the value of n to run for.
//   alloc_n - the value of n to allocate, which should match on all runs if
//             we want a fair memory comparison.
//   max_bytes - output set to the maximum bytes of memory used during the run.
//
// Results:
//   true if the program ran successfully, false otherwise.
//
// Side effects: 
//   Sets max_bytes to the maximum bytes used during the run.
bool Run(FbleLoadedProgram* prgm, size_t use_n, size_t alloc_n, size_t* max_bytes)
{
  assert(use_n <= alloc_n);

  size_t num_bits = 0;
  while (alloc_n > 0) {
    num_bits++;
    alloc_n /= 2;
  }

  bool success = false;
  FbleArena* arena = FbleNewArena();
  FbleProfile* profile = FbleNewProfile(arena);
  FbleCompiledProgram* compiled = FbleCompile(arena, prgm, profile);
  if (compiled != NULL) {
    FbleValueHeap* heap = FbleNewValueHeap(arena);
    FbleValue* linked = FbleLink(heap, compiled);
    FbleFreeCompiledProgram(arena, compiled);

    FbleValue* func = FbleEval(heap, linked, profile);
    FbleReleaseValue(heap, linked);
    if (func != NULL) {
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
        FbleRetainValue(heap, bit);
        FbleValue* xs[2] = { bit, tail };
        FbleValueV args = { .size = 2, .xs = xs };
        FbleValue* cons = FbleNewStructValue(heap, args);
        FbleReleaseValue(heap, bit);
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

      FbleValue* result = FbleApply(heap, func, &tail, profile);

      // As a special case, if the result of evaluation is a process, execute
      // the process. This allows us to test process execution.
      if (result != NULL && FbleIsProcValue(result)) {
        FbleIO io = { .io = &FbleNoIO };
        FbleValue* exec_result = FbleExec(heap, &io, result, profile);
        FbleReleaseValue(heap, result);
        result = exec_result;
      }

      success = (result != NULL);
      FbleReleaseValue(heap, result);
      FbleReleaseValue(heap, tail);
    }

    FbleReleaseValue(heap, func);
    FbleFreeValueHeap(heap);
  }
  FbleFreeProfile(arena, profile);

  *max_bytes = FbleArenaMaxSize(arena);
  FbleFreeArena(arena);
  return success;
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

  if (argc < 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return EX_USAGE;
  }
  
  const char* path = *argv;
  argc--;
  argv++;

  const char* include_path = NULL;
  if (argc > 0) {
    include_path = *argv;
  }

  FbleArena* arena = FbleNewArena();
  FbleLoadedProgram* prgm = FbleLoad(arena, path, include_path);
  if (prgm == NULL) {
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  bool debug = false;
  if (debug) {
    for (size_t i = 0; i <= 200; ++i) {
      size_t max_n = 0;
      if (!Run(prgm, i, 200, &max_n)) {
        FbleFreeLoadedProgram(arena, prgm);
        FbleFreeArena(arena);
        return EX_FAIL;
      }
      fprintf(stderr, "% 4zi: %zi\n", i, max_n);
    }
  }

  size_t small_n = 100;
  size_t large_n = 200;

  size_t max_small_n = 0;
  if (!Run(prgm, small_n, large_n, &max_small_n)) {
    FbleFreeLoadedProgram(arena, prgm);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  size_t max_large_n = 0;
  if (!Run(prgm, large_n, large_n, &max_large_n)) {
    FbleFreeLoadedProgram(arena, prgm);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  // The memory samples can be a little bit noisy. Be lenient to that.
  // Hopefully small memory growth will not be able to hide within 10% of the
  // small heap size.
  size_t noise = 10 * max_small_n / 100;
  if (!growth && max_large_n > max_small_n + noise) {
    fprintf(stderr, "memory growth of %zi bytes\n", max_large_n - max_small_n);
    FbleFreeLoadedProgram(arena, prgm);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  if (growth && max_large_n <= max_small_n + noise) {
    fprintf(stderr, "memory constant: M(%zi) = %zi, M(%zi) = %zi\n",
        small_n, max_small_n, large_n, max_large_n);
    FbleFreeLoadedProgram(arena, prgm);
    FbleFreeArena(arena);
    return EX_FAIL;
  }

  FbleFreeLoadedProgram(arena, prgm);
  FbleFreeArena(arena);
  return EX_SUCCESS;
}
