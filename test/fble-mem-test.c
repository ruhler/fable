// fble-mem-test.c --
//   Main entry point for interpreter based fble-mem-test program.

#include "mem-test.h"    // for FbleMemTestMain

// main --
//   The main entry point for the fble-mem-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   See documentation for FbleMemTestMain.
//
// Side effects:
//  See documentation for FbleMemTestMain.
int main(int argc, const char* argv[])
{
  return FbleMemTestMain(argc, argv, NULL);
}
