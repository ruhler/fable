// fble-ref-test.c --
//   This file implements the main entry point for the fble-ref-test program.

#include <stdio.h>    // for FILE, fprintf, stderr

#include "fble.h"

// main --
//   The main entry point for the fble-ref-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints an error to stderr and exits the program in the case of error.
int main(int argc, char* argv[])
{
  FbleArena* arena = FbleNewArena(NULL);
  FbleDeleteArena(arena);
  return 0;
}
