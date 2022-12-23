/**
 * @file string.c
 * FbleString routines.
 */

#include <fble/fble-string.h>

#include <assert.h>   // for assert
#include <string.h>   // for strlen, strcpy

#include <fble/fble-alloc.h>

// FbleNewString -- see documentation in fble-string.h
FbleString* FbleNewString(const char* str)
{
  FbleString* string = FbleAllocExtra(FbleString, strlen(str) + 1);
  string->refcount = 1;
  string->magic = FBLE_STRING_MAGIC;
  strcpy(string->str, str);
  return string;
}

// FbleCopyString -- see documentation in fble-string.h
FbleString* FbleCopyString(FbleString* string)
{
  string->refcount++;
  return string;
}

// FbleFreeString -- see documentation in fble-string.h
void FbleFreeString(FbleString* string)
{
  // If the string magic is wrong, the string is corrupted. That suggests we
  // have already freed this string, and that something is messed up with
  // tracking FbleString refcounts. 
  assert(string->magic == FBLE_STRING_MAGIC && "corrupt FbleString");
  if (--string->refcount == 0) {
    FbleFree(string);
  }
}
