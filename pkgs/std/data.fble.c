/**
 * @file data.fble.c
 *  Implementation of routines to interact with fble standard data types.
 */
#include <assert.h>   // for assert
#include <stdlib.h>   // for abort
#include <string.h>   // for strchr

#include "data.fble.h"

#include <fble/fble-runtime.h>   // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#define INTP_TAGWIDTH 2
#define INT_TAGWIDTH 2
#define LIST_TAGWIDTH 1
#define CONS_FIELDC 2
#define MAYBE_TAGWIDTH 1

static FbleValue* MakeIntP(FbleRuntime* runtime, int64_t x);
static int64_t ReadIntP(FbleValue* num);

/**
 * @func[MakeIntP] Makes an FbleValue of type @l{/Std/Int/IntP%.IntP@}.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[int64_t][x] The integer value. Must be greater than 0.
 *
 *  @returns[FbleValue*]
 *   An FbleValue for the integer.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* MakeIntP(FbleRuntime* runtime, int64_t x)
{
  assert(x > 0);
  if (x == 1) {
    return FbleNewEnumValue(runtime, INTP_TAGWIDTH, 0);
  }

  FbleValue* p = MakeIntP(runtime, x / 2);
  return FbleNewUnionValue(runtime, INTP_TAGWIDTH, 1 + (x % 2), p);
}

/**
 * @func[ReadIntP] Reads a number of type @l{/Std/Int/IntP%.IntP@}.
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

// FbleNewCharValue -- see documentation in data.fble.h
FbleValue* FbleNewCharValue(FbleRuntime* runtime, wchar_t c)
{
  return FbleNewStructValue_(runtime, 1, FbleNewIntValue(runtime, (uint64_t)c));
}

// FbleCharValueAccess -- see documentation in data.fble.h
wchar_t FbleCharValueAccess(FbleValue* c)
{
  return (wchar_t)FbleIntValueAccess(FbleStructValueField(c, 1, 0));
}

// See documentation in int.fble.h
FbleValue* FbleNewIntValue(FbleRuntime* runtime, int64_t x)
{
  if (x < 0) {
    FbleValue* p = MakeIntP(runtime, -x);
    FbleValue* result = FbleNewUnionValue(runtime, INT_TAGWIDTH, 0, p);
    return result;
  }

  if (x == 0) {
    return FbleNewEnumValue(runtime, INT_TAGWIDTH, 1);
  }

  FbleValue* p = MakeIntP(runtime, x);
  return FbleNewUnionValue(runtime, INT_TAGWIDTH, 2, p);
}

// FbleIntValueAccess -- see documentation in data.fble.h
int64_t FbleIntValueAccess(FbleValue* x)
{
  switch (FbleUnionValueTag(x, INT_TAGWIDTH)) {
    case 0: return -ReadIntP(FbleUnionValueArg(x, INT_TAGWIDTH));
    case 1: return 0;
    case 2: return ReadIntP(FbleUnionValueArg(x, INT_TAGWIDTH));
    default: assert(false && "Invalid Int@ tag"); abort();
  }
}

// See documentation in data.fble.h
FbleValue* FbleNewMaybeValue(FbleRuntime* runtime, FbleValue* arg)
{
  if (arg == NULL) {
    return FbleNewEnumValue(runtime, MAYBE_TAGWIDTH, 1);
  }
  return FbleNewUnionValue(runtime, MAYBE_TAGWIDTH, 0, arg);
}

// FbleStringValueAccess -- see documentation in data.fble.h
char* FbleStringValueAccess(FbleValue* str)
{
  struct { size_t size; wchar_t* xs; } chars;
  FbleInitVector(chars);
  while (FbleUnionValueTag(str, LIST_TAGWIDTH) == 0) {
    FbleValue* charP = FbleUnionValueArg(str, LIST_TAGWIDTH);
    FbleValue* charV = FbleStructValueField(charP, CONS_FIELDC, 0);
    str = FbleStructValueField(charP, CONS_FIELDC, 1);

    wchar_t c = FbleCharValueAccess(charV);
    FbleAppendToVector(chars, c);
  }
  FbleAppendToVector(chars, '\0');

  size_t mblen = wcstombs(NULL, chars.xs, 0);
  if (mblen == (size_t)-1) {
    return NULL;
  }

  char* buf = FbleAllocArray(char, mblen + 1);
  wcstombs(buf, chars.xs, mblen + 1);
  FbleFreeVector(chars);
  return buf;
}

// FbleNewStringValue -- see documentation in data.fble.h
FbleValue* FbleNewStringValue(FbleRuntime* runtime, const char* str)
{
  size_t mblen = mbstowcs(NULL, str, 0);
  if (mblen == (size_t)-1) {
    return NULL;
  }

  wchar_t chars[mblen + 1];
  mbstowcs(chars, str, mblen + 1);

  FbleValue* charS = FbleNewEnumValue(runtime, LIST_TAGWIDTH, 1);
  for (size_t i = 0; i < mblen; ++i) {
    FbleValue* charV = FbleNewCharValue(runtime, chars[mblen - i - 1]);
    FbleValue* charP = FbleNewStructValue_(runtime, 2, charV, charS);
    charS = FbleNewUnionValue(runtime, LIST_TAGWIDTH, 0, charP);
  }
  return charS;
}
