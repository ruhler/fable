// fble-md5
//   A program to compute the md5 sum of a file.

#include "Md5/md5.fble.h"   // for FbleMd5Main


// main --
//   The main entry point for fble-md5.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   See documentation for FbleMd5Main.
//
// Side effects:
//   See documentation for FbleMd5Main.
int main(int argc, const char* argv[])
{
  return FbleMd5Main(argc, argv, NULL);
}
