// fble-tests
//   A program to run fble programs with a tests interface.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fflush
#include <string.h>     // for strcmp

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
      "Usage: fble-tests FILE DIR \n"
      "Run the fble test process described by the fble program FILE.\n"
      "Example: fble-tests prgms/fble-tests.fble prgms\n"
  );
}

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
  static const char* chars =
    "\n\t !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz"
    "{|}~";
  return chars[FbleUnionValueTag(c)];
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  if (io->ports.xs[0] != NULL) {
    FbleValue* charS = io->ports.xs[0];
    while (FbleUnionValueTag(charS) == 0) {
      FbleValue* charP = FbleUnionValueAccess(charS);
      FbleValue* charV = FbleStructValueAccess(charP, 0);
      charS = FbleStructValueAccess(charP, 1);

      char c = ReadChar(charV);
      printf("%c", c);
    }
    fflush(stdout);

    FbleValueRelease(arena, io->ports.xs[0]);
    io->ports.xs[0] = NULL;
    return true;
  }
  return false;
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

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* path = argv[1];
  const char* include_path = argv[2];

  FbleArena* prgm_arena = FbleNewArena();
  FbleExpr* prgm = FbleParse(prgm_arena, path, include_path);
  if (prgm == NULL) {
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena();
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);
  FbleNameV blocks;
  FbleCallGraph* graph = NULL;

  FbleValue* func = FbleEval(value_arena, prgm, &blocks, &graph);
  if (func == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeCallGraph(eval_arena, graph);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* output = FbleNewPortValue(value_arena, 0);
  FbleValue* proc = FbleApply(value_arena, func, output, graph);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, output);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeCallGraph(eval_arena, graph);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* ports[1] = {NULL};
  FbleIO io = { .io = &IO, .ports = { .size = 1, .xs = ports} };

  FbleValue* value = FbleExec(value_arena, &io, proc, graph);

  FbleValueRelease(value_arena, proc);
  FbleValueRelease(value_arena, ports[0]);

  size_t result = FbleUnionValueTag(value);

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);
  FbleFreeBlockNames(eval_arena, &blocks);
  FbleFreeCallGraph(eval_arena, graph);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(prgm_arena);

  return result;
}
