/**
 * @file fble-main.h
 *  Helper functions for writing Fble*Main functions.
 */
#ifndef FBLE_MAIN_H_
#define FBLE_MAIN_H_

#include <stdbool.h>    // for bool

#include "fble-arg-parse.h"   // for FbleMainArg

/**
 * @struct[FbleMainArgs] Describes common arguments to a main program.
 *  @field[FbleModuleArg][module] The main module to run.
 *  @field[const char*][profile_file] Filename to write the profile to.
 *  @field[const char*][deps_file] Filename to write the deps file to.
 *  @field[const char*][deps_target] Target to write to the deps file.
 *  @field[bool][help] True to output help info.
 *  @field[bool][version] True to output version info.
 */
typedef struct {
  FbleModuleArg module;
  const char* profile_file;
  const char* deps_file;
  const char* deps_target;
  bool help;
  bool version;
} FbleMainArgs;

/**
 * @func[FbleNewMainArgs] Creates a new FbleMainArgs structure.
 *  @returns FbleMainArgs
 *   A newly allocated FbleMainArgs structure.
 *
 *  @sideeffects
 *   Allocates resources that should be freed using FbleFreeMainArgs when no
 *   longer needed.
 */
FbleMainArgs FbleNewMainArgs();

/**
 * @func[FbleFreeMainArgs] Frees resources associated with an FbleMainArgs.
 *  @arg[FbleMainArgs][arg] The main args to free.
 *
 *  @sideeffects
 *   Frees resources associated with the main args.
 */
void FbleFreeMainArgs(FbleMainArgs args);

/**
 * @func[FbleParseMainArg] Parse a command line options for FbleMainArgs.
 *  @arg[bool] preloaded
 *   True if the main module is preloaded. False otherwise.
 *  @arg[FbleMainArgs*] dest
 *   The main args to populate from the command line.
 *  @arg[int*] argc
 *   pointer to number of arguments remaining.
 *  @arg[const char***] argv
 *   pointer to arguments remaining.
 *  @arg[bool*] error
 *   boolean value to set in case of error.
 *
 *  @returns bool
 *   true if the next argument was consumed, false otherwise.
 *
 *  @sideeffects
 *   @i There will only be side effects if the function returns true.
 *   @i Updates arg with the value of the parsed arguments.
 *   @i Advances argc and argv to the next argument.
 *   @i Sets error to true in case of error parsing the matched argument.
 *   @i Prints an error message to stderr in case of error.
 */
bool FbleParseMainArg(bool preloaded, FbleMainArgs* dest, int* argc, const char*** argv, bool* error);

#endif // FBLE_MAIN_H_

