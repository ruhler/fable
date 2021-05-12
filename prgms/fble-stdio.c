// fble-stdio
//   A program to run fble programs with a stdio interface:
//
//     (Maybe@<Str@>-, Str@+) { Bool@; }
//
//   The first argument is stdin: a you can read lines of input from stdin. It
//   returns Nothing on end of file.
//
//   The second argument sends strings to stdout.
//
//   The return value should be a union type. The tag of the union type is the
//   return code of the program. For example, True causes an exit code of 0
//   and False causes an exit code of 1.
//

#define _GNU_SOURCE     // for getline
#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fflush, getline
#include <string.h>     // for strcmp
#include <stdlib.h>     // for free

#include "fble-link.h"    // for FbleLinkFromSource.
#include "fble-value.h"   // for FbleValue, etc.

typedef struct {
  FbleIO io;

  FbleValue* input;
  FbleValue* output;
} Stdio;

static void PrintUsage(FILE* stream);
static char ReadChar(FbleValue* c);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
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
      "Usage: fble-stdio [--profile FILE] FILE DIR \n"
      "Run the fble stdio program described by the fble program FILE.\n"
      "Options:\n"
      "  --profile FILE\n"
      "    Writes a profile of the test run to FILE\n"
      "Example: fble-stdio --profile tests.prof prgms/Fble/Tests.fble prgms\n"
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

// IO --
//   FbleIO.io function for external ports.
//   See the corresponding documentation in fble-value.h.
//
// Ports:
//  input: Maybe@<Str@>-  Read a line from stdin. Nothing on end of file.
//  output: Str@+          Output a line to stdout.
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  Stdio* stdio = (Stdio*)io;
  bool change = false;
  if (stdio->output != NULL) {
    // Output a string to stdout.
    FbleValue* charS = stdio->output;
    while (FbleUnionValueTag(charS) == 0) {
      FbleValue* charP = FbleUnionValueAccess(charS);
      FbleValue* charV = FbleStructValueAccess(charP, 0);
      charS = FbleStructValueAccess(charP, 1);

      char c = ReadChar(charV);
      printf("%c", c);
    }
    fflush(stdout);

    FbleReleaseValue(heap, stdio->output);
    stdio->output = NULL;
    change = true;
  }

  if (block && stdio->input == NULL) {
    // Read a line from stdin.
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    if (read < 0) {
      stdio->input = FbleNewEnumValue(heap, 1);
    } else {
      FbleValue* charS = FbleNewEnumValue(heap, 1);
      for (size_t i = 0; i < read; ++i) {
        FbleValue* charV = WriteChar(heap, line[read - i - 1]);
        FbleValue* charP = FbleNewStructValue(heap, 2, charV, charS);
        FbleReleaseValue(heap, charV);
        FbleReleaseValue(heap, charS);
        charS = FbleNewUnionValue(heap, 0, charP);
        FbleReleaseValue(heap, charP);
      }
      stdio->input = FbleNewUnionValue(heap, 0, charS);
      FbleReleaseValue(heap, charS);
    }
    free(line);
    change = true;
  }
  return change;
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
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  FILE* fprofile = NULL;
  if (argc > 2 && strcmp("--profile", argv[1]) == 0) {
    fprofile = fopen(argv[2], "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", argv[2]);
      return 1;
    }

    argc -= 2;
    argv += 2;
  }

  const char* path = argc <= 1 ? NULL : argv[1];
  const char* include_path = argc <= 2 ? NULL : argv[2];

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = Main(heap, path, include_path, profile);
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
    .input = NULL,
    .output = NULL,
  };

  FbleValue* args[2] = {
    FbleNewInputPortValue(heap, &io.input),
    FbleNewOutputPortValue(heap, &io.output)
  };
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
  FbleReleaseValue(heap, io.input);
  FbleReleaseValue(heap, io.output);

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
