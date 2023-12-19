/**
 * @file typecheck.h
 *  Header for fble type checking.
 */

#ifndef FBLE_INTERNAL_TYPECHECK_H_
#define FBLE_INTERNAL_TYPECHECK_H_

#include <fble/fble-load.h>

#include "tc.h"

/**
 * Typechecks the main module of the given program.
 *
 * An FbleTc is produced for the main module of the program. The FbleTc
 * produced is a type checked expression suitable for use in the body of a
 * function that takes the computed module values for each module listed in
 * module->deps as arguments to the function.
 *
 * @param program  The program to typecheck
 *
 * @returns
 *   The type checked expression for the body of the main module, or NULL in
 *   case of failure to type check.
 *
 * @sideeffects
 * * Prints messages to stderr in case of failure to type check.
 * * The user is responsible for freeing the returned FbleTc using FbleFreeTc
 *   when it is no longer needed.
 */
FbleTc* FbleTypeCheckModule(FbleLoadedProgram* program);

/**
 * Typechecks all modules of the given program.
 *
 * An FbleTc is produced for each module in the program. The FbleTc
 * produced is a type checked expression suitable for use in the body of a
 * function that takes the computed module values for each module listed in
 * module->deps as arguments to the function.
 *
 * @param program - the program to typecheck
 *
 * @returns
 *   An array of FbleTc, one for each module in the program, or NULL in case
 *   of error to typecheck.
 *
 * @sideeffects
 * * Prints messages to stderr in case of failure to type check.
 * * The user is responsible for freeing the returned array and all FbleTc*
 *   values contained therein when no longer needed.
 */
FbleTc** FbleTypeCheckProgram(FbleLoadedProgram* program);

#endif // FBLE_INTERNAL_TYPECHECK_H_
