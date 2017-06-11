// loc.c --
//   This file implements utility routines for dealing with fbld locs.

#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr

#include "fbld.h"


// FbldReportError -- see documentation in fbld.h
void FbldReportError(const char* format, FbldLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}
