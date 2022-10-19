#ifndef FBLE_MD5_MD5_FBLE_H_
#define FBLE_MD5_MD5_FBLE_H_

#include "fble-link.h"

// FbleMd5Main -- 
//   A main function for running an Md5 program with standard command line
//   options.
//
// Inputs:
//   argc - The number of args.
//   argv - The args.
//   module - The compiled module to use as the Md5 program to run, or NULL
//            to determine the module based on command line options.
//
// Result:
//   0 for success, 1 for failure, 2 for usage error.
//
// Side effects:
// * Runs the Md5 program.
// * Writes to a profile if specified by the command line options.
// * Reads files and prints messages to stdout and stderr.
int FbleMd5Main(int argc, const char** argv, FbleCompiledModuleFunction* module);

#endif // FBLE_MD5_MD5_FBLE_H_

