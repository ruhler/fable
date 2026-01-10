/**
 * @file mem-test.h
 *  Header for FbleMemTestMain.
 */
#ifndef FBLE_TEST_MEM_TEST_H_
#define FBLE_TEST_MEM_TEST_H_

#include <fble/fble-main.h>       // for FbleMainStatus
#include <fble/fble-program.h>    // for FblePreloadedModule

/**
 * @func[FbleMemTestMain] A main function for running an fble memory test.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The native module to run, or NULL to determine the module based on
 *   command line options.
 *
 *  @returns[FbleMainStatus] Status of running the code.
 *
 *  @sideeffects
 *   Prints an error to stderr and exits the program in the case of error.
 */
FbleMainStatus FbleMemTestMain(int argc, const char** argv, FblePreloadedModule* preloaded);

#endif // FBLE_TEST_MEM_TEST_H_
