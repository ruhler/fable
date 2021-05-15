// fble-compile.h --
//   Public header file for functions to help with loading fble programs as
//   values into C code.
#ifndef FBLE_MAIN_H_
#define FBLE_MAIN_H_

#include "fble-profile.h"
#include "fble-value.h"

// FbleCompiledMainFunction --
//   The type of a main function exported from compiled .fble code.
typedef FbleValue* FbleCompiledMainFunction(FbleValueHeap* heap, FbleProfile* profile);

// FbleCompiledMain --
//   A macro that should be set at compile time to the name of the compiled
//   main program to use, or left unset to default to loading interpreted
//   code.
//
// See documentation of FbleMain for how to use this.
#ifdef FbleCompiledMain
FbleValue* FbleCompiledMain(FbleValueHeap* heap, FbleProfile* profile);
#else  // FbleCompiledMain
#define FbleCompiledMain NULL
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
// Usage: (In the case where compiled_main is NULL)
//   SEARCH_PATH MODULE_PATH
//
//   Loads the fble module with given MODULE_PATH using SEARCH_PATH as the
//   directory to search for the .fble file for the module.
//
// Example use:
//   FbleValue* linked = FbleMain(heap, profile, FbleCompiledMain, argc, argv);
//
// Where FbleCompiledMain defaults to NULL, but can be overridden at compile
// time using something like -DFbleCompiledMain=FbleCompiledMain.
FbleValue* FbleMain(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledMainFunction* compiled_main, int argc, char** argv);

#endif // FBLE_MAIN_H_
