/**
 * @file data.fble.c
 *  Implementation of routines to interact with fble standard data types.
 */
#include <assert.h>   // for assert
#include <stdlib.h>   // for abort
#include <string.h>   // for strlen

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
/**
 * @func[FbleUtf8Encode] Encode a character in utf8.
 *  You give a unicode code point, it gives you back the sequence of utf-8
 *  bytes to represent that code point. The resulting bytes are not null
 *  terminated or anything like that.
 *
 *  @arg[uint32_t][c] The unicode code point.
 *  @arg{char[6]}[bytes] 6 bytes of output buffer.
 *  @returns[size_t]
 *   The number of bytes used for the encoded character. Zero on failure.
 *  @sideeffects
 *   @i Fills in up to 6 bytes in the given bytes array.
 */
size_t FbleUtf8Encode(uint32_t c, char bytes[6])
{
  // 0x00000000 - 0x0000007F:
  //        0xxxxxxx
  if (c <= 0x7F) {
    bytes[0] = c;
    return 1;
  }

  // 0x00000080 - 0x000007FF:
  //        110xxxxx 10xxxxxx
  if (c <= 0x7FF) {
    bytes[0] = 0xC0 | (c >> 6);
    bytes[1] = 0x80 | ((c >> 0) & 0x3F);
    return 2;
  }

  // 0x00000800 - 0x0000FFFF:
  //        1110xxxx 10xxxxxx 10xxxxxx
  if (c <= 0xFFFF) {
    bytes[0] = 0xE0 | (c >> 12);
    bytes[1] = 0x80 | ((c >> 6) & 0x3F);
    bytes[2] = 0x80 | ((c >> 0) & 0x3F);
    return 3;
  }

  // 0x00010000 - 0x001FFFFF:
  //        11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if (c <= 0x1FFFFF) {
    bytes[0] = 0xF0 | (c >> 18);
    bytes[1] = 0x80 | ((c >> 12) & 0x3F);
    bytes[2] = 0x80 | ((c >> 6) & 0x3F);
    bytes[3] = 0x80 | ((c >> 0) & 0x3F);
    return 4;
  }

  // 0x00200000 - 0x03FFFFFF:
  //        111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
  if (c <= 0x3FFFFFF) {
    bytes[0] = 0xF0 | (c >> 24);
    bytes[1] = 0x80 | ((c >> 18) & 0x3F);
    bytes[2] = 0x80 | ((c >> 12) & 0x3F);
    bytes[3] = 0x80 | ((c >> 6) & 0x3F);
    bytes[4] = 0x80 | ((c >> 0) & 0x3F);
    return 5;
  }

  // 0x04000000 - 0x7FFFFFFF:
  //        1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
  if (c <= 0x7FFFFFFF) {
    bytes[0] = 0xF0 | (c >> 30);
    bytes[1] = 0x80 | ((c >> 24) & 0x3F);
    bytes[2] = 0x80 | ((c >> 18) & 0x3F);
    bytes[3] = 0x80 | ((c >> 12) & 0x3F);
    bytes[4] = 0x80 | ((c >> 6) & 0x3F);
    bytes[5] = 0x80 | ((c >> 0) & 0x3F);
    return 6;
  }

  return 0;
}

// FbleStringValueAccess -- see documentation in data.fble.h
char* FbleStringValueAccess(FbleValue* str)
{
  struct { size_t size; char* xs; } chars;
  FbleInitVector(chars);
  char buf[8];
  while (FbleUnionValueTag(str, LIST_TAGWIDTH) == 0) {
    FbleValue* charP = FbleUnionValueArg(str, LIST_TAGWIDTH);
    FbleValue* charV = FbleStructValueField(charP, CONS_FIELDC, 0);
    str = FbleStructValueField(charP, CONS_FIELDC, 1);

    uint32_t c = FbleCharValueAccess(charV);
    size_t n = FbleUtf8Encode(c, buf);
    if (n == 0) {
      return NULL;
    }

    for (size_t i = 0; i < n; ++i) {
      FbleAppendToVector(chars, buf[i]);
    }
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
    uint8_t h = (uint8_t)*str++;

    // Count number of 1 bits in the byte.
    size_t n = 0;
    while (h & 0x80) {
      n++;
      h <<= 1;
    }

    // Extract the content bits from the byte.
    uint32_t v = h >> n;

    // Get the content from the next t bytes.
    size_t tn = n > 1 ? (n - 1) : 0;
    for (size_t i = 0; i < tn; ++i) {
      uint8_t t = (uint8_t)*str++;
      if ((t & 0xC0) != 0x80) {
        return NULL;
      }
      v = (v << 6) | (t & 0x3F);
    }

    // Verify we used the shortest encoding.
    static uint32_t limits[6] = { 0x7F, 0x7FF, 0xFFFF, 0x1FFFFF, 0x3FFFFFF, 0x7FFFFFFF };
    for (size_t i = 0; i < 6; ++i) {
      if (v <= limits[i]) {
        if (tn != i) {
          return NULL;
        }
        break;
      }
    }

    chars[len] = v;
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
