/**
 * @file fble-link.h
 *  Routines for loading fble programs.
 */

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-execute.h"
#include "fble-load.h"
#include "fble-profile.h"
#include "fble-value.h"

/**
 * @func[FbleCompiledModuleFunction] Compiled module function type.
 *  The type of a module function generated for compiled .fble code.
 *
 *  @arg[FbleExecutableProgram*] program
 *   Modules loaded into the program so far.
 *
 *  @sideeffects
 *   Adds this module to the given program if it is not already in the
 *   program.
 */
typedef void FbleCompiledModuleFunction(FbleExecutableProgram* program);

/**
 * @func[FbleLoadFromCompiled] Loads a precompiled fble program.
 *  @arg[FbleExecutableProgram*] program
 *   The program to add the module and its dependencies to.
 *  @arg[FbleExecutableModule*] module
 *   The compiled module to load. Borrowed.
 *  @arg[size_t] depc
 *   The number of other modules this module immediately depends on.
 *  @arg[FbleCompiledModuleFunction**] deps
 *   The immediate dependencies of this module.
 *
 *  @sideeffects
 *   Adds this module and any modules it depends on to the given program.
 */
void FbleLoadFromCompiled(FbleExecutableProgram* program, FbleExecutableModule* module, size_t depc, FbleCompiledModuleFunction** deps);

/**
 * @func[FbleLinkFromCompiledOrSource] Loads an optionally compiled program.
 *  This is a convenience function that attempts to load a compiled program if
 *  available, and if not, attempts to load from source.
 *
 *  If module is non-NULL, loads from compiled. Otherwise loads from
 *  module_path.
 *
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *  @arg[FbleCompiledModuleFunction*] module
 *   The compiled main module function. May be NULL.
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
 *   The user should call FbleReleaseValue on the returned value when it is no
 *   longer needed.
 */
FbleValue* FbleLinkFromCompiledOrSource(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* module, FbleSearchPath* search_path, FbleModulePath* module_path);

/**
 * @func[FblePrintCompiledHeaderLine] Prints an information line about a compiled module.
 *  This is a convenience function for providing more information to users as
 *  part of a fble compiled main function. It prints a header line if the
 *  compiled module is not NULL, of the form something like:
 *
 *  @code[txt] @
 *   fble-debug-test: fble-test -m /DebugTest% (compiled)
 *
 *  Note, extracting the module name is a relatively expensive operation,
 *  because it involves loading the entire module and its dependencies, then
 *  throwing all of that away when it's done.
 *
 *  @arg[FILE*] stream
 *   The output stream to print to.
 *  @arg[const char*] tool
 *   Name of the underlying tool, e.g. "fble-test".
 *  @arg[const char*] arg0
 *   argv[0] from the main function.
 *  @arg[FbleCompiledModuleFunction*] module
 *   The optionally compiled module to get the module name from.
 *
 *  @sideeffects
 *   Prints a header line to the given stream.
 */
void FblePrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FbleCompiledModuleFunction* module);

#endif // FBLE_LINK_H_
