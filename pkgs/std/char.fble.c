/**
 * @file char.fble.c
 *  Implementation of routines to interact with @l{Char@} type.
 */
#include <assert.h>   // for assert
#include <string.h>   // for strchr

#include "char.fble.h"
#include "int.fble.h"

#include <fble/fble-runtime.h>   // for FbleValue, etc.

// FbleNewCharValue -- see documentation in char.fble.h
FbleValue* FbleNewCharValue(FbleRuntime* runtime, wchar_t c)
{
  return FbleNewStructValue_(runtime, 1, FbleNewIntValue(runtime, (uint64_t)c));
}

// FbleCharValueAccess -- see documentation in char.fble.h
wchar_t FbleCharValueAccess(FbleValue* c)
{
  return (wchar_t)FbleIntValueAccess(FbleStructValueField(c, 1, 0));
}
