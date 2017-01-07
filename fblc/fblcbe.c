// fblcbe.c --
//
//   The file implements the main entry point for the fblc binary encoder.

#include <stdio.h>      // for FILE
#include <string.h>     // for strcmp
#include <unistd.h>     // for STDOUT_FILENO

#include "fblct.h"
#include "gc.h"

static void PrintUsage(FILE* fout);

// PrintUsage --
//   
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
      "Usage: fblcbe FILE\n"
      "Encode the fblc program FILE in binary format.\n"
  );
}

// main --
//
//   The main entry point for the fblc binary encoder.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints the binary encoding of the given program to standard out, or
//   prints an error message to standard error if an error is encountered.

int main(int argc, char* argv[])
{
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    return 1;
  }

  const char* filename = argv[1];

  GcInit();
  FblcArena* gc_arena = CreateGcArena();
  FblcArena* bulk_arena = CreateBulkFreeArena(gc_arena);
  SProgram* sprog = ParseProgram(bulk_arena, filename);
  if (sprog == NULL) {
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  if (!CheckProgram(sprog)) {
    fprintf(stderr, "input FILE is not a well formed  program.\n");
    FreeBulkFreeArena(bulk_arena);
    FreeGcArena(gc_arena);
    GcFinish();
    return 1;
  }

  FblcArena* bulk_arena_2 = CreateBulkFreeArena(gc_arena);
  FblcWriteProgram(sprog->program, STDOUT_FILENO);

  FreeBulkFreeArena(bulk_arena_2);
  FreeBulkFreeArena(bulk_arena);
  FreeGcArena(gc_arena);
  GcFinish();
  return 0;
}
