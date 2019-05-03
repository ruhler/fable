// fble-test.c --
//   This file implements the main entry point for the fble-test program.

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
      "Usage: fble-test [--error] [--memory] FILE [PATH]\n"
      "Type check and evaluate the fble program from FILE.\n"
      "PATH is an optional include search path.\n"
      "If the result is a process, run the process.\n"
      "Exit status is 0 if the program produced no type or runtime errors, 1 otherwise.\n"
      "With --error, exit status is 0 if the program produced a type or runtime error, 0 otherwise.\n"
      "With --memory, a memory report is given after executing the program.\n"
  );
}

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

  bool expect_error = false;
  if (argc > 0 && strcmp("--error", *argv) == 0) {
    expect_error = true;
    argc--;
    argv++;
  }

  bool report_memory = false;
  if (argc > 0 && strcmp("--memory", *argv) == 0) {
    report_memory = true;
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

  FILE* stderr_save = stderr;
  stderr = expect_error ? stdout : stderr;

  FbleArena* arena = FbleNewArena(NULL);

  FbleExpr* prgm = FbleParse(arena, path, include_path);
  FbleValue* result = NULL;
  if (prgm != NULL) {
    FbleArena* eval_arena = FbleNewArena(arena);
    FbleValueArena* value_arena = FbleNewValueArena(eval_arena);
    result = FbleEval(value_arena, prgm);

    // As a special case, if the result of evaluation is a process, execute
    // the process. This allows us to test process execution.
    if (result != NULL && result->tag == FBLE_PROC_VALUE) {
      FbleIO io = { .io = &NoIO, .ports = { .size = 0, .xs = NULL } };
      FbleValue* exec_result = FbleExec(value_arena, &io, (FbleProcValue*)result);
      FbleValueRelease(value_arena, result);
      result = exec_result;
    }

    FbleValueRelease(value_arena, result);
    FbleDeleteValueArena(value_arena);
    FbleAssertEmptyArena(eval_arena);
    FbleDeleteArena(eval_arena);
  }

  if (report_memory) {
    printf("Max memory used: %zi (bytes)\n", FbleArenaMaxSize(arena));
  }

  FbleDeleteArena(arena);

  if (result == NULL) {
    return expect_error ? EX_SUCCESS : EX_FAIL;
  }

  if (expect_error) {
    fprintf(stderr_save, "expected error, but none encountered.\n");
    return EX_FAIL;
  }

  return EX_SUCCESS;
}
