// fble-app
//   A program to run interpreted fble programs with a /App/App%.App@ interface.

#include "app.fble.h"  // For FbleAppMain

// main --
//   The main entry point for fble-app.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   See documentation for FbleAppMain.
//
// Side effects:
//   See documentation for FbleAppMain.
int main(int argc, const char* argv[])
{
  return FbleAppMain(argc, argv, NULL);
}
