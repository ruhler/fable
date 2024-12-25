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
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *  @arg[FbleNativeModuleV] native_search_path
 *   The search path to use for locating native modules.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for locating .fble files.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load.
 *  @arg[FbleStringV*] build_deps
 *   Output to store list of files the load depended on. This should be a
 *   preinitialized vector, or NULL.
 *
 *  @returns FbleValue*
 *   A zero-argument fble function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @item
 *    The user should free strings added to build_deps when no longer
 *    needed, including in the case when program loading fails.
 */
FbleValue* FbleLink(FbleValueHeap* heap, FbleProfile* profile, FbleNativeModuleV native_search_path, FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps);

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
