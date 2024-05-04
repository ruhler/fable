/**
 * @file fble-mem-test.c
 *  Main entry point for interpreter based fble-mem-test program.
 */

#include "mem-test.h"    // for FbleMemTestMain

/**
 * @func[main] The main entry point for the fble-mem-test program.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int]
 *   See documentation for FbleMemTestMain.
 *
 *  @sideeffects
 *   See documentation for FbleMemTestMain.
 */
int main(int argc, const char* argv[])
{
  return FbleMemTestMain(argc, argv, NULL);
}
