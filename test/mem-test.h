#ifndef FBLE_TEST_MEM_TEST_H_
#define FBLE_TEST_MEM_TEST_H_

#include <fble/fble-generate.h>    // for FbleGeneratedModule

/**
 * @func[FbleMemTestMain] A main function for running an fble memory test.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FbleGeneratedModule*][module]
 *   The compiled module to run, or NULL to determine the module based on
 *   command line options.
 *
 *  @returns[int] 0 on success, 1 on error, 2 on usage error.
 *
 *  @sideeffects
 *   Prints an error to stderr and exits the program in the case of error.
 */
int FbleMemTestMain(int argc, const char** argv, FbleGeneratedModule* module);

#endif // FBLE_TEST_MEM_TEST_H_
