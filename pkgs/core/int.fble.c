
#include <assert.h>       // for assert
#include <stdlib.h>       // for abort

#include "int.fble.h"

#include <fble/fble-value.h>   // for FbleValue, etc.

static FbleValue* MakeIntP(FbleValueHeap* heap, int64_t x);
static int64_t ReadIntP(FbleValue* num);

// MakeIntP -- 
//   Make an FbleValue of type /Core/Int/Core/IntP%.IntP@ for the given integer.
//
// Inputs:
//   heap - the heap to use for allocations.
//   x - the integer value. Must be greater than 0.
//
// Results:
//   An FbleValue for the integer.
//
// Side effects:
//   Allocates a value that should be freed with FbleReleaseValue when no
//   longer needed. Behavior is undefined if x is not positive.
static FbleValue* MakeIntP(FbleValueHeap* heap, int64_t x)
{
  assert(x > 0);
  if (x == 1) {
    return FbleNewEnumValue(heap, 0);
  }

  FbleValue* p = MakeIntP(heap, x / 2);
  FbleValue* result = FbleNewUnionValue(heap, 1 + (x % 2), p);
  FbleReleaseValue(heap, p);
  return result;
}

// ReadIntP --
//   Read a number from an FbleValue of type /Core/Int/Core/IntP%.IntP@.
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
    case 1: return 2 * ReadIntP(FbleUnionValueArg(x));
    case 2: return 2 * ReadIntP(FbleUnionValueArg(x)) + 1;
    default: assert(false && "Invalid IntP@ tag"); abort();
  }
}

// See documentation in int.fble.h
FbleValue* FbleNewIntValue(FbleValueHeap* heap, int64_t x)
{
  if (x < 0) {
    FbleValue* p = MakeIntP(heap, -x);
    FbleValue* result = FbleNewUnionValue(heap, 0, p);
    FbleReleaseValue(heap, p);
    return result;
  }

  if (x == 0) {
    return FbleNewEnumValue(heap, 1);
  }

  FbleValue* p = MakeIntP(heap, x);
  FbleValue* result = FbleNewUnionValue(heap, 2, p);
  FbleReleaseValue(heap, p);
  return result;
}

// FbleIntValueAccess -- see documentation in int.fble.h
int64_t FbleIntValueAccess(FbleValue* x)
{
  switch (FbleUnionValueTag(x)) {
    case 0: return -ReadIntP(FbleUnionValueArg(x));
    case 1: return 0;
    case 2: return ReadIntP(FbleUnionValueArg(x));
    default: assert(false && "Invalid Int@ tag"); abort();
  }
}
