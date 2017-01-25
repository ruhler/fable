// fblc-tictactoe
//   A program to run fblc programs with a tictactoe interface.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf

#include "fblcs.h"
#include "gc.h"

// IOUser --
//   User data for FblcIO.
typedef struct {
  FblcsProgram* sprog;
  FblcProcDecl* proc;
} IOUser;

static void PrintUsage(FILE* stream);
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports);
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
      "Usage: fblc-tictactoe FILE MAIN [ARG...] \n"
      "Evaluate the tictactoe process called MAIN in the environment of the\n"
      "fblc program FILE with the given ARGs.\n"  
      "ARG is a value text representation of the argument value.\n"
      "The number of arguments must match the expected types for the MAIN\n"
      "process.\n"
      "Example: fblc-tictactoe tictactoe.fblc NewGame \n"
  );
}

// IO --
//   io function for external ports with IOUser as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
  IOUser* io_user = (IOUser*)user;

  if (ports[1] != NULL) {
    FblcValue** squares = ports[1]->fields[0]->fields;
    printf("  1 2 3\n");
    for (int r = 0; r < 3; r++) {
      printf("%c", 'A'+r);
      for (int c = 0; c < 3; c++) {
        FblcValue* square = *squares++;
        switch (square->tag) {
          case 0: printf(" X"); break;
          case 1: printf(" 0"); break;
          case 2: printf(" _"); break;
          default: printf(" ?"); break;
        }
      }
      printf("\n");
    }

    FblcValue* status = ports[1]->fields[1];
    switch (status->tag) {
      case 0:
        switch (status->fields[0]->tag) {
          case 0: printf("Player X move:\n"); break;
          case 1: printf("Player O move:\n"); break;
        }
        break;

      case 1:
        switch (status->fields[0]->tag) {
          case 0: printf("GAME OVER: Player X wins:\n"); break;
          case 1: printf("GAME OVER: Player O wins:\n"); break;
        }
        break;

      case 2:
        printf("GAME OVER: DRAW\n");
        break;
    }

    FblcRelease(arena, ports[1]);
    ports[1] = NULL;
  }

  if (block && ports[0] == NULL) {
    // Read the next input from the user.
    int c0 = getchar();
    if (c0 == 'R') {
      const char* s = "Input:reset(Unit())";
      ports[0] = FblcsParseValueFromString(arena, io_user->sprog, io_user->proc->portv[0].type, s);
    } else if (c0 == 'P') {
      const char* s = "Input:computer(Unit())";
      ports[0] = FblcsParseValueFromString(arena, io_user->sprog, io_user->proc->portv[0].type, s);
    } else if (c0 >= 'A' && c0 <= 'C') {
      int c1 = getchar();
      if (c1 >= '1' && c1 <= '3') {
        const char* s = "Input:position(Position:UL(Unit()))";
        ports[0] = FblcsParseValueFromString(arena, io_user->sprog, io_user->proc->portv[0].type, s);
        ports[0]->fields[0]->tag = (c0 - 'A') * 3 + (c1 - '1');
      }
    }
    c0 = getchar();
    if (ports[0] == NULL) {
      fprintf(stderr, "Invalid Input\n");
      while (c0 != EOF && c0 != '\n') {
        c0 = getchar();
      }
    }
  }
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

  GcInit();
  FblcArena* gc_arena = CreateGcArena();
  FblcArena* bulk_arena = CreateBulkFreeArena(gc_arena);
  FblcsProgram* sprog = FblcsLoadProgram(bulk_arena, filename);
  if (sprog == NULL) {
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcDeclId decl_id = FblcsLookupDecl(sprog, entry);
  if (decl_id == FBLC_NULL_ID) {
    fprintf(stderr, "entry %s not found.\n", entry);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv[decl_id];
  if (proc->tag != FBLC_PROC_DECL) {
    fprintf(stderr, "entry %s is not a process.\n", entry);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  if (proc->argc != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", proc->argc, argc);
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FblcsParseValueFromString(gc_arena, sprog, proc->argv[i], argv[i]);
  }

  IOUser user = { .sprog = sprog, .proc = proc };
  FblcIO io = { .io = &IO, .user = &user };

  FblcValue* value = FblcExecute(gc_arena, sprog->program, proc, args, &io);
  assert(value != NULL);
  FblcRelease(gc_arena, value);

  FreeBulkFreeArena(bulk_arena);
  FreeGcArena(gc_arena);
  GcFinish();
  return 0;
}
