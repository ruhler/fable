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
FbleValue* FbleNewCharValue(FbleRuntime* runtime, uint32_t c)
{
  return FbleNewStructValue_(runtime, 1, FbleNewIntValue(runtime, c));
}

// FbleCharValueAccess -- see documentation in data.fble.h
uint32_t FbleCharValueAccess(FbleValue* c)
{
  return (uint32_t)FbleIntValueAccess(FbleStructValueField(c, 1, 0));
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
  struct { size_t size; char* xs; } chars;
  FbleInitVector(chars);
  while (FbleUnionValueTag(str, LIST_TAGWIDTH) == 0) {
    FbleValue* charP = FbleUnionValueArg(str, LIST_TAGWIDTH);
    FbleValue* charV = FbleStructValueField(charP, CONS_FIELDC, 0);
    str = FbleStructValueField(charP, CONS_FIELDC, 1);

    uint32_t c = FbleCharValueAccess(charV);

    // 0x00000000 - 0x0000007F:
    //        0xxxxxxx
    if (c <= 0x7F) {
      FbleAppendToVector(chars, c);
      continue;
    }

    // 0x00000080 - 0x000007FF:
    //        110xxxxx 10xxxxxx
    if (c <= 0x7FF) {
      FbleAppendToVector(chars, 0xC0 | (c >> 6));
      FbleAppendToVector(chars, 0x80 | ((c >> 0) & 0x3F));
      continue;
    }

    // 0x00000800 - 0x0000FFFF:
    //        1110xxxx 10xxxxxx 10xxxxxx
    if (c <= 0xFFFF) {
      FbleAppendToVector(chars, 0xE0 | (c >> 12));
      FbleAppendToVector(chars, 0x80 | ((c >> 6) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 0) & 0x3F));
      continue;
    }

    // 0x00010000 - 0x001FFFFF:
    //        11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if (c <= 0x1FFFFF) {
      FbleAppendToVector(chars, 0xF0 | (c >> 18));
      FbleAppendToVector(chars, 0x80 | ((c >> 12) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 6) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 0) & 0x3F));
      continue;
    }

    // 0x00200000 - 0x03FFFFFF:
    //        111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    if (c <= 0x3FFFFFF) {
      FbleAppendToVector(chars, 0xF0 | (c >> 24));
      FbleAppendToVector(chars, 0x80 | ((c >> 18) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 12) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 6) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 0) & 0x3F));
      continue;
    }

    // 0x04000000 - 0x7FFFFFFF:
    //        1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    if (c <= 0x7FFFFFFF) {
      FbleAppendToVector(chars, 0xF0 | (c >> 30));
      FbleAppendToVector(chars, 0x80 | ((c >> 24) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 18) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 12) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 6) & 0x3F));
      FbleAppendToVector(chars, 0x80 | ((c >> 0) & 0x3F));
      continue;
    }

    return NULL;
  }

  FbleAppendToVector(chars, '\0');
  return chars.xs;
}

// FbleNewStringValue -- see documentation in data.fble.h
FbleValue* FbleNewStringValue(FbleRuntime* runtime, const char* str)
{
  size_t maxlen = strlen(str);
  uint32_t chars[maxlen];

  // Convert UTF-8 to unicode code points.
  size_t len = 0;
  for ( ; *str != '\0'; len++) {
    char b0 = *str++;

    // 0x00000000 - 0x0000007F:
    //        0xxxxxxx
    if (b0 <= 0x7F) {
      chars[len] = b0;
      continue;
    }

    // 0x00000080 - 0x000007FF:
    //        110xxxxx 10xxxxxx
    if ((b0 & 0xE0) == 0xC0) {
      char b1 = *str++;
      if ((b1 & 0xC0) != 0x80) {
        // Invalid UTF-8 encoding.
        return NULL;
      }
      uint32_t v = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
      if (v <= 0x7F) {
        // Not shortest possible encoding.
        return NULL;
      }
      chars[len] = v;
      continue;
    }

    // 0x00000800 - 0x0000FFFF:
    //        1110xxxx 10xxxxxx 10xxxxxx
    // 0x00010000 - 0x001FFFFF:
    //        11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    // 0x00200000 - 0x03FFFFFF:
    //        111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    // 0x04000000 - 0x7FFFFFFF:
    //        1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    assert(false && "TODO: finish utf8 decoding");
    return NULL;
  }

  // Build up the fble string from unicode code points.
  FbleValue* charS = FbleNewEnumValue(runtime, LIST_TAGWIDTH, 1);
  for (size_t i = 0; i < len; ++i) {
    FbleValue* charV = FbleNewCharValue(runtime, chars[len - i - 1]);
    FbleValue* charP = FbleNewStructValue_(runtime, 2, charV, charS);
    charS = FbleNewUnionValue(runtime, LIST_TAGWIDTH, 0, charP);
  }
  return charS;
}
