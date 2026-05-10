/**
 * @file fble-cli.c
 *  A program to run interpreted fble programs with a @l{/Std/Io/Cli%.Main@}
 *  interface.
 */

#include "cli.fble.h"  // for FbleCliMain

/**
 * @func[main] The main entry point for fble-cli.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int]
 *   See documentation for FbleCliMain.
 *
 *  @sideeffects
 *   See documentation for FbleCliMain.
 */
int main(int argc, const char* argv[])
{
  return FbleCliMain(argc, argv, NULL);
}
