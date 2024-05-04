/**
 * @file fble-test.c
 *  Main entry point for interpreter based fble-test program.
 */

#include "test.h"    // for FbleTestMain

/**
 * @func[main] The main entry point for the fble-test program.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char*][argv] The command line arguments.
 *
 *  @returns[int]
 *   See documentation for FbleTestMain.
 *
 *  @sideeffects
 *   See documentation for FbleTestMain.
 */
int main(int argc, const char* argv[])
{
  return FbleTestMain(argc, argv, NULL);
}
