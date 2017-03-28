// fblc-tictactoe
//   A program to run fblc programs with a tictactoe interface.

#include <assert.h>     // for assert
#include <stdio.h>      // for FILE, printf, fprintf
#include <stdlib.h>     // for malloc, free

#include "fblcs.h"

static void PrintUsage(FILE* stream);
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports);
static void* MallocAlloc(FblcArena* this, size_t size);
static void MallocFree(FblcArena* this, void* ptr);
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
//   io function for external ports with NULL as user data.
//   See the corresponding documentation in fblc.h.
static void IO(void* user, FblcArena* arena, bool block, FblcValue** ports)
{
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
      ports[0] = FblcNewUnion(arena, 3, 2, FblcNewStruct(arena, 0));
    } else if (c0 == 'P') {
      ports[0] = FblcNewUnion(arena, 3, 1, FblcNewStruct(arena, 0));
    } else if (c0 >= 'A' && c0 <= 'C') {
      int c1 = getchar();
      if (c1 >= '1' && c1 <= '3') {
        int tag = (c0 - 'A') * 3 + (c1 - '1');
        ports[0] = FblcNewUnion(arena, 3, 0, FblcNewUnion(arena, 9, tag, FblcNewStruct(arena, 0)));
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

  FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv.xs[decl_id];
  if (proc->_base.tag != FBLC_PROC_DECL) {
    fprintf(stderr, "entry %s is not a process.\n", entry);
    return 1;
  }

  if (proc->argv.size != argc) {
    fprintf(stderr, "expected %zi args, but %i were provided.\n", proc->argv.size, argc);
    return 1;
  }

  FblcValue* args[argc];
  for (size_t i = 0; i < argc; ++i) {
    args[i] = FblcsParseValueFromString(&arena, sprog, proc->argv.xs[i], argv[i]);
  }

  FblcIO io = { .io = &IO, .user = NULL };

  FblcValue* value = FblcExecute(&arena, sprog->program, proc, args, &io);
  assert(value != NULL);
  FblcRelease(&arena, value);
  return 0;
}
