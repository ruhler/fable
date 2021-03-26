// fble-compiled-stdio
//   A program to run a compiled fble program with a stdio interface:
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
//   The entry point to the compiled fble program should be called
//   FbleStdioMain. 

#define _GNU_SOURCE     // for getline
#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fflush, getline
#include <stdlib.h>     // for free
#include <string.h>     // for strchr

#include "fble.h"

FbleValue* FbleStdioMain(FbleValueHeap* heap);

typedef struct {
  FbleIO io;

  FbleValue* input;
  FbleValue* output;
} Stdio;

static char ReadChar(FbleValue* c);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
int main(int argc, char* argv[]);

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
//   io function for external ports.
//   See the corresponding documentation in fble.h.
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
        FbleValue* xs[] = { charV, charS };
        FbleValueV args = { .size = 2, .xs = xs };
        FbleValue* charP = FbleNewStructValue(heap, args);
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
  FbleArena* arena = FbleNewArena();
  FbleValueHeap* heap = FbleNewValueHeap(arena);
  FbleValue* compiled = FbleStdioMain(heap);
  FbleValue* func = FbleEval(heap, compiled, NULL);
  FbleReleaseValue(heap, compiled);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeArena(arena);
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
  FbleValue* proc = FbleApply(heap, func, args, NULL);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);
  FbleReleaseValue(heap, args[1]);

  if (proc == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeArena(arena);
    return 1;
  }

  FbleValue* value = FbleExec(heap, &io.io, proc, NULL);

  FbleReleaseValue(heap, proc);
  FbleReleaseValue(heap, io.input);
  FbleReleaseValue(heap, io.output);

  size_t result = FbleUnionValueTag(value);

  FbleReleaseValue(heap, value);
  FbleFreeValueHeap(heap);
  FbleFreeArena(arena);
  return result;
}
