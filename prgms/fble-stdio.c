// fble-stdio
//   A program to run fble programs with a /Stdio%.Stdio@ interface.

#define _GNU_SOURCE     // for getline
#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fflush, getline
#include <string.h>     // for strcmp
#include <stdlib.h>     // for free

#include "fble-main.h"    // for FbleMain.
#include "fble-value.h"   // for FbleValue, etc.

typedef struct {
  FbleIO io;

  FbleValue* in;
  FbleValue* out;
  FbleValue* err;
} Stdio;

static void PrintUsage(FILE* stream);
static char ReadChar(FbleValue* c);
static FbleValue* WriteChar(FbleValueHeap* heap, char c);
static void Output(FILE* stream, FbleValue* str);
static FbleValue* ToFbleString(FbleValueHeap* heap, const char* str);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
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

// gStdLibChars --
//   The list of characters (in tag order) supported by the StdLib.Char@ type.
static const char* gStdLibChars =
    "\n\t !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz"
    "{|}~";

// ReadChar --
//   Read a character from an FbleValue of type StdLib.Char@
//
// Inputs:
//   c - the value of the character.
//
// Results:
//   The value x represented as a c char.
//
// Side effects:
//   None
static char ReadChar(FbleValue* c)
{
  return gStdLibChars[FbleUnionValueTag(c)];
}

// WriteChar --
//   Write a character to an FbleValue of type StdLib.Char@.
//   The character '?' is used for any characters not currently supported by
//   the StdLib.Char@ type.
//
// Inputs:
//   heap - the heap to use for allocations.
//   c - the value of the character to write.
//
// Results:
//   The FbleValue c represented as an StdLib.Char@.
//
// Side effects:
//   Allocates a value that must be freed when no longer required.
static FbleValue* WriteChar(FbleValueHeap* heap, char c)
{
  char* p = strchr(gStdLibChars, c);
  if (p == NULL || c == '\0') {
    assert(c != '?');
    return WriteChar(heap, '?');
  }
  assert(p >= gStdLibChars);
  size_t tag = p - gStdLibChars;
  return FbleNewEnumValue(heap, tag);
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
  FbleValue* charS = str;
  while (FbleUnionValueTag(charS) == 0) {
    FbleValue* charP = FbleUnionValueAccess(charS);
    FbleValue* charV = FbleStructValueAccess(charP, 0);
    charS = FbleStructValueAccess(charP, 1);

    char c = ReadChar(charV);
    fprintf(stream, "%c", c);
  }
  fflush(stream);
}

// ToFbleString --
//   Convert a C string to an Fble /String%.String@.
//
// Inputs:
//   heap - the heap to use for allocations.
//   str - the string to convert.
//
// Results:
//   A newly allocated fble /String%.String@ with the contents of str.
//
// Side effects:
//   Allocates an FbleValue that should be freed with FbleReleaseValue when no
//   longer needed.
static FbleValue* ToFbleString(FbleValueHeap* heap, const char* str)
{
  size_t length = strlen(str);
  FbleValue* charS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < length; ++i) {
    FbleValue* charV = WriteChar(heap, str[length - i - 1]);
    FbleValue* charP = FbleNewStructValue(heap, 2, charV, charS);
    FbleReleaseValue(heap, charV);
    FbleReleaseValue(heap, charS);
    charS = FbleNewUnionValue(heap, 0, charP);
    FbleReleaseValue(heap, charP);
  }
  return charS;
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
      FbleValue* charS = ToFbleString(heap, line);
      stdio->in = FbleNewUnionValue(heap, 0, charS);
      FbleReleaseValue(heap, charS);
    }
    free(line);
    change = true;
  }
  return change;
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
    FbleValue* argV = ToFbleString(heap, argv[argc - i - 1]);
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
