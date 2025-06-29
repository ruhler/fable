/**
 * @file fble-program.h
 *  Representations of fble programs.
 */

#ifndef FBLE_PROGRAM_H_
#define FBLE_PROGRAM_H_

#include <stdio.h>  // for FILE

#include "fble-function.h"
#include "fble-module-path.h"
#include "fble-string.h"

/** Abstract syntax tree of an expression. */
typedef struct FbleExpr FbleExpr;

/** Compiled Fble bytecode. */
typedef struct FbleCode FbleCode;

/** Forward reference for FbleModule. */
typedef struct FbleModule FbleModule;

/**
 * @struct[FbleModuleV] Vector of FbleModule.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleModule**][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleModule** xs;
} FbleModuleV;

/**
 * @enum[FbleModuleMagic] Magic number used in FbleModule.
 *  @field[FBLE_MODULE_MAGIC] The magic value.
 */
typedef enum {
  FBLE_MODULE_MAGIC = 0x9881432
} FbleModuleMagic;

/**
 * @struct[FbleModule] Contents of an fble module.
 *  Reference counted, pass by pointer, explicitly copy and free required.
 *
 *  Either one or both of 'type' and 'value' fields may be supplied. The
 *  'value' field is required to run or generate code for the module. The type
 *  of the module can be determined either from the 'type' field or the type
 *  of the 'value' field. If both 'type' and 'value' are supplied, the
 *  typechecker will check that they describe the same type for the module.
 *
 *  The @a[code] and @a[profile_blocks] fields are populated by calling
 *  FbleCompileModule or FbleCompileProgram. Alternatively the @a[exe] and
 *  @a[profile_blocks] fields are populated by loading a generated module.
 *
 *  @field[size_t][refcount] Current reference count.
 *  @field[FbleModuleMagic][magic] FBLE_MODULE_MAGIC.
 *  @field[FbleModulePath*][path] The path to the module.
 *  @field[FbleModuleV][type_deps]
 *   List of modules the @a[type] field depends on.
 *  @field[FbleModuleV][link_deps]
 *   List of modules the implementation depends on.
 *  @field[FbleExpr*][type]
 *   Abstract syntax of an expression whose type is the module type. May be
 *   NULL.
 *
 *   The expression describes the body of a function that takes the computed
 *   module values for each module listed in 'type_deps' as arguments to the
 *   function
 *  @field[FbleExpr*][value]
 *   Abstract syntax of the module implementation. May be NULL.
 *
 *   The expression describes the body of a function that takes the computed
 *   module values for each module listed in 'link_deps' as arguments to the
 *   function
 *  @field[FbleCode*][code]
 *   Compiled bytecode to compute the module's value. May be NULL.
 *
 *   The code describes the body of a function that takes the computed module
 *   values for each module listed in 'link_deps' as arguments to the function
 *  @field[FbleExecutable*][exe]
 *   Executable code to compute the module's value. May be NULL.
 *
 *   The code describes the body of a function that takes the computed module
 *   values for each module listed in 'link_deps' as arguments to the function
 *  @field[FbleNameV][profile_blocks]
 *   Profiling blocks used by the compiled code for the module.
 */
struct FbleModule {
  size_t refcount;
  FbleModuleMagic magic;
  FbleModulePath* path;
  FbleModuleV type_deps;
  FbleModuleV link_deps;
  FbleExpr* type;
  FbleExpr* value;
  FbleCode* code;
  FbleExecutable* exe;
  FbleNameV profile_blocks;
};

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
 * @func[FbleCopyModule] Makes a reference counted copy of a module.
 *  @arg[FbleModule*][module] The module to copy.
 *  @returns[FbleModule*] The copy of the module.
 *  @sideeffects
 *   Increments the reference count on a module. Call FbleFreeModule on the
 *   returned result when done using it.
 */
FbleModule* FbleCopyModule(FbleModule* module);

/**
 * @func[FbleFreeModule] Frees an FbleModule.
 *  @arg[FbleModule*][module] The module to free.
 *
 *  @sideeffects
 *   Decrements the reference count on a module and frees resources associated
 *   with it as appropriate.
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

// Forward declaration of FblePreloadedModule.
typedef struct FblePreloadedModule FblePreloadedModule;

/**
 * @struct[FblePreloadedModuleV] A vector of FblePreloadedModule.
 *  @field[size_t][size] Number of elements.
 *  @field[FblePreloadedModule**][xs] Elements.
 */
typedef struct {
  size_t size;
  FblePreloadedModule** xs;
} FblePreloadedModuleV;

/**
 * @struct[FblePreloadedModule] A preloaded module implementation.
 *  @field[FbleModulePath*][path] The path to the module.
 *  @field[FblePreloadedModuleV][deps]
 *   List of modules this module depends on.
 *  @field[FbleExecutable*][executable]
 *   Code to compute the value of the module, suitable for use in the body of
 *   a function that takes the computed module values for each module listed
 *   in 'deps' as arguments to the function.
 *   
 *   executable->args must be the same as deps.size.
 *   executable->statics must be 0.
 *  @field[FbleNameV][profile_blocks]
 *   Profile blocks used by functions in the module.
 */
struct FblePreloadedModule {
  FbleModulePath* path;
  FblePreloadedModuleV deps;
  FbleExecutable* executable;
  FbleNameV profile_blocks;
};

#endif // FBLE_PROGRAM_H_
