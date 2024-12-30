/**
 * @file mem-test.h
 *  Header for FbleMemTestMain.
 */
#ifndef FBLE_TEST_MEM_TEST_H_
#define FBLE_TEST_MEM_TEST_H_

#include <fble/fble-program.h>    // for FblePreloadedModule

/**
 * @func[FbleMemTestMain] A main function for running an fble memory test.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The native module to run, or NULL to determine the module based on
 *   command line options.
 *
 *  @returns[int] 0 on success, 1 on error, 2 on usage error.
 *
 *  @sideeffects
 *   Prints an error to stderr and exits the program in the case of error.
 */
int FbleMemTestMain(int argc, const char** argv, FblePreloadedModule* preloaded);

#endif // FBLE_TEST_MEM_TEST_H_
