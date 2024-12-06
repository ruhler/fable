/**
 * @file fble-program.h
 *  Representations of fble programs.
 */

#ifndef FBLE_PROGRAM_H_
#define FBLE_PROGRAM_H_

#include <stdio.h>  // for FILE

#include "fble-module-path.h"
#include "fble-string.h"

/** Abstract syntax tree of an expression. */
typedef struct FbleExpr FbleExpr;

/** Compiled Fble bytecode. */
typedef struct FbleCode FbleCode;

/**
 * @struct[FbleModule] Contents of an fble module.
 *  Either one or both of 'type' and 'value' fields may be supplied. The
 *  'value' field is required to run or generate code for the module. The type
 *  of the module can be determined either from the 'type' field or the type
 *  of the 'value' field. If both 'type' and 'value' are supplied, the
 *  typechecker will check that they describe the same type for the module.
 *
 *  The @a[code] and @a[profile_blocks] fields are populated by calling
 *  FbleCompileModule or FbleCompileProgram.
 *
 *  @field[FbleModulePath*][path] The path to the module.
 *  @field[FbleModulePathV][deps] List of modules this module depends on.
 *  @field[FbleExpr*][type] Abstract syntax of the module type. May be NULL.
 *  @field[FbleExpr*][value]
 *   Abstract syntax of the module implementation. May be NULL.
 *  @field[FbleCode*][code]
 *   Compiled bytecode to compute the module's value. May be NULL.
 *
 *   The code describes the body of a function that takes the computed module
 *   values for each module listed in 'deps' as arguments to the function
 *  @field[FbleNameV][profile_blocks]
 *   Profiling blocks used by the compiled code for the module.
 */
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExpr* type;
  FbleExpr* value;
  FbleCode* code;
  FbleNameV profile_blocks;
} FbleModule;

/**
 * @struct[FbleModuleV] Vector of FbleModule.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleLoadedModule*][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleModule* xs;
} FbleModuleV;

/**
 * @struct[FbleProgram] Contents for a full fble program.
 *  The program is represented as a list of dependant module in topological
 *  dependancy order. Later modules in the list may depend on earlier modules
 *  in the list, but not the other way around.
 *
 *  The last module in the list is the main program.
 *
 *  @field[FbleModuleV][modules] Program modules.
 */
typedef struct {
  FbleModuleV modules;
} FbleProgram;

/**
 * @func[FbleFreeModule] Frees an FbleModule.
 *  @arg[FbleModule*][module] The module to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given module.
 */
void FbleFreeModule(FbleModule* module);

/**
 * @func[FbleFreeProgram] Frees an FbleProgram.
 *  @arg[FbleProgram*][program] The program to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeProgram(FbleProgram* program);

#endif // FBLE_PROGRAM_H_
