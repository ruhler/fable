// name.c --
//   This file implements utility routines for manipulating fbld names.

#include <string.h>   // for strcmp

#include "fbld.h"


// FbldNamesEqual -- see documentation in fbld.h
bool FbldNamesEqual(FbldName a, FbldName b)
{
  return strcmp(a, b) == 0;
}
