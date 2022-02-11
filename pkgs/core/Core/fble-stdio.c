// fble-stdio
//   A program to run interpreted fble programs with a /Stdio%.Stdio@ interface.

#include "Core/stdio.fble.h"  // for FbleStdioMain

// main --
//   The main entry point for fble-stdio.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   See documentation for FbleStdioMain.
//
// Side effects:
//   See documentation for FbleStdioMain.
int main(int argc, const char* argv[])
{
  return FbleStdioMain(argc, argv, NULL);
}
