#ifndef FBLE_CORE_INT_FBLE_H_
#define FBLE_CORE_INT_FBLE_H_

#include <fble/fble-value.h>

// FbleIntValueAccess --
//   Read a number from an FbleValue of type /Core/Int%.Int@.
//
// Inputs:
//   x - the FbleValue value of the number.
//
// Results:
//   The number represented as a C int64_t.
//
// Side effects:
//   Behavior is undefined if the int value cannot be represented using the C
//   int64_t type, for example because it is too large.
int64_t FbleIntValueAccess(FbleValue* x);

#endif // FBLE_CORE_INT_FBLE_H_
