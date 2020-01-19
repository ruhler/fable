// fble-mem-test.c --
//   This file implements the main entry point for the fble-mem-test program.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static bool NoIO(FbleIO* io, FbleValueArena* arena, bool block);
static void PrintUsage(FILE* stream);
int main(int argc, char* argv[]);

// NoIO --
//   An IO function that does no IO.
//   See documentation in fble.h
static bool NoIO(FbleIO* io, FbleValueArena* arena, bool block)
{
  assert(!block && "blocked indefinately on no IO");
  return false;
}

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
//   use_large_n - true if we should run it for large_n, false otherwise.
//   max_bytes - output set to the maximum bytes of memory used during the run.
//
// Results:
//   true if the program ran successfully, false otherwise.
//
// Side effects: 
//   Sets max_bytes to the maximum bytes used during the run.
bool Run(FbleProgram* prgm, bool use_large_n, size_t* max_bytes)
{
  bool success = false;
  FbleArena* eval_arena = FbleNewArena();
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);
  FbleNameV blocks;
  FbleProfile* profile = NULL;
  FbleValue* func = FbleEval(value_arena, prgm, &blocks, &profile);
  if (func != NULL) {
    // Number type is: @ Nat@ = +(Nat@ S, Unit@ Z);
    FbleValueV args = { .size = 0, .xs = NULL };
    FbleValue* unit = FbleNewStructValue(value_arena, args);
    FbleValue* zero = FbleNewUnionValue(value_arena, 1, unit);
    FbleValue* large_n = zero;
    FbleValue* small_n = NULL;
    for (size_t i = 0; i < 200; i++) {
      large_n = FbleNewUnionValue(value_arena, 0, large_n);
      if (i == 100) {
        small_n = large_n;
      }
    }

    FbleValue* n = (use_large_n ? large_n : small_n);
    FbleValue* result = FbleApply(value_arena, func, n, profile);

    // As a special case, if the result of evaluation is a process, execute
    // the process. This allows us to test process execution.
    if (result != NULL && FbleIsProcValue(result)) {
      FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL } };
      FbleValue* exec_result = FbleExec(value_arena, &io, result, profile);
      FbleValueRelease(value_arena, result);
      result = exec_result;
    }

    success = (result != NULL);
    FbleValueRelease(value_arena, result);
    FbleValueRelease(value_arena, large_n);
  }

  FbleValueRelease(value_arena, func);
  FbleDeleteValueArena(value_arena);
  FbleFreeBlockNames(eval_arena, &blocks);
  FbleFreeProfile(eval_arena, profile);

  *max_bytes = FbleArenaMaxSize(eval_arena);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
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

  FbleArena* prgm_arena = FbleNewArena();
  FbleProgram* prgm = FbleLoad(prgm_arena, path, include_path);
  if (prgm == NULL) {
    FbleDeleteArena(prgm_arena);
    return EX_FAIL;
  }

  size_t max_small_n = 0;
  if (!Run(prgm, false, &max_small_n)) {
    FbleDeleteArena(prgm_arena);
    return EX_FAIL;
  }

  size_t max_large_n = 0;
  if (!Run(prgm, true, &max_large_n)) {
    FbleDeleteArena(prgm_arena);
    return EX_FAIL;
  }

  if (!growth && max_large_n > max_small_n) {
    fprintf(stderr, "memory growth of %zi bytes\n", max_large_n - max_small_n);
    FbleDeleteArena(prgm_arena);
    return EX_FAIL;
  }

  if (growth && max_large_n == max_small_n) {
    fprintf(stderr, "memory constant\n");
    FbleDeleteArena(prgm_arena);
    return EX_FAIL;
  }

  FbleDeleteArena(prgm_arena);
  return EX_SUCCESS;
}
