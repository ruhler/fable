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

//static char* DRAW_COLOR_CHARS = " RGYBMCW";
static char* DRAW_COLOR_CHARS = "        ";
static short DRAW_COLORS[] = {
  COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
  COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
};
static short DRAW_COLOR_PAIRS[8];

// Time --
//   Opaque representation of a point in time.
typedef int Time;

// IOUser --
//   User data for FblcIO.
typedef struct {
  Time tnext;
} IOUser;

static void GetCurrentTime(Time* time);
static void AddTimeMillis(Time* time, int millis);
static int DiffTimeMillis(Time* a, Time* b);

static void PrintUsage(FILE* stream);
static int ReadNum(FbleValue* num);
static bool IO(void* user, FbleValueArena* arena, bool block, FbleValue** ports);
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
      "Example: fble-snake prgms/snake.fble\n"
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
  assert(x->tag == FBLE_UNION_VALUE);
  FbleUnionValue* u = (FbleUnionValue*)x;
  if (u->tag == 0) {
    return 0;
  }
  return 1 + ReadNum(u->arg);
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fble.h.
static bool IO(void* user, FbleValueArena* arena, bool block, FbleValue** ports)
{
  bool change = false;
  IOUser* io_user = (IOUser*)user;

  if (ports[1] != NULL) {
    FbleUnionValue* drawS = (FbleUnionValue*)ports[1];
    assert(drawS->_base.tag == FBLE_UNION_VALUE);
    while (drawS->tag) {
      FbleStructValue* drawP = (FbleStructValue*)drawS->arg;
      assert(drawP->_base.tag == FBLE_STRUCT_VALUE);

      FbleStructValue* draw = (FbleStructValue*)drawP->fields.xs[0];
      assert(draw->_base.tag == FBLE_STRUCT_VALUE);

      drawS = (FbleUnionValue*)drawP->fields.xs[1];
      assert(drawS->_base.tag == FBLE_UNION_VALUE);

      int ux = ReadNum(draw->fields.xs[0]);
      int uy = ReadNum(draw->fields.xs[1]);
      int w = ReadNum(draw->fields.xs[2]);
      int h = ReadNum(draw->fields.xs[3]);

      FbleUnionValue* color = (FbleUnionValue*)draw->fields.xs[4];
      assert(color->_base.tag == FBLE_UNION_VALUE);

      char c = DRAW_COLOR_CHARS[color->tag];
      color_set(DRAW_COLOR_PAIRS[color->tag], NULL);
      for (int i = ux; i < ux + w; ++i) {
        for (int j = uy; j < uy + h; ++j) {
          int x = i + 1;
          int y = MAX_ROW + 1 - j;
          mvaddch(y, x, c);
        }
      }
      color_set(0, NULL);
    }

    FbleValueRelease(arena, ports[1]);
    ports[1] = NULL;
    change = true;
  }

  if (block && ports[0] == NULL) {
    // Read the next input from the user.
    Time tnow;
    GetCurrentTime(&tnow);
    int dt = DiffTimeMillis(&io_user->tnext, &tnow);
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
        ports[0] = FbleNewUnionValue(arena, 0, FbleNewUnionValue(arena, c, FbleNewStructValue(arena, &args)));
        return true;
      }
      GetCurrentTime(&tnow);
      dt = DiffTimeMillis(&io_user->tnext, &tnow);
    }

    AddTimeMillis(&io_user->tnext, TICK_INTERVAL);
    FbleValueV args = { .size = 0, .xs = NULL };
    ports[0] = FbleNewUnionValue(arena, 1, FbleNewStructValue(arena, &args));
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

  FbleArena* arena = FbleNewArena(NULL);
  FbleExpr* prgm = FbleParse(arena, path);
  if (prgm == NULL) {
    FbleDeleteArena(arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena(arena);
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);

  FbleValue* func = FbleEval(value_arena, prgm);
  if (func == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(arena);
    return 1;
  }
  assert(func->tag == FBLE_FUNC_VALUE);

  FbleValueV args;
  FbleVectorInit(arena, args);
  FbleVectorAppend(arena, args, FbleNewPortValue(value_arena, 0));
  FbleVectorAppend(arena, args, FbleNewPortValue(value_arena, 1));
  FbleValue* proc = FbleApply(value_arena, (FbleFuncValue*)func, &args);
  FbleValueRelease(value_arena, args.xs[0]);
  FbleValueRelease(value_arena, args.xs[1]);
  FbleFree(arena, args.xs);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(arena);
    return 1;
  }
  assert(proc->tag == FBLE_PROC_VALUE);

  initscr();
  cbreak();
  noecho();
  curs_set(0);

  // Set up the color pairs.
  start_color();
  assert(COLOR_PAIRS > 8);
  for (int i = 0; i < 8; ++i) {
    init_pair(i+1, COLOR_WHITE, DRAW_COLORS[i]);
    DRAW_COLOR_PAIRS[i] = i+1;
  }

  color_set(8, NULL);
  for (int c = 0; c <= MAX_COL + 2; ++c) {
    mvaddch(0, c, ' ');
    mvaddch(MAX_ROW + 2, c, ' ');
  }

  for (int r = 1; r <= MAX_ROW + 1; ++r) {
    mvaddch(r, 0, ' ');
    mvaddch(r, MAX_COL + 2, ' ');
  }
  color_set(0, NULL);
  refresh();

  IOUser user;
  GetCurrentTime(&user.tnext);
  AddTimeMillis(&user.tnext, TICK_INTERVAL);
  FbleIO io = { .io = &IO, .portc = 2, .user = &user };

  FbleValue* value = FbleExec(value_arena, (FbleProcValue*)proc, &io);

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(arena);

  mvaddstr(MAX_ROW + 3, 3, "GAME OVER");
  refresh();
  timeout(-1);
  getch();
  endwin();
  return 0;
}
