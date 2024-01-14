#ifndef FBLE_APP_APP_FBLE_H_
#define FBLE_APP_APP_FBLE_H_

#include <fble/fble-codegen.h>

// FbleAppMain -- 
//   A main function for running an App@ program with standard command line
//   options.
//
// Inputs:
//   argc - The number of args.
//   argv - The args.
//   module - The compiled module to use as the App@ program to run, or NULL
//            to determine the module based on command line options.
//
// Result:
//   0 on success, 1 on failure, 2 for usage error.
//
// Side effects:
// * Runs the App@ program, which may interact with keyboard and time events
//   and draw to a display.
// * Writes to a profile if specified by the command line options.
int FbleAppMain(int argc, const char** argv, FbleGeneratedModule* module);

#endif // FBLE_APP_APP_FBLE_H_

