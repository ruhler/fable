/**
 * @file loc.c
 * FbleLoc routines.
 */

#include <fble/fble-loc.h>

#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr

#include <fble/fble-alloc.h>
#include <fble/fble-string.h>

// See documentation in fble-loc.h.
FbleLoc FbleNewLoc(const char* source, size_t line, size_t col)
{
  FbleLoc loc = {
    .source = FbleNewString(source),
    .line = line,
    .col = col,
  };
  return loc;
}

// See documentation in fble-loc.h.
FbleLoc FbleCopyLoc(FbleLoc loc)
{
  FbleLoc copy = {
    .source = FbleCopyString(loc.source),
    .line = loc.line,
    .col = loc.col,
  };
  return copy;
}

// See documentation in fble-loc.h.
void FbleFreeLoc(FbleLoc loc)
{
  FbleFreeString(loc.source);
}

// See documentation in fble-loc.h.
void FbleReportWarning(const char* format, FbleLoc loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%zi:%zi: warning: ", loc.source->str, loc.line, loc.col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// See documentation in fble-loc.h.
void FbleReportError(const char* format, FbleLoc loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%zi:%zi: error: ", loc.source->str, loc.line, loc.col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}
