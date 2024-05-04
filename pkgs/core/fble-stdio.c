/**
 * @file fble-stdio.c
 *  A program to run interpreted fble programs with a @l{/Core/Stdio%.Stdio@}
 *  interface.
 */

#include "stdio.fble.h"  // for FbleStdioMain

/**
 * @func[main] The main entry point for fble-stdio.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int]
 *   See documentation for FbleStdioMain.
 *
 *  @sideeffects
 *   See documentation for FbleStdioMain.
 */
int main(int argc, const char* argv[])
{
  return FbleStdioMain(argc, argv, NULL);
}
