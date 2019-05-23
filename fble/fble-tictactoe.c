// fble-tictactoe
//   A program to run fble programs with a tictactoe interface.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for strcmp

#include "fble.h"

static void PrintUsage(FILE* stream);
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
      "Usage: fble-tictactoe FILE \n"
      "Execute the tictactoe process described by the fble program FILE.\n"
      "Example: fble-tictactoe progms/tictactoe.fble\n"
  );
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  bool change = false;
  if (io->ports.xs[1] != NULL) {
    FbleValue* output = io->ports.xs[1];
    FbleValue* board = FbleStructValueAccess(output, 0);
    FbleValue* status = FbleStructValueAccess(output, 1);
    printf("  1 2 3\n");
    for (size_t r = 0; r < 3; r++) {
      printf("%c", 'A'+r);
      for (size_t c = 0; c < 3; c++) {
        FbleValue* square = FbleStructValueAccess(board, r * 3 + c);
        switch (FbleUnionValueTag(square)) {
          case 0: printf(" X"); break;
          case 1: printf(" 0"); break;
          case 2: printf(" _"); break;
          default: printf(" ?"); break;
        }
      }
      printf("\n");
    }

    switch (FbleUnionValueTag(status)) {
      case 0:
        switch (FbleUnionValueTag(FbleUnionValueAccess(status))) {
          case 0: printf("Player X move:\n"); break;
          case 1: printf("Player O move:\n"); break;
        }
        break;

      case 1:
        switch (FbleUnionValueTag(FbleUnionValueAccess(status))) {
          case 0: printf("GAME OVER: Player X wins:\n"); break;
          case 1: printf("GAME OVER: Player O wins:\n"); break;
        }
        break;

      case 2:
        printf("GAME OVER: DRAW\n");
        break;
    }

    FbleValueRelease(arena, io->ports.xs[1]);
    io->ports.xs[1] = NULL;
    change = true;
  }

  if (block && io->ports.xs[0] == NULL) {
    // Read the next input from the user.
    int c0 = getchar();
    if (c0 == 'R') {
      FbleValueV args = { .size = 0, .xs = NULL };
      io->ports.xs[0] = FbleNewUnionValue(arena, 2, FbleNewStructValue(arena, args));
    } else if (c0 == 'P') {
      FbleValueV args = { .size = 0, .xs = NULL };
      io->ports.xs[0] = FbleNewUnionValue(arena, 1, FbleNewStructValue(arena, args));
    } else if (c0 >= 'A' && c0 <= 'C') {
      int c1 = getchar();
      if (c1 >= '1' && c1 <= '3') {
        int tag = (c0 - 'A') * 3 + (c1 - '1');
        FbleValueV args = { .size = 0, .xs = NULL };
        io->ports.xs[0] = FbleNewUnionValue(arena, 0,
            FbleNewUnionValue(arena, tag,
              FbleNewStructValue(arena, args)));
      }
    }
    c0 = getchar();
    if (io->ports.xs[0] == NULL) {
      fprintf(stderr, "Invalid Input\n");
      while (c0 != EOF && c0 != '\n') {
        c0 = getchar();
      }
    }
    change = true;
  }
  return change;
}

// main --
//   The main entry point for fblc-tictactoe.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Performs IO based on the execution of FILE.
//   Prints an error message to standard error if an error is encountered.
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

  FbleArena* prgm_arena = FbleNewArena();
  FbleExpr* prgm = FbleParse(prgm_arena, path, NULL);
  if (prgm == NULL) {
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena();
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);

  FbleValue* func = FbleEval(value_arena, prgm);
  if (func == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* input = FbleNewPortValue(value_arena, 0);
  FbleValue* output = FbleNewPortValue(value_arena, 1);
  FbleValue* main1 = FbleApply(value_arena, func, input);
  FbleValue* proc = FbleApply(value_arena, main1, output);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, main1);
  FbleValueRelease(value_arena, input);
  FbleValueRelease(value_arena, output);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }
  assert(proc->tag == FBLE_PROC_VALUE);

  FbleValue* ports[2] = {NULL, NULL};
  FbleIO io = {
    .io = &IO,
    .ports = { .size = 2, .xs = ports}
  };

  FbleValue* value = FbleExec(value_arena, &io, (FbleProcValue*)proc);

  FbleValueRelease(value_arena, proc);
  FbleValueRelease(value_arena, ports[0]);
  FbleValueRelease(value_arena, ports[1]);

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(prgm_arena);

  return 0;
}
