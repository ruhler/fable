/**
 * @file fble-link.h
 * Routines for loading fble programs.
 */

#ifndef FBLE_LINK_H_
#define FBLE_LINK_H_

#include "fble-execute.h"
#include "fble-load.h"
#include "fble-profile.h"
#include "fble-value.h"

/**
 * Links an fble program.
 *
 * Links the modules of an executable program together into a single FbleValue
 * representing a zero-argument function that can be used to compute the value
 * of the program.
 *
 * @param heap      Heap to use for allocations.
 * @param program   The executable program to link.
 * @param profile   Profile to construct for the linked program. May be NULL.
 *
 * @returns
 *   An FbleValue representing a zero-argument function that can be used to
 *   compute the value of the program.
 *
 * @sideeffects
 *   Allocates an FbleValue that should be freed using FbleReleaseValue when
 *   no longer needed.
 */
FbleValue* FbleLink(FbleValueHeap* heap, FbleExecutableProgram* program, FbleProfile* profile);

/**
 * Loads an fble program from source.
 *
 * Loads, compiles, and links a full program from source.
 *
 * @param heap          Heap to use for allocations.
 * @param search_path   The search path to use for locating .fble files.
 * @param module_path   The module path for the main module to load. Borrowed.
 * @param profile       Profile to populate with blocks. May be NULL.
 *
 * @returns
 *   A zero-argument function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 * @sideeffects
 * * Prints an error message to stderr if the program fails to load.
 * * The user should call FbleReleaseValue on the returned value when it is no
 *   longer needed.
 */
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, FbleSearchPath search_path, FbleModulePath* module_path, FbleProfile* profile);

/**
 * Compiled module function type.
 *
 * The type of a module function generated for compiled .fble code.
 *
 * @param program  Modules loaded into the program so far.
 *
 * @sideeffects
 *   Adds this module to the given program if it is not already in the
 *   program.
 */
typedef void FbleCompiledModuleFunction(FbleExecutableProgram* program);

/**
 * Loads a precompiled fble program.
 *
 * @param program  The program to add the module and its dependencies to.
 * @param module   The compiled module to load. Borrowed.
 * @param depc     The number of other modules this module immediately depends
 *                 on.
 * @param deps     The immediate dependencies of this module.
 *
 * @sideeffects
 * * Adds this module and any modules it depends on to the given program.
 */
void FbleLoadFromCompiled(FbleExecutableProgram* program, FbleExecutableModule* module, size_t depc, FbleCompiledModuleFunction** deps);

/**
 * Loads and links a precompield fble program.
 *
 * @param module    The compiled main module function.
 * @param heap      Heap to use for allocations.
 * @param profile   Profile to populate with blocks. May be NULL.
 *
 * @returns
 *   A zero-argument fble function that computes the value of the program when
 *   executed.
 *
 * @sideeffects
 * * The user should call FbleReleaseValue on the returned value when it is no
 *   longer needed.
 */
FbleValue* FbleLinkFromCompiled(FbleCompiledModuleFunction* module, FbleValueHeap* heap, FbleProfile* profile);

/**
 * Loads an optionally compiled program.
 *
 * This is a convenience function that attempts to load a compiled program
 * if available, and if not, attempts to load from source.
 *
 * If module is non-NULL, loads from compiled. Otherwise loads from
 * module_path.
 *
 * @param heap          Heap to use for allocations.
 * @param profile       Profile to populate with blocks. May be NULL.
 * @param module        The compiled main module function. May be NULL.
 * @param search_path   The search path to use for locating .fble files.
 * @param module_path   The module path for the main module to load.
 *
 * @returns
 *   A zero-argument fble function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 * @sideeffects
 * * The user should call FbleReleaseValue on the returned value when it is no
 *   longer needed.
 */
FbleValue* FbleLinkFromCompiledOrSource(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* module, FbleSearchPath search_path, const char* module_path);

#endif // FBLE_LINK_H_
