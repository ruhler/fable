// fble-profiles-test.c --
//   A program that runs tests for profiling instrumentation of .fble code.

#include "profiles-test.h"    // for FbleProfilesTestMain

// main --
//   The main entry point for the fble-profiles-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   See documentation for FbleProfilesTestMain.
//
// Side effects:
//  See documentation for FbleProfilesTestMain.
int main(int argc, const char* argv[])
{
  return FbleProfilesTestMain(argc, argv, NULL);
}
