/**
 * @file fble-main.h
 *  Helper functions for writing Fble*Main functions.
 */
#ifndef FBLE_MAIN_H_
#define FBLE_MAIN_H_

#include <stdbool.h>    // for bool

#include "fble-arg-parse.h"   // for FbleMainArg

/**
 * @enum[FbleMainStatus] Status codes used by FbleMain.
 *  @field[FBLE_MAIN_SUCCESS] The function completed successfully.
 *  @field[FBLE_MAIN_FAILURE] The function returned failure.
 *  @field[FBLE_MAIN_COMPILE_ERROR] The function failed to compile.
 *  @field[FBLE_MAIN_RUNTIME_ERROR] The function failed to run.
 *  @field[FBLE_MAIN_USAGE_ERROR] Error in command line options.
 *  @field[FBLE_MAIN_OTHER_ERROR] Other kind of error.
 */
typedef enum {
  FBLE_MAIN_SUCCESS = 0,
  FBLE_MAIN_FAILURE = 1,
  FBLE_MAIN_COMPILE_ERROR = 2,
  FBLE_MAIN_RUNTIME_ERROR = 3,
  FBLE_MAIN_USAGE_ERROR = 4,
  FBLE_MAIN_OTHER_ERROR = 5,
} FbleMainStatus;

/**
 * @func[FbleMain] Load, link, and evaluate an Fble Main function.
 *  This is a helper function for doing standard stuff to convert a set of
 *  command line arguments into an evaluated main fble program.
 *
 *  Probably best to look at the source code for a detailed explanation of how
 *  this works.
 *
 *  When FBLE_MAIN_SUCCESS is returned and result is NULL, that indicates the
 *  program should exit immediately with success. For example, the --help
 *  option was passed on the command line.
 *
 *  @arg[FbleArgParser*][arg_parser] Optional custom arg parser.
 *  @arg[void*][data] User data for custom arg parser.
 *  @arg[const char*][tool] Name of the underlying tool, e.g. "fble-test".
 *  @arg[const unsigned char*][usage] Usage help text to output for --help.
 *  @arg[int*][argc] Number of command line arguments.
 *  @arg[const char***][argv] The command line arguments.
 *  @arg[FblePreloadedModule*][preloaded] Optional preloaded module to run.
 *  @arg[FblePreloadedModuleV] builtins
 *   List of builtin modules to search.
 *  @arg[FbleValueHeap*][heap] Heap to use for allocating values.
 *  @arg[FbleProfile*][profile] Profile for evaluating the main program.
 *  @arg[const char**][profile_output_file]
 *   Output parameter for the profile output file.
 *  @arg[uint64_t][profile_sample_period]
 *   Downsampling period to use when outputting the profile.
 *  @arg[FbleValue**][result]
 *   Output parameter for the result of evaluation.
 *
 *  @returns[FbleMainStatus] Status result code.
 *
 *  @sideeffects
 *   @i Advances argc and argv past the parsed arguments.
 *   @i Generates build dependency file if requested.
 *   @i Outputs messages to stderr and stdout in case of error or for --help.
 *   @item
 *    Evaluates the main module, with whatever side effects that has on heap
 *    and profile.
 *   @i Enables or disables profiling as requested.
 *   @i Sets profile_output_file and result based on results.
 */
FbleMainStatus FbleMain(
    FbleArgParser* arg_parser,
    void* data,
    const char* tool,
    const unsigned char* usage,
    int* argc,
    const char*** argv,
    FblePreloadedModule* preloaded,
    FblePreloadedModuleV builtins,
    FbleValueHeap* heap,
    FbleProfile* profile,
    const char** profile_output_file,
    uint64_t* profile_sample_period,
    FbleValue** result);


#endif // FBLE_MAIN_H_

