/**
 * @file unused.h
 *  Function for detecting unused variables.
 */

#ifndef FBLE_INTERNAL_UNUSED_H_
#define FBLE_INTERNAL_UNUSED_H_

#include "expr.h"

/**
 * @func[FbleWarnAboutUnusedVars] Prints warnings about unused vars.
 *  The expression should be a well formed and properly typed fble expression.
 *
 *  @arg[FbleExpr*][expr] The expression to check.
 *
 *  @sideeffects
 *   Prints a warning message for each unused variable in the given
 *   expression.
 */
void FbleWarnAboutUnusedVars(FbleExpr* expr);

#endif // FBLE_INTERNAL_UNUSED_H_
