// fble-snake
//   A program to run fble programs with a snake interface.

#include <assert.h>     // for assert
#include <curses.h>     // for initscr, cbreak, etc.
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for strcmp
#include <sys/time.h>   // for gettimeofday

#include "fble.h"

#define MAX_ROW 20
#define MAX_COL 60
#define TICK_INTERVAL 200

// Time --
//   Opaque representation of a point in time.
typedef int Time;

// SnakeIO --
//   The FbleIO with additional user data.
typedef struct {
  FbleIO io;
  Time tnext;
} SnakeIO;

static void GetCurrentTime(Time* time);
static void AddTimeMillis(Time* time, int millis);
static int DiffTimeMillis(Time* a, Time* b);

static void PrintUsage(FILE* stream);
static int ReadNum(FbleValue* num);
static bool IO(FbleIO* io, FbleValueArena* arena, bool block);
int main(int argc, char* argv[]);

// GetCurrentTime --
//   Gets the current time.
//
// Inputs:
//   time - Time value to set.
//
// Results: 
//   None.
//
// Side effects:
//   The argument time is set to the current time.
static void GetCurrentTime(Time* time)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *time = 1000 * tv.tv_sec + tv.tv_usec/1000;
}

// AddTimeMillis --
//   Advance the given time by the given number of milliseconds.
//
// Inputs:
//   time - The time to advance.
//   millis - The number of milliseconds to advance the time by.
//
// Results:
//   None.
//
// Side effects:
//   The time argument is set to a time millis milliseconds in the future of
//   its current value.
static void AddTimeMillis(Time* time, int millis)
{
  *time += millis;
}

// DiffTimeMillis --
//   Return the difference in time between the given times.
//
// Inputs:
//   a - the first time argument.
//   b - the second time argument.
//
// Results:
//   The number of milliseconds further into the future b is with respect to
//   a. If b is for an earlier time than a, a negative number of milliseconds
//   will be returned.
//
// Side effects:
//   None.
static int DiffTimeMillis(Time* a, Time* b)
{
  return *a - *b;
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
      "Usage: fble-snake FILE \n"
      "Execute the snake process described by the fble program FILE.\n"
      "Example: fble-snake prgms/fble-snake.fble\n"
  );
}

// ReadNum --
//   Read a number from an FbleValue of type Row@ or Col@
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   None
static int ReadNum(FbleValue* x)
{
  if (FbleUnionValueTag(x) == 0) {
    return 0;
  }
  return 1 + ReadNum(FbleUnionValueAccess(x));
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  SnakeIO* sio = (SnakeIO*)io;
  bool change = false;

  if (io->ports.xs[1] != NULL) {
    FbleValue* draw = io->ports.xs[1];
    FbleValue* pos = FbleStructValueAccess(draw, 0);
    FbleValue* cell = FbleStructValueAccess(draw, 1);
    int row = ReadNum(FbleStructValueAccess(pos, 0));
    int col = ReadNum(FbleStructValueAccess(pos, 1));

    int y = MAX_ROW + 1 - row;
    int x = col + 1;

    char c = '?';
    switch (FbleUnionValueTag(cell)) {
      case 0: c = ' '; break;
      case 1: c = 'S'; break;
      case 2: c = '$'; break;
    }

    mvaddch(y, x, c);
    FbleValueRelease(arena, io->ports.xs[1]);
    io->ports.xs[1] = NULL;
    change = true;
  }

  if (block && io->ports.xs[0] == NULL) {
    // Read the next input from the user.
    Time tnow;
    GetCurrentTime(&tnow);
    int dt = DiffTimeMillis(&sio->tnext, &tnow);
    while (dt > 0) {
      timeout(dt);
      refresh();
      int c = getch();
      switch (c) {
        case 'k': c = 0; break;
        case 'j': c = 1; break;
        case 'h': c = 2; break;
        case 'l': c = 3; break;
        default: c = ERR; break;
      }
      if (c != ERR) {
        FbleValueV args = { .size = 0, .xs = NULL };
        io->ports.xs[0] = FbleNewUnionValue(arena, 0, FbleNewUnionValue(arena, c, FbleNewStructValue(arena, args)));
        return true;
      }
      GetCurrentTime(&tnow);
      dt = DiffTimeMillis(&sio->tnext, &tnow);
    }

    AddTimeMillis(&sio->tnext, TICK_INTERVAL);
    FbleValueV args = { .size = 0, .xs = NULL };
    io->ports.xs[0] = FbleNewUnionValue(arena, 1, FbleNewStructValue(arena, args));
    change = true;
  }

  return change;
}

// main --
//   The main entry point for fbld-snake.
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

  FbleArena* prgm_arena = FbleNewArena();
  FbleProgram* prgm = FbleLoad(prgm_arena, path, NULL);
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

  FbleValue* input = FbleNewInputPortValue(value_arena, 0);
  FbleValue* output = FbleNewOutputPortValue(value_arena, 1);
  FbleValue* main1 = FbleApply(value_arena, func, input, graph);
  FbleValue* proc = FbleApply(value_arena, main1, output, graph);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, main1);
  FbleValueRelease(value_arena, input);
  FbleValueRelease(value_arena, output);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeCallGraph(eval_arena, graph);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  initscr();
  cbreak();
  noecho();
  curs_set(0);

  for (int c = 0; c <= MAX_COL + 2; ++c) {
    mvaddch(0, c, '#');
    mvaddch(MAX_ROW + 2, c, '#');
  }

  for (int r = 1; r <= MAX_ROW + 1; ++r) {
    mvaddch(r, 0, '#');
    mvaddch(r, MAX_COL + 2, '#');
  }
  refresh();

  FbleValue* ports[2] = {NULL, NULL};
  SnakeIO sio = {
    .io = { .io = &IO, .ports = { .size = 2, .xs = ports} },
    .tnext = 0
  };

  GetCurrentTime(&sio.tnext);
  AddTimeMillis(&sio.tnext, TICK_INTERVAL);

  FbleValue* value = FbleExec(value_arena, &sio.io, proc, graph);

  FbleValueRelease(value_arena, proc);
  FbleValueRelease(value_arena, ports[0]);
  FbleValueRelease(value_arena, ports[1]);

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);
  FbleFreeBlockNames(eval_arena, &blocks);
  FbleFreeCallGraph(eval_arena, graph);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(prgm_arena);

  mvaddstr(MAX_ROW + 3, 3, "GAME OVER");
  refresh();
  timeout(-1);
  getch();
  endwin();
  return 0;
}
