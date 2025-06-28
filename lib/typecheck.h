/**
 * @file typecheck.h
 *  Header for fble type checking.
 */

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include <fble/fble-program.h>  // for FbleModule, FbleProgram

#include "program.h"    // for FbleModuleMap
#include "tc.h"         // for FbleTc

/**
 * @func[FbleTypeCheckModule] Typechecks the main module of the given program.
 *  An FbleTc is produced for the main module of the program. The FbleTc
 *  produced is a type checked expression suitable for use in the body of a
 *  function that takes the computed module values for each module listed in
 *  module->deps as arguments to the function.
 *
 *  @arg[FbleProgram*][program] The program to typecheck
 *
 *  @returns[FbleTc*]
 *   The type checked expression for the body of the main module, or NULL in
 *   case of failure to type check.
 *
 *  @sideeffects
 *   @i Prints messages to stderr in case of failure to type check.
 *   @item
 *    The user is responsible for freeing the returned FbleTc using FbleFreeTc
 *    when it is no longer needed.
 */
FbleTc* FbleTypeCheckModule(FbleProgram* program);

/**
 * @func[FbleTypeCheckProgram] Typechecks all modules of the given program.
 *  An FbleTc is produced for each module in the program. The FbleTc produced
 *  is a type checked expression suitable for use in the body of a function
 *  that takes the computed module values for each module listed in
 *  module->deps as arguments to the function.
 *
 *  @arg[FbleProgram*][program] The program to typecheck
 *
 *  @returns[FbleModuleMap*]
 *   An map from FbleModule* to FbleTc* for each module in the program.
 *   NULL in case of error to typecheck.
 *
 *  @sideeffects
 *   @i Prints messages to stderr in case of failure to type check.
 *   @item
 *    The user is responsible for freeing the returned map and all FbleTc*
 *    values contained therein when no longer needed.
 */
FbleModuleMap* FbleTypeCheckProgram(FbleProgram* program);

#endif // FBLE_INTERNAL_TYPECHECK_H_
