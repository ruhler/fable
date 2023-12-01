/**
 * @file unused.h
 *
 * Function for detecting unused variables.
 */

#ifndef FBLE_INTERNAL_UNUSED_H_
#define FBLE_INTERNAL_UNUSED_H_

#include "expr.h"

// FbleWarnAboutUnusedVars -
//   Prints a warning for each unused variable in the given expression.
//
// The expression should be a well formed and properly typed fble expression.
//
// Args:
//   expr - the expression to check.
//
// Side effects:
//   Prints a warning message for each unused variable in the given
//   expression.
void FbleWarnAboutUnusedVars(FbleExpr* expr);

#endif // FBLE_INTERNAL_UNUSED_H_
