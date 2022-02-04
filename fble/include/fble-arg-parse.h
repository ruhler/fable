// fble-arg-parse.h --
//   Header file for command line argument parsing library.

#ifndef FBLE_ARG_PARSE_H_
#define FBLE_ARG_PARSE_H_

#include <stdbool.h>    // for bool

#include "fble-load.h"  // for FbleSearchPath

// FbleParseBoolArg --
//   Parse a boolean flag command line argument.
//
// Sample argument usage:
//   --foo
//
// The flag may be specified at most once on the command line.
//
// Inputs:
//   name - the name of the flag to parse, such as "--foo".
//   dest - boolean value to update based on the flag setting.
//   argc - pointer to number of arguments remaining.
//   argv - pointer to arguments remaining.
//   error - boolean value to set in case of error.
//
// Results:
//   Returns true if the next argument matches the given name, false
//   otherwise.
//
// Side effects:
// * There will only be side effects if the function returns true.
// * Updates dest with the value of the parsed argument.
// * Advances argc and argv to the next argument.
// * Sets error to true in case of error parsing the matched argument.
// * Prints an error message to stderr in case of error.
bool FbleParseBoolArg(const char* name, bool* dest, int* argc, const char*** argv, bool* error);

// FbleParseStringArg --
//   Parse a string command line argument.
//
// Sample argument usage:
//   --foo value
// 
// The flag may be specified at most once on the command line.
//
// Inputs:
//   name - the name of the flag to parse, such as "--foo".
//   dest - string value to update based on the flag setting.
//   argc - pointer to number of arguments remaining.
//   argv - pointer to arguments remaining.
//   error - boolean value to set in case of error.
//
// Results:
//   Returns true if the next argument matches the given name, false
//   otherwise.
//
// Side effects:
// * There will only be side effects if the function returns true.
// * Updates dest with the value of the parsed argument.
// * Advances argc and argv to the next argument.
// * Sets error to true in case of error parsing the matched argument.
// * Prints an error message to stderr in case of error.
bool FbleParseStringArg(const char* name, const char** dest, int* argc, const char*** argv, bool* error);

// FbleParseSearchPathArg --
//   Parse an FbleSearchPath command line argument.
//
// Sample argument usage:
//   --foo entry1 --foo entry2
// 
// Inputs:
//   name - the name of the flag to parse, such as "--foo".
//   dest - search path value to append to based on the flag setting.
//   argc - pointer to number of arguments remaining.
//   argv - pointer to arguments remaining.
//   error - boolean value to set in case of error.
//
// Results:
//   Returns true if the next argument matches the given name, false
//   otherwise.
//
// Side effects:
// * There will only be side effects if the function returns true.
// * Updates dest with the value of the parsed argument.
// * Advances argc and argv to the next argument.
// * Sets error to true in case of error parsing the matched argument.
// * Prints an error message to stderr in case of error.
bool FbleParseSearchPathArg(const char* name, FbleSearchPath* dest, int* argc, const char*** argv, bool* error);

#endif // FBLE_ARG_PARSE_H_
