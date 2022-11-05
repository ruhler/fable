// fble-test.c --
//   Main entry point for interpreter based fble-test program.

#include "test.h"    // for FbleTestMain

// main --
//   The main entry point for the fble-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   See documentation for FbleTestMain.
//
// Side effects:
//  See documentation for FbleTestMain.
int main(int argc, const char* argv[])
{
  return FbleTestMain(argc, argv, NULL);
}
