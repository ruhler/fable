/**
 * @file fble-profiles-test.c
 *  A program that runs tests for profiling instrumentation of .fble code.
 */

#include "profiles-test.h"    // for FbleProfilesTestMain

/**
 * @func[main] The main entry point for the fble-profiles-test program.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int]
 *   See documentation for FbleProfilesTestMain.
 *
 *  @sideeffects
 *   See documentation for FbleProfilesTestMain.
 */
int main(int argc, const char* argv[])
{
  return FbleProfilesTestMain(argc, argv, NULL);
}
