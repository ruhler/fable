/**
 * @file fble-arg-parse.h
 * Command line argument parsing.
 */

#ifndef FBLE_ARG_PARSE_H_
#define FBLE_ARG_PARSE_H_

#include <stdbool.h>    // for bool

#include "fble-load.h"  // for FbleSearchPath

/**
 * Parse a boolean flag command line argument.
 *
 * Sample argument usage:
 *   --foo
 *
 * The flag may be specified at most once on the command line.
 *
 * @param name    the name of the flag to parse, such as "--foo".
 * @param dest    boolean value to update based on the flag setting.
 * @param argc    pointer to number of arguments remaining.
 * @param argv    pointer to arguments remaining.
 * @param error   boolean value to set in case of error.
 *
 * @returns
 *   true if the next argument matches the given name, false otherwise.
 *
 * @sideeffects
 * * There will only be side effects if the function returns true.
 * * Updates dest with the value of the parsed argument.
 * * Advances argc and argv to the next argument.
 * * Sets error to true in case of error parsing the matched argument.
 * * Prints an error message to stderr in case of error.
 */
bool FbleParseBoolArg(const char* name, bool* dest, int* argc, const char*** argv, bool* error);

/**
 * Parse a string command line argument.
 *
 * Sample argument usage:
 *   --foo value
 * 
 * The flag may be specified at most once on the command line.
 *
 * @param name    the name of the flag to parse, such as "--foo".
 * @param dest    string value to update based on the flag setting.
 * @param argc    pointer to number of arguments remaining.
 * @param argv    pointer to arguments remaining.
 * @param error   boolean value to set in case of error.
 *
 * @returns
 *   true if the next argument matches the given name, false otherwise.
 *
 * @sideeffects
 * * There will only be side effects if the function returns true.
 * * Updates dest with the value of the parsed argument.
 * * Advances argc and argv to the next argument.
 * * Sets error to true in case of error parsing the matched argument.
 * * Prints an error message to stderr in case of error.
 */
bool FbleParseStringArg(const char* name, const char** dest, int* argc, const char*** argv, bool* error);

/**
 * Describes an input module to a program.
 */
typedef struct {
  FbleSearchPath* search_path;
  FbleModulePath* module_path;
} FbleModuleArg;

/**
 * Creates a new FbleModuleArg structure.
 *
 * @returns
 *   A newly allocated FbleModuleArg structure.
 *
 * @sideeffects
 *   Allocates resources that should be freed using FbleFreeModuleArg when no
 *   longer needed.
 */
FbleModuleArg FbleNewModuleArg();

/**
 * Frees resources associated with an FbleModuleArg.
 *
 * @param arg  The module argument to free.
 *
 * @sideeffects
 *   Frees resources associated with the module arg.
 */
void FbleFreeModuleArg(FbleModuleArg arg);

/**
 * Parse command line options for an FbleModuleArg.
 *
 * Sample argument usage:
 *   -I entry1 -Ientry2 -p package1 --package package2 -m /Foo%
 * 
 * @param dest    The module arg to populate from the command line.
 * @param argc    pointer to number of arguments remaining.
 * @param argv    pointer to arguments remaining.
 * @param error   boolean value to set in case of error.
 *
 * @returns
 *   true if the next argument was consumed, false otherwise.
 *
 * @sideeffects
 * * There will only be side effects if the function returns true.
 * * Updates arg with the value of the parsed arguments.
 * * Advances argc and argv to the next argument.
 * * Sets error to true in case of error parsing the matched argument.
 * * Prints an error message to stderr in case of error.
 */
bool FbleParseModuleArg(FbleModuleArg* dest, int* argc, const char*** argv, bool* error);

/**
 * Indicate a command line error.
 *
 * Pretends to parse an arg, but actually just indicate error.
 *
 * @param argc    pointer to number of arguments remaining.
 * @param argv    pointer to arguments remaining.
 * @param error   boolean value to set in case of error.
 *
 * @returns true.
 *
 * @sideeffects
 * * Prints an error message to stderr and sets error to true.
 */
bool FbleParseInvalidArg(int* argc, const char*** argv, bool* error);

#endif // FBLE_ARG_PARSE_H_
