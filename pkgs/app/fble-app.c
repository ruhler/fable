/**
 * @file fble-app.c
 *  A program to run interpreted fble programs with a @l{/App/App%.App@}
 *  interface.
 */

#include "app.h"  // For FbleAppMain

/**
 * @func[main] The main entry point for fble-app.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int]
 *   See documentation for FbleAppMain.
 *
 *  @sideeffects
 *   See documentation for FbleAppMain.
 */
int main(int argc, const char* argv[])
{
  return FbleAppMain(argc, argv, NULL);
}
