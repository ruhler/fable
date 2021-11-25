#ifndef FBLE_INT_H_
#define FBLE_INT_H_

#include "fble-value.h"

// FbleIntValueRead --
//   Read a number from an FbleValue of type /Int%.Int@.
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
int64_t FbleIntValueRead(FbleValue* x);

#endif // FBLE_INT_H_
