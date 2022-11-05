#ifndef FBLE_TEST_PROFILES_TEST_H_
#define FBLE_TEST_PROFILES_TEST_H_

#include <fble/fble-link.h>
#include <fble/fble-profile.h>
#include <fble/fble-value.h>

// FbleProfilesTestMain -- 
//   A main function for running the profiles test.
//
// Inputs:
//   argc - The number of args.
//   argv - The args.
//   module - The compiled module to use as the Stdio@ program to run, or NULL
//            to determine the module based on command line options.
//
// Result:
//   0 for pass, 1 for fail, 2 for usage error.
//
// Side effects:
// * Outputs a profile to stdout for debug purposes.
// * Aborts in case of assertion failure.
int FbleProfilesTestMain(int argc, const char** argv, FbleCompiledModuleFunction* module);


#endif // FBLE_TEST_PROFILES_TEST_H_
