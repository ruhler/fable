// syntax.c --
//   This file implements the fble abstract syntax routines.

#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr
#include <string.h>   // for strcmp

#include "fble-syntax.h"


// FbleReportError -- see documentation in fble-syntax.h
void FbleReportError(const char* format, FbleLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// FbleNamesEqual -- see documentation in fble-syntax.h
bool FbleNamesEqual(const char* a, const char* b)
{
  return strcmp(a, b) == 0;
}
