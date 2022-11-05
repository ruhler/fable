#ifndef FBLE_TEST_TEST_H_
#define FBLE_TEST_TEST_H_

#include "fble-link.h"
#include "fble-profile.h"
#include "fble-value.h"

// FbleTestMain -- 
//   A main function for running a basic fble test.
//
// Inputs:
//   argc - The number of args.
//   argv - The args.
//   module - The compiled module to run, or NULL to determine the module
//            based on command line options.
//
// Result:
//   0 on success, 1 on error, 2 on usage error.
//
// Side effects:
// * Prints an error to stderr and exits the program in the case of error.
int FbleTestMain(int argc, const char** argv, FbleCompiledModuleFunction* module);


#endif // FBLE_TEST_TEST_H_
