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

#include "fble.h"

static void PrintUsage(FILE* stream);
static char ReadChar(FbleValue* c);
static bool IO(FbleIO* io, FbleValueArena* arena, bool block);
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
//   arena - the arena to use for allocations.
//   c - the value of the character to write.
//
// Results:
//   The FbleValue c represented as an StdLib.Char@.
//
// Side effects:
//   Allocates a value that must be freed when no longer required.
static FbleValue* WriteChar(FbleValueArena* arena, char c)
{
  char* p = strchr(gStdLibChars, c);
  if (p == NULL || c == '\0') {
    assert(c != '?');
    return WriteChar(arena, '?');
  }
  assert(p >= gStdLibChars);
  size_t tag = p - gStdLibChars;
  FbleValueV args = { .size = 0, .xs = NULL };
  return FbleNewUnionValue(arena, tag, FbleNewStructValue(arena, args));
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
//
// Ports:
//  0: Maybe@<Str@>-  Read a line from stdin. Nothing on end of file.
//  1: Str@+          Output a line to stdout.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  bool change = false;
  if (io->ports.xs[1] != NULL) {
    // Output a string to stdout.
    FbleValue* charS = io->ports.xs[1];
    while (FbleUnionValueTag(charS) == 0) {
      FbleValue* charP = FbleUnionValueAccess(charS);
      FbleValue* charV = FbleStructValueAccess(charP, 0);
      charS = FbleStructValueAccess(charP, 1);

      char c = ReadChar(charV);
      printf("%c", c);
    }
    fflush(stdout);

    FbleValueRelease(arena, io->ports.xs[1]);
    io->ports.xs[1] = NULL;
    change = true;
  }

  if (block && io->ports.xs[0] == NULL) {
    // Read a line from stdin.
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    FbleValueV emptyArgs = { .size = 0, .xs = NULL };
    FbleValue* unit = FbleNewStructValue(arena, emptyArgs);
    if (read < 0) {
      io->ports.xs[0] = FbleNewUnionValue(arena, 1, unit);
    } else {
      FbleValue* charS = FbleNewUnionValue(arena, 1, unit);
      for (size_t i = 0; i < read; ++i) {
        FbleValue* charV = WriteChar(arena, line[read - i - 1]);
        FbleValue* xs[] = { charV, charS };
        FbleValueV args = { .size = 2, .xs = xs };
        FbleValue* charP = FbleNewStructValue(arena, args);
        charS = FbleNewUnionValue(arena, 0, charP);
      }
      io->ports.xs[0] = FbleNewUnionValue(arena, 0, charS);
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

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* path = argv[1];
  const char* include_path = argv[2];

  FbleArena* prgm_arena = FbleNewArena();
  FbleProgram* prgm = FbleLoad(prgm_arena, path, include_path);
  if (prgm == NULL) {
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena();
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);
  FbleNameV blocks;
  FbleProfile* profile = NULL;

  FbleValue* func = FbleEval(value_arena, prgm, &blocks, &profile);
  if (func == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeProfile(eval_arena, profile);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* args[2];
  args[0] = FbleNewInputPortValue(value_arena, 0);
  args[1] = FbleNewOutputPortValue(value_arena, 1);
  FbleValueV argsv = { .xs = args, .size = 2 };
  FbleValue* proc = FbleApply(value_arena, func, argsv, profile);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, args[0]);
  FbleValueRelease(value_arena, args[1]);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeProfile(eval_arena, profile);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* ports[2] = {NULL, NULL};
  FbleIO io = { .io = &IO, .ports = { .size = 2, .xs = ports} };

  FbleValue* value = FbleExec(value_arena, &io, proc, profile);

  FbleValueRelease(value_arena, proc);
  FbleValueRelease(value_arena, ports[0]);
  FbleValueRelease(value_arena, ports[1]);

  size_t result = FbleUnionValueTag(value);

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);

  if (fprofile != NULL) {
    FbleProfileReport(fprofile, &blocks, profile);
  }

  FbleFreeBlockNames(eval_arena, &blocks);
  FbleFreeProfile(eval_arena, profile);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(prgm_arena);

  return result;
}
