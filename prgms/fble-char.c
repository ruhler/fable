
#include <assert.h>   // for assert
#include <string.h>   // for strchr

#include "fble-char.h"

#include "fble-value.h"   // for FbleValue, etc.

// Chars --
//   The list of characters (in tag order) supported by the /Char%.Char@ type.
static const char* Chars =
    "\n\t !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz"
    "{|}~";

// FbleCharValueRead -- see documentation in fble-char.h
char FbleCharValueRead(FbleValue* c)
{
  return Chars[FbleUnionValueTag(c)];
}

// FbleCharValueWrite -- see documentation in fble-char.h
FbleValue* FbleCharValueWrite(FbleValueHeap* heap, char c)
{
  char* p = strchr(Chars, c);
  if (p == NULL || c == '\0') {
    assert(c != '?');
    return FbleCharValueWrite(heap, '?');
  }
  assert(p >= Chars);
  size_t tag = p - Chars;
  return FbleNewEnumValue(heap, tag);
}
