/**
 * @file int.fble.c
 *  Routines for interacting with @l{Int@} type.
 */
#include <assert.h>       // for assert
#include <stdlib.h>       // for abort

#include "int.fble.h"

#include <fble/fble-value.h>   // for FbleValue, etc.

#define INTP_TAGWIDTH 2
#define INT_TAGWIDTH 2

static FbleValue* MakeIntP(FbleValueHeap* heap, int64_t x);
static int64_t ReadIntP(FbleValue* num);

/**
 * @func[MakeIntP] Makes an FbleValue of type @l{/Core/Int/Core/IntP%.IntP@}.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[int64_t][x] The integer value. Must be greater than 0.
 *
 *  @returns[FbleValue*]
 *   An FbleValue for the integer.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* MakeIntP(FbleValueHeap* heap, int64_t x)
{
  assert(x > 0);
  if (x == 1) {
    return FbleNewEnumValue(heap, INTP_TAGWIDTH, 0);
  }

  FbleValue* p = MakeIntP(heap, x / 2);
  return FbleNewUnionValue(heap, INTP_TAGWIDTH, 1 + (x % 2), p);
}

/**
 * @func[ReadIntP] Reads a number of type @l{/Core/Int/Core/IntP%.IntP@}.
 *  @arg[FbleValue*][x] The value of the number.
 *
 *  @returns[int64_t]
 *   The number x represented as an integer.
 *
 *  @sideeffects
 *   Behavior is undefined if the int value cannot be represented using the C
 *   int64_t type, for example because it is too large.
 */
static int64_t ReadIntP(FbleValue* x)
{
  switch (FbleUnionValueTag(x, INTP_TAGWIDTH)) {
    case 0: return 1;
    case 1: return 2 * ReadIntP(FbleUnionValueArg(x, INTP_TAGWIDTH));
    case 2: return 2 * ReadIntP(FbleUnionValueArg(x, INTP_TAGWIDTH)) + 1;
    default: assert(false && "Invalid IntP@ tag"); abort();
  }
}

// See documentation in int.fble.h
FbleValue* FbleNewIntValue(FbleValueHeap* heap, int64_t x)
{
  if (x < 0) {
    FbleValue* p = MakeIntP(heap, -x);
    FbleValue* result = FbleNewUnionValue(heap, INT_TAGWIDTH, 0, p);
    return result;
  }

  if (x == 0) {
    return FbleNewEnumValue(heap, INT_TAGWIDTH, 1);
  }

  FbleValue* p = MakeIntP(heap, x);
  return FbleNewUnionValue(heap, INT_TAGWIDTH, 2, p);
}

// FbleIntValueAccess -- see documentation in int.fble.h
int64_t FbleIntValueAccess(FbleValue* x)
{
  switch (FbleUnionValueTag(x, INT_TAGWIDTH)) {
    case 0: return -ReadIntP(FbleUnionValueArg(x, INT_TAGWIDTH));
    case 1: return 0;
    case 2: return ReadIntP(FbleUnionValueArg(x, INT_TAGWIDTH));
    default: assert(false && "Invalid Int@ tag"); abort();
  }
}
