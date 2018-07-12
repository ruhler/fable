// name.c --
//   This file implements utility routines for manipulating fble names.

#include <string.h>   // for strcmp

#include "fble.h"


// FbleNamesEqual -- see documentation in fbld.h
bool FbleNamesEqual(const char* a, const char* b)
{
  return strcmp(a, b) == 0;
}
