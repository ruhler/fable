/**
 * @file fble-link.h
 *  Routines for loading fble programs.
 */

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-function.h"
#include "fble-generate.h"
#include "fble-load.h"
#include "fble-profile.h"
#include "fble-value.h"

/**
 * @func[FbleLink] Loads an optionally compiled program.
 *  This function attempts to load a compiled program if available, and if
 *  not, attempts to load from source.
 *
 *  If module is non-NULL, loads from compiled. Otherwise loads from
 *  module_path.
 *
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *  @arg[FbleNativeModule*] module
 *   The native main module function. May be NULL.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for locating .fble files.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load.
 *
 *  @returns FbleValue*
 *   A zero-argument fble function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleLink(FbleValueHeap* heap, FbleProfile* profile, FbleNativeModule* module, FbleSearchPath* search_path, FbleModulePath* module_path);

/**
 * @func[FblePrintCompiledHeaderLine] Prints an information line about a compiled module.
 *  This is a convenience function for providing more information to users as
 *  part of a fble compiled main function. It prints a header line if the
 *  compiled module is not NULL, of the form something like:
 *
 *  @code[txt] @
 *   fble-debug-test: fble-test -m /DebugTest% (compiled)
 *
 *  @arg[FILE*] stream
 *   The output stream to print to.
 *  @arg[const char*] tool
 *   Name of the underlying tool, e.g. "fble-test".
 *  @arg[const char*] arg0
 *   argv[0] from the main function.
 *  @arg[FbleNativeModule*] module
 *   Optional native module to get the module name from.
 *
 *  @sideeffects
 *   Prints a header line to the given stream.
 */
void FblePrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FbleNativeModule* module);

#endif // FBLE_LINK_H_
