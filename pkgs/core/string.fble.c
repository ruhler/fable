
#include "string.fble.h"

#include <string.h>         // for strlen

#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "char.fble.h" // for FbleCharValueAccess, FbleNewCharValue

// FbleStringValueAccess -- see documentation in string.fble.h
char* FbleStringValueAccess(FbleValue* str)
{
  struct { size_t size; char* xs; } chars;
  FbleVectorInit(chars);
  while (FbleUnionValueTag(str) == 0) {
    FbleValue* charP = FbleUnionValueAccess(str);
    FbleValue* charV = FbleStructValueAccess(charP, 0);
    str = FbleStructValueAccess(charP, 1);

    char c = FbleCharValueAccess(charV);
    FbleVectorAppend(chars, c);
  }
  FbleVectorAppend(chars, '\0');
  return chars.xs;
}

// FbleNewStringValue -- see documentation in string.fble.h
FbleValue* FbleNewStringValue(FbleValueHeap* heap, const char* str)
{
  size_t length = strlen(str);
  FbleValue* charS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < length; ++i) {
    FbleValue* charV = FbleNewCharValue(heap, str[length - i - 1]);
    FbleValue* charP = FbleNewStructValue_(heap, 2, charV, charS);
    FbleReleaseValue(heap, charV);
    FbleReleaseValue(heap, charS);
    charS = FbleNewUnionValue(heap, 0, charP);
    FbleReleaseValue(heap, charP);
  }
  return charS;
}

// FbleDebugTrace -- see documentation in string.fble.h
void FbleDebugTrace(FbleValue* str)
{
  char* cstr = FbleStringValueAccess(str);
  fprintf(stderr, "%s", cstr);
  FbleFree(cstr);
}
