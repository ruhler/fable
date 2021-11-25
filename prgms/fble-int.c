
#include <assert.h>       // for assert
#include <stdlib.h>       // for abort

#include "fble-int.h"

#include "fble-value.h"   // for FbleValue, etc.

static int64_t ReadIntP(FbleValue* num);

// ReadIntP --
//   Read a number from an FbleValue of type /Int/IntP%.IntP@.
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   Behavior is undefined if the int value cannot be represented using the C
//   int64_t type, for example because it is too large.
static int64_t ReadIntP(FbleValue* x)
{
  switch (FbleUnionValueTag(x)) {
    case 0: return 1;
    case 1: return 2 * ReadIntP(FbleUnionValueAccess(x));
    case 2: return 2 * ReadIntP(FbleUnionValueAccess(x)) + 1;
    default: assert(false && "Invalid IntP@ tag"); abort();
  }
}

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
int64_t FbleIntValueRead(FbleValue* x)
{
  switch (FbleUnionValueTag(x)) {
    case 0: return -ReadIntP(FbleUnionValueAccess(x));
    case 1: return 0;
    case 2: return ReadIntP(FbleUnionValueAccess(x));
    default: assert(false && "Invalid Int@ tag"); abort();
  }
}
