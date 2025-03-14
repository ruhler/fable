/**
 * @file char.fble.c
 *  Implementation of routines to interact with @l{Char@} type.
 */
#include <assert.h>   // for assert
#include <string.h>   // for strchr

#include "char.fble.h"

#include <fble/fble-value.h>   // for FbleValue, etc.

// Chars --
//   The list of characters (in tag order) supported by the /Core/Char%.Char@ type.
static const char* Chars =
    "\n\t\r !\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz"
    "{|}~";
#define CHARS_TAGWIDTH 7

// FbleNewCharValue -- see documentation in char.fble.h
FbleValue* FbleNewCharValue(FbleValueHeap* heap, char c)
{
  char* p = strchr(Chars, c);
  if (p == NULL || c == '\0') {
    assert(c != '?');
    return FbleNewCharValue(heap, '?');
  }
  assert(p >= Chars);
  size_t tag = p - Chars;
  return FbleNewEnumValue(heap, CHARS_TAGWIDTH, tag);
}

// FbleCharValueAccess -- see documentation in char.fble.h
char FbleCharValueAccess(FbleValue* c)
{
  return Chars[FbleUnionValueTag(c, CHARS_TAGWIDTH)];
}
