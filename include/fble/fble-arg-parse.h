/**
 * @file fble-arg-parse.h
 *  Command line argument parsing.
 */

#ifndef FBLE_ARG_PARSE_H_
#define FBLE_ARG_PARSE_H_

#include <stdbool.h>    // for bool

#include "fble-load.h"  // for FbleSearchPath

/**
 * @func[FbleArgParser] Generic interface for argument parsing.
 *  Parses a command line argument.
 *
 *  @arg[void*] dest
 *   value to update with the contents of the parsed arg.
 *  @arg[int*] argc
 *   pointer to number of arguments remaining.
 *  @arg[const char***] argv
 *   pointer to arguments remaining.
 *  @arg[bool*] error
 *   boolean value to set in case of error.
 *
 *  @returns bool
 *   true if the next argument was consumed by the parser, false otherwise.
 *
 *  @sideeffects
 *   @i There will only be side effects if the function returns true.
 *   @i Updates dest with the value of the parsed argument.
 *   @i Advances argc and argv to the next argument.
 *   @i Sets error to true in case of error parsing the matched argument.
 *   @i Prints an error message to stderr in case of error.
 */
typedef bool FbleArgParser(void* dest, int* argc, const char*** argv, bool* error);

/**
 * @func[FbleParseBoolArg] Parse a boolean flag command line argument.
 *  Sample argument usage:
 *  
 *  @code[sh] @
 *   --foo
 *
 *  The flag may be specified at most once on the command line.
 *
 *  @arg[const char*] name
 *   the name of the flag to parse, such as "--foo".
 *  @arg[bool*] dest
 *   boolean value to update based on the flag setting.
 *  @arg[int*] argc
 *   pointer to number of arguments remaining.
 *  @arg[const char***] argv
 *   pointer to arguments remaining.
 *  @arg[bool*] error
 *   boolean value to set in case of error.
 *
 *  @returns bool
 *   true if the next argument matches the given name, false otherwise.
 *
 *  @sideeffects
 *   @i There will only be side effects if the function returns true.
 *   @i Updates dest with the value of the parsed argument.
 *   @i Advances argc and argv to the next argument.
 *   @i Sets error to true in case of error parsing the matched argument.
 *   @i Prints an error message to stderr in case of error.
 */
bool FbleParseBoolArg(const char* name, bool* dest, int* argc, const char*** argv, bool* error);

/**
 * @func[FbleParseIntArg] Parse an int command line argument.
 *  Sample argument usage:
 *  
 *  @code[sh] @
 *   --foo 123
 *
 *  The flag may be specified at most once on the command line.
 *
 *  @arg[const char*] name
 *   the name of the flag to parse, such as "--foo".
 *  @arg[int*] dest
 *   int value to update based on the flag setting.
 *  @arg[int*] argc
 *   pointer to number of arguments remaining.
 *  @arg[const char***] argv
 *   pointer to arguments remaining.
 *  @arg[bool*] error
 *   boolean value to set in case of error.
 *
 *  @returns bool
 *   true if the next argument matches the given name, false otherwise.
 *
 *  @sideeffects
 *   @i There will only be side effects if the function returns true.
 *   @i Updates dest with the value of the parsed argument.
 *   @i Advances argc and argv to the next argument.
 *   @i Sets error to true in case of error parsing the matched argument.
 *   @i Prints an error message to stderr in case of error.
 */
bool FbleParseIntArg(const char* name, int* dest, int* argc, const char*** argv, bool* error);

/**
 * @func[FbleParseStringArg] Parse a string command line argument.
 *  Sample argument usage:
 *
 *  @code[sh] @
 *   --foo value
 * 
 *  The flag may be specified at most once on the command line.
 *
 *  @arg[const char*] name
 *   the name of the flag to parse, such as "--foo".
 *  @arg[const char**] dest
 *   string value to update based on the flag setting.
 *  @arg[int*] argc
 *   pointer to number of arguments remaining.
 *  @arg[const char***] argv
 *   pointer to arguments remaining.
 *  @arg[bool*] error
 *   boolean value to set in case of error.
 *
 *  @returns bool
 *   true if the next argument matches the given name, false otherwise.
 *
 *  @sideeffects
 *   @i There will only be side effects if the function returns true.
 *   @i Updates dest with the value of the parsed argument.
 *   @i Advances argc and argv to the next argument.
 *   @i Sets error to true in case of error parsing the matched argument.
 *   @i Prints an error message to stderr in case of error.
 */
bool FbleParseStringArg(const char* name, const char** dest, int* argc, const char*** argv, bool* error);

/**
 * @struct[FbleModuleArg] Describes an input module to a program.
 *  @field[FbleSearchPath*][search_path] Module search path.
 *  @field[FbleModulePath*][module_path] The module argument.
 */
typedef struct {
  FbleSearchPath* search_path;
  FbleModulePath* module_path;
} FbleModuleArg;

/**
 * @func[FbleNewModuleArg] Creates a new FbleModuleArg structure.
 *  @returns FbleModuleArg
 *   A newly allocated FbleModuleArg structure.
 *
 *  @sideeffects
 *   Allocates resources that should be freed using FbleFreeModuleArg when no
 *   longer needed.
 */
FbleModuleArg FbleNewModuleArg();

/**
 * @func[FbleFreeModuleArg] Frees resources associated with an FbleModuleArg.
 *  @arg[FbleModuleArg][arg] The module argument to free.
 *
 *  @sideeffects
 *   Frees resources associated with the module arg.
 */
void FbleFreeModuleArg(FbleModuleArg arg);

/**
 * @func[FbleParseModuleArg] Parse command line options for an FbleModuleArg.
 *  Sample argument usage:
 *
 *  @code[sh] @
 *   -I entry1 -Ientry2 -p package1 --package package2 -m /Foo%
 * 
 *  @arg[FbleModuleArg*] dest
 *   The module arg to populate from the command line.
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
bool FbleParseModuleArg(FbleModuleArg* dest, int* argc, const char*** argv, bool* error);

/**
 * @func[FbleParseInvalidArg] Indicate a command line error.
 *  Pretends to parse an arg, but actually just indicate error.
 *
 *  @arg[int*] argc
 *   pointer to number of arguments remaining.
 *  @arg[const char***] argv
 *   pointer to arguments remaining.
 *  @arg[bool*] error
 *   boolean value to set in case of error.
 *
 *  @returns[bool] true.
 *
 *  @sideeffects
 *   Prints an error message to stderr and sets error to true.
 */
bool FbleParseInvalidArg(int* argc, const char*** argv, bool* error);

#endif // FBLE_ARG_PARSE_H_
