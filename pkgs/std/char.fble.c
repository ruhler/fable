/**
 * @file char.fble.c
 *  Implementation of routines to interact with @l{Char@} type.
 */
#include <assert.h>   // for assert
#include <string.h>   // for strchr

#include "char.fble.h"
#include "int.fble.h"

#include <fble/fble-value.h>   // for FbleValue, etc.

// FbleNewCharValue -- see documentation in char.fble.h
FbleValue* FbleNewCharValue(FbleValueHeap* heap, wchar_t c)
{
  return FbleNewStructValue_(heap, 1, FbleNewIntValue(heap, (uint64_t)c));
}

// FbleCharValueAccess -- see documentation in char.fble.h
wchar_t FbleCharValueAccess(FbleValue* c)
{
  return (wchar_t)FbleIntValueAccess(FbleStructValueField(c, 1, 0));
}
