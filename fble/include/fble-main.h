// fble-compile.h --
//   Public header file for functions to help with loading fble programs as
//   values into C code.
#ifndef FBLE_MAIN_H_
#define FBLE_MAIN_H_

#include "fble-link.h"
#include "fble-profile.h"
#include "fble-value.h"

// FbleCompiledMain --
//   A macro that should be set at compile time to the name of the compiled
//   main program to use, or left unset to default to loading interpreted
//   code.
//   
//   For example, compile with -DFbleCompiledMain=FbleCompiledMain linking
//   with the .o file of a .c program generated with
//   `fble-compile --export FbleCompiledMain ...` to statically link that
//   compiled main module. Compile without any special flags to allow loading
//   .fble files directly at runtime based on command line arguments.
//
// See documentation of FbleMain for how to use this.
//
// FBLE_MAIN_USAGE_SUMMARY --
//   A string to include in an application usage summary line describing the
//   usage of the argc/argv passed to FbleMain.
//
// FBLE_MAIN_USAGE_DETAIL --
//   A string to include in an application usage description with more detail
//   about the usage of argc/argv passed to FbleMain.
//
// FBLE_MAIN_USAGE_EXAMPLE --
//   A string to include an an application usage example command line showing
//   what to use in the argc/argv passed to FbleMain.
#ifdef FbleCompiledMain
void FbleCompiledMain(FbleExecutableProgram* program);

#define FBLE_MAIN_USAGE_SUMMARY ""
#define FBLE_MAIN_USAGE_DETAIL "Loads the statically linked fble program.\n"
#define FBLE_MAIN_USAGE_EXAMPLE ""

#else  // FbleCompiledMain

#define FbleCompiledMain NULL

#define FBLE_MAIN_USAGE_SUMMARY "SEARCH_PATH MODULE_PATH"
#define FBLE_MAIN_USAGE_DETAIL \
  "MODULE_PATH is the module path of the main fble module to load.\n" \
  "SEARCH_PATH is the directory containing the .fble files for the module hierarchy.\n"
#define FBLE_MAIN_USAGE_EXAMPLE "prgms /Fble/Tests%"

#endif // FbleCompiledMain

// FbleMain -- 
//   Load an fble program. The program to load is either one that was compiled
//   in statically, or one read from the search path and module path specified
//   in argc and argv.
//
// Inputs:
//   heap - the heap to use to load the program with.
//   profile - the profile, if any, to add blocks to for the loaded program.
//   compiled_main - The compiled program to load, or NULL if an interpreted
//                   program should be used instead.
//   argc - the number of arguments in argv.
//   argv - a list of arguments describing the search path and main module to
//          load, in the case when compiled_main is NULL.
//
// Results:
//   The loaded fble program, or NULL on error.
//
// Side effects:
// * The returned program should be freed using FbleReleaseValue when no long
//   in use.
// * Prints messages to stderr in case of error.
//
// Usage:
//   See FBLE_MAIN_USAGE_SUMMARY, FBLE_MAIN_USAGE_DETAIL, and
//   FBLE_MAIN_USAGE_EXAMPLE for usage information on the arguments passed in
//   argc/argv.
//
// Example use:
//   FbleValue linked = FbleMain(heap, profile, FbleCompiledMain, argc, argv);
//
// Where FbleCompiledMain defaults to NULL, but can be overridden at compile
// time using something like -DFbleCompiledMain=FbleCompiledMain.
FbleValue* FbleMain(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* compiled_main, int argc, char** argv);

#endif // FBLE_MAIN_H_
