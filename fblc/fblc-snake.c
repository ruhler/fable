// fblc-snake
//   A program to run fblc programs with a snake interface.

#include <curses.h>     // for initscr, cbreak, etc.
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free
#include <sys/time.h>   // for gettimeofday

#include "fblcs.h"

#define MAX_ROW 20
#define MAX_COL 60
#define TICK_INTERVAL 200

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
static int ReadNum(FblcValue* num);
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports);
static void* MallocAlloc(FblcArena* this, size_t size);
static void MallocFree(FblcArena* this, void* ptr);
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
      "Usage: fblc-snake FILE MAIN [ARG...] \n"
      "Evaluate the snake process called MAIN in the environment of the\n"
      "fblc program FILE with the given ARGs.\n"  
      "ARG is a value text representation of the argument value.\n"
      "The number of arguments must match the expected types for the MAIN\n"
      "process.\n"
      "Example: fblc-snake snake.fblc Main\n"
  );
}

// ReadNum --
//   Read a row or column number from an FblcValue.
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   None
static int ReadNum(FblcValue* x)
{
  if (x->tag == 0) {
    return 0;
  }
  return 1 + ReadNum(x->fields[0]);
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;

  if (ports[1] != NULL) {
    FblcValue* draw = ports[1];
    FblcValue* pos = draw->fields[0];
    int row = ReadNum(pos->fields[0]);
    int col = ReadNum(pos->fields[1]);

    int y = MAX_ROW + 1 - row;
    int x = col + 1;

    FblcValue* cell = draw->fields[1];
    char c = '?';
    switch (cell->tag) {
      case 0: c = ' '; break;
      case 1: c = 'S'; break;
      case 2: c = '$'; break;
    }
    mvaddch(y, x, c);
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

// MallocAlloc -- FblcArena alloc function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void* MallocAlloc(FblcArena* this, size_t size)
{
  return malloc(size);
}

// MallocFree -- FblcArena free function implemented using malloc.
// See fblc.h for documentation about FblcArena alloc functions.
static void MallocFree(FblcArena* this, void* ptr)
{
  free(ptr);
}

// main --
//   The main entry point for fblc-tictactoe. Evaluates the MAIN
//   process from the given program with the given arguments.
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

  const char* filename = argv[1];
  const char* entry = argv[2];
  argv += 3;
  argc -= 3;

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena arena = { .alloc = &MallocAlloc, .free = &MallocFree };

  FblcsProgram* sprog = FblcsLoadProgram(&arena, filename);
  if (sprog == NULL) {
    return 1;
  }

  FblcDeclId decl_id = FblcsLookupDecl(sprog, entry);
  if (decl_id == FBLC_NULL_ID) {
    fprintf(stderr, "entry %s not found.\n", entry);
    return 1;
  }

  FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv[decl_id];
  if (proc->_base.tag != FBLC_PROC_DECL) {
    fprintf(stderr, "entry %s is not a process.\n", entry);
    return 1;
  }

  if (proc->argc != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", proc->argc, argc);
    return 1;
  }

  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FblcsParseValueFromString(&arena, sprog, proc->argv[i], argv[i]);
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

  IOUser user;
  GetCurrentTime(&user.tnext);
  AddTimeMillis(&user.tnext, TICK_INTERVAL);
  FblcIO io = { .io = &IO, .user = &user };

  FblcValue* value = FblcExecute(&arena, sprog->program, proc, args, &io);
  FblcRelease(&arena, value);

  mvaddstr(MAX_ROW + 3, 3, "GAME OVER");
  refresh();
  timeout(-1);
  getch();
  endwin();
  return 0;
}

