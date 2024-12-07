/**
 * @file profiles-test.h
 *  Header for profiles test.
 */
#ifndef FBLE_TEST_PROFILES_TEST_H_
#define FBLE_TEST_PROFILES_TEST_H_

#include <fble/fble-program.h>   // for FbleNativeModule

/**
 * @func[FbleProfilesTestMain A main function for running the profiles test.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FbleNativeModule][module]
 *   The native module to use as the Stdio@ program to run, or NULL to
 *   determine the module based on command line options.
 *
 *  @returns[int]
 *   0 for pass, 1 for fail, 2 for usage error.
 *
 *  @sideeffects
 *   @i Outputs a profile to stdout for debug purposes.
 *   @i Aborts in case of assertion failure.
 */
int FbleProfilesTestMain(int argc, const char** argv, FbleNativeModule* module);

#endif // FBLE_TEST_PROFILES_TEST_H_
