// fblcbe.c --
//   The file implements the main entry point for the fblc binary encoder.

#include <stdio.h>      // for FILE
#include <string.h>     // for strcmp
#include <stdlib.h>     // for malloc, free
#include <unistd.h>     // for STDOUT_FILENO

#include "fblcs.h"

static void PrintUsage(FILE* fout);
static void* MallocAlloc(FblcArena* this, size_t size);
static void MallocFree(FblcArena* this, void* ptr);
int main(int argc, char* argv[]);

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
    return 1;
  }

  const char* filename = argv[1];

  // Simply pass allocations through to malloc. We won't be able to track or
  // free memory that the caller is supposed to track and free, but we don't
  // leak memory in a loop and we assume this is the main entry point of the
  // program, so we should be okay.
  FblcArena arena = { .alloc = &MallocAlloc, .free = &MallocFree };

  FblcsProgram* sprog = FblcsLoadProgram(&arena, filename);
  if (sprog == NULL) {
    return 1;
  }

  FblcWriteProgram(sprog->program, STDOUT_FILENO);
  return 0;
}
