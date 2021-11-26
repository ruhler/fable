// fble-stdio
//   A program to run fble programs with a /Stdio%.Stdio@ interface.

#define _GNU_SOURCE     // for getline
#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fflush, getline
#include <string.h>     // for strcmp
#include <stdlib.h>     // for free

#include "fble-alloc.h"   // for FbleFree
#include "fble-main.h"    // for FbleMain.
#include "fble-value.h"   // for FbleValue, etc.

#include "char.fble.h"    // for FbleCharValueAccess
#include "int.fble.h"     // for FbleIntValueAccess
#include "string.fble.h"  // for FbleNewStringValue, FbleStringValueAccess

typedef struct {
  FbleIO io;

  FbleValue* in;
  FbleValue* out;
  FbleValue* err;
} Stdio;

static void PrintUsage(FILE* stream);
static void Output(FILE* stream, FbleValue* str);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
int debug();
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
  fprintf(stream, "%s",
      "Usage: fble-stdio [--profile FILE] " FBLE_MAIN_USAGE_SUMMARY " ARGS\n"
      "Run an fble stdio program.\n"
      FBLE_MAIN_USAGE_DETAIL
      "Options:\n"
      "  --profile FILE\n"
      "    Writes a profile of the test run to FILE\n"
      "Example: fble-stdio --profile tests.prof " FBLE_MAIN_USAGE_EXAMPLE "\n"
  );
}

// Output --
//   Output a string to the given output stream.
//
// Inputs:
//   stream - the stream to output to
//   str - the /String%.String@ to write
//
// Side effects:
//   Outputs the string to the stream and flushes the stream.
static void Output(FILE* stream, FbleValue* str)
{
  char* chars = FbleStringValueAccess(str);
  fprintf(stream, "%s", chars);
  fflush(stream);
  FbleFree(chars);
}
// IO --
//   FbleIO.io function for external ports.
//   See the corresponding documentation in fble-value.h.
//
// Ports:
//  in:   Read a line from stdin. Nothing on end of file.
//  out:  Write to stdout.
//  err:  Write to stderr.
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  Stdio* stdio = (Stdio*)io;
  bool change = false;
  if (stdio->out != NULL) {
    Output(stdout, stdio->out);
    FbleReleaseValue(heap, stdio->out);
    stdio->out = NULL;
    change = true;
  }

  if (stdio->err != NULL) {
    Output(stderr, stdio->err);
    FbleReleaseValue(heap, stdio->err);
    stdio->err = NULL;
    change = true;
  }

  if (block && stdio->in == NULL) {
    // Read a line from stdin.
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    if (read < 0) {
      stdio->in = FbleNewEnumValue(heap, 1);
    } else {
      FbleValue* charS = FbleNewStringValue(heap, line);
      stdio->in = FbleNewUnionValue(heap, 0, charS);
      FbleReleaseValue(heap, charS);
    }
    free(line);
    change = true;
  }
  return change;
}

// debug --
//   Placeholder to force linking with some functions that are useful for
//   debugging.
int debug()
{
  intptr_t x = 0;
  x += (intptr_t)(FbleCharValueAccess);
  x += (intptr_t)(FbleIntValueAccess);
  x += (intptr_t)(FbleStringValueAccess);
  return x;
}


// main --
//   The main entry point for fble-tests.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Performs IO based on the execution of FILE. Prints an error message to
//   standard error if an error is encountered.
int main(int argc, char* argv[])
{
  argc--;
  argv++;
  if (argc > 0 && strcmp("--help", *argv) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  FILE* fprofile = NULL;
  if (argc > 1 && strcmp("--profile", *argv) == 0) {
    fprofile = fopen(argv[1], "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", argv[1]);
      return 1;
    }

    argc -= 2;
    argv += 2;
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = FbleMain(heap, profile, FbleCompiledMain, argc, argv);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return 1;
  }

  FbleValue* func = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return 1;
  }

  Stdio io = {
    .io = { .io = &IO, },
    .in = NULL,
    .out = NULL,
    .err = NULL,
  };

  FbleName block_names[5];
  block_names[0].name = FbleNewString("stdin!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("stdout!");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("stdout!!");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[3].name = FbleNewString("stderr!");
  block_names[3].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[4].name = FbleNewString("stderr!!");
  block_names[4].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 5, .xs = block_names };
    block_id = FbleProfileAddBlocks(profile, names);
  };
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);
  FbleFreeName(block_names[2]);
  FbleFreeName(block_names[3]);
  FbleFreeName(block_names[4]);

  FbleValue* fble_stdin = FbleNewInputPortValue(heap, &io.in, block_id);
  FbleValue* fble_stdout = FbleNewOutputPortValue(heap, &io.out, block_id + 1);
  FbleValue* fble_stderr = FbleNewOutputPortValue(heap, &io.err, block_id + 3);
  FbleValue* fble_io = FbleNewStructValue(heap, 3, fble_stdin, fble_stdout, fble_stderr);
  FbleReleaseValue(heap, fble_stdin);
  FbleReleaseValue(heap, fble_stdout);
  FbleReleaseValue(heap, fble_stderr);

  argc -= FBLE_COMPILED_MAIN_ARGS;
  argv += FBLE_COMPILED_MAIN_ARGS;

  FbleValue* argS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argV = FbleNewStringValue(heap, argv[argc - i - 1]);
    FbleValue* argP = FbleNewStructValue(heap, 2, argV, argS);
    FbleReleaseValue(heap, argV);
    FbleReleaseValue(heap, argS);
    argS = FbleNewUnionValue(heap, 0, argP);
    FbleReleaseValue(heap, argP);
  }

  FbleValue* args[2] = { fble_io, argS };
  FbleValue* proc = FbleApply(heap, func, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);
  FbleReleaseValue(heap, args[1]);

  if (proc == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return 1;
  }

  FbleValue* value = FbleExec(heap, &io.io, proc, profile);

  FbleReleaseValue(heap, proc);

  size_t result = 1;
  if (value != NULL) {
    result = FbleUnionValueTag(value);
    FbleReleaseValue(heap, value);
  }

  FbleFreeValueHeap(heap);

  if (fprofile != NULL) {
    FbleProfileReport(fprofile, profile);
  }

  FbleFreeProfile(profile);
  return result;
}
