// loc.c --
//   This file implements utility routines for dealing with fble locs.

#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr

#include "fble.h"


// FbleReportError -- see documentation in fbld.h
void FbleReportError(const char* format, FbleLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}
