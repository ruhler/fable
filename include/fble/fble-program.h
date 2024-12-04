/**
 * @file fble-program.h
 *  Representations of fble programs.
 */

#ifndef FBLE_PROGRAM_H_
#define FBLE_PROGRAM_H_

#include <stdio.h>  // for FILE

#include "fble-module-path.h"
#include "fble-string.h"

/**
 * Abstract syntax tree of an expression.
 */
typedef struct FbleExpr FbleExpr;

/**
 * @struct[FbleLoadedModule] Abstract syntax tree of a module.
 *  Either one or both of 'type' and 'value' fields may be supplied. The
 *  'value' field is required to run or generate code for the module. The type
 *  of the module can be determined either from the 'type' field or the type
 *  of the 'value' field. If both 'type' and 'value' are supplied, the
 *  typechecker will check that they describe the same type for the module.
 *
 *  @field[FbleModulePath*][path] The path to the module.
 *  @field[FbleModulePathV][deps] List of modules this module depends on.
 *  @field[FbleExpr*][type] Abstract syntax of the module type. May be NULL.
 *  @field[FbleExpr*][value]
 *   Abstract syntax of the module implementation. May be NULL.
 */
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExpr* type;
  FbleExpr* value;
} FbleLoadedModule;

/**
 * @struct[FbleLoadedModuleV] Vector of FbleLoadedModule.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleLoadedModule*][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleLoadedModule* xs;
} FbleLoadedModuleV;

/**
 * @struct[FbleLoadedProgram] Abstract syntax tree for a full fble program.
 *  The program is represented as a list of dependant module in topological
 *  dependancy order. Later modules in the list may depend on earlier modules
 *  in the list, but not the other way around.
 *
 *  The last module in the list is the main program. The module path for the
 *  main module is the empty path @l{/%}.
 *
 *  @field[FbleLoadedModuleV][modules] Program modules.
 */
typedef struct {
  FbleLoadedModuleV modules;
} FbleLoadedProgram;

/**
 * @func[FbleFreeLoadedProgram] Frees an FbleLoadedProgram.
 *  @arg[FbleLoadedProgram*][program] The program to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeLoadedProgram(FbleLoadedProgram* program);

#endif // FBLE_PROGRAM_H_
