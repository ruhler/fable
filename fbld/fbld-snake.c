// fbld-snake
//   A program to run fbld programs with a snake interface.

#include <assert.h>     // for assert
#include <curses.h>     // for initscr, cbreak, etc.
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for strcmp
#include <sys/time.h>   // for gettimeofday

#include "fblc.h"
#include "fbld.h"

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
static FblcValue* ParseValueFromString(FblcArena* arena, FbldProgram* prgm, const char* string);
static int ReadUBNat(FblcValue* num);
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports);
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
      "Usage: fbld-snake FILE MAIN [ARG...] \n"
      "Execute the snake process called MAIN in the environment of the\n"
      "fbld program FILE with the given ARGs.\n"
      "ARG is a value text representation of the argument value.\n"
      "The number of arguments must match the expected types for the MAIN\n"
      "process.\n"
      "Example: fbld-snake prgms/Snake.fbld Main\n"
  );
}

// ParseValueFromString --
//   Parse an fblc value from an fbld string description of the value.
//
// Inputs:
//   arena - Arena to use for allocating the parsed value.
//   modulev - The program environment.
//   string - The string to parse the value from.
//
// Results:
//   The parsed value, or NULL if there is a problem parsing the value.
//
// Side effects:
//   Reports a message to stderr if the value cannot be parsed.
static FblcValue* ParseValueFromString(FblcArena* arena, FbldProgram* prgm, const char* string)
{
  FbldValue* fbld_value = FbldParseValueFromString(arena, string);
  if (fbld_value == NULL) {
    fprintf(stderr, "Unable to parse fbld value '%s'\n", string);
    return NULL;
  }

  if (!FbldCheckValue(arena, prgm, fbld_value)) {
    fprintf(stderr, "Invalid value '%s'\n", string);
    return NULL;
  }
  return FbldCompileValue(arena, prgm, fbld_value);
}

// ReadUBNat --
//   Read a number from an FblcValue of type Nat@UBNat
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   None
static int ReadUBNat(FblcValue* x)
{
  switch (x->tag) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * ReadUBNat(x->fields[0]);
    case 3: return 2 * ReadUBNat(x->fields[0]) + 1;
    default: assert(false && "Invalid UBNat tag"); abort();
  }
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;

  if (ports[1] != NULL) {
    FblcValue* drawS = ports[1];
    while (drawS->tag) {
      FblcValue* drawP = drawS->fields[0];
      FblcValue* draw = drawP->fields[0];
      drawS = drawP->fields[1];

      int ux = ReadUBNat(draw->fields[0]);
      int uy = ReadUBNat(draw->fields[1]);
      int w = ReadUBNat(draw->fields[2]);
      int h = ReadUBNat(draw->fields[3]);

      FblcValue* color = draw->fields[4];
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

    FblcRelease(arena, ports[1]);
    ports[1] = NULL;
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
        ports[0] = FblcNewUnion(arena, 2, 0, FblcNewUnion(arena, 4, c, FblcNewStruct(arena, 0)));
        return;
      }
      GetCurrentTime(&tnow);
      dt = DiffTimeMillis(&io_user->tnext, &tnow);
    }

    AddTimeMillis(&io_user->tnext, TICK_INTERVAL);
    ports[0] = FblcNewUnion(arena, 2, 1, FblcNewStruct(arena, 0));
  }
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
//   Performs IO based on the execution of the MAIN process on the
//   given arguments, or prints an error message to standard error if an error
//   is encountered.
//
//   This function leaks memory under the assumption it is called as the main
//   entry point of the program and the OS will free the memory resources used
//   immediately after the function returns.
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

  if (argc <= 2) {
    fprintf(stderr, "no main entry point provided.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* path = argv[1];
  const char* entry = argv[2];
  argv += 3;
  argc -= 3;

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena* arena = &FblcMallocArena;

  FbldQRef* qentry = FbldParseQRefFromString(arena, entry);
  if (qentry == NULL) {
    fprintf(stderr, "failed to parse entry\n");
    return 1;
  }

  FbldAccessLocV accessv;
  FblcVectorInit(arena, accessv);
  FbldLoaded* loaded = FbldLoadCompileProgram(arena, &accessv, path, qentry);
  if (loaded == NULL) {
    return 1;
  }

  if (loaded->proc_d->argv->size != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", loaded->proc_d->argv->size, argc);
    return 1;
  }

  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = ParseValueFromString(arena, loaded->prog, argv[i]);
  }
  
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
  FblcIO io = { .io = &IO, .user = &user };
  FblcInstr instr = { .on_undefined_access = NULL };

  FblcValue* value = FblcExecute(arena, &instr, loaded->proc_c, args, &io);
  FblcRelease(arena, value);

  mvaddstr(MAX_ROW + 3, 3, "GAME OVER");
  refresh();
  timeout(-1);
  getch();
  endwin();
  return 0;
}

