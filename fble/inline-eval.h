// inline-eval.h --
//   Header file for inline evaluation.

#ifndef FBLE_INTERNAL_INLINE_EVAL_H_
#define FBLE_INTERNAL_INLINE_EVAL_H_

#include "value.h"

// FbleInlineEval --
//   Perform an inline evaluation on the given inline value expression.
//
// Inputs:
//   heap - the heap to allocate values on.
//   expr - the expression to evaluate. Borrowed.
//
// Results:
//   A newly allocated value that is the result of evaluating the given
//   expression, or NULL if evaluation leads to undefined union field access.
//
// Side effects:
// * The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue* FbleInlineEval(FbleValueHeap* heap, FbleValue* expr);

#endif // FBLE_INTERNAL_INLINE_EVAL_H_
