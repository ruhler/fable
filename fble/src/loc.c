
#include "fble-loc.h"

#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr

#include "fble-alloc.h"
#include "fble-string.h"

// FbleCopyLoc -- see documentation in fble-loc.h
FbleLoc FbleCopyLoc(FbleArena* arena, FbleLoc loc)
{
  FbleLoc copy = {
    .source = FbleCopyString(arena, loc.source),
    .line = loc.line,
    .col = loc.col,
  };
  return copy;
}

// FbleFreeLoc -- see documentation in fble-loc.h
void FbleFreeLoc(FbleArena* arena, FbleLoc loc)
{
  FbleFreeString(arena, loc.source);
}

// FbleReportWarning -- see documentation in fble-loc.h
void FbleReportWarning(const char* format, FbleLoc loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: warning: ", loc.source->str, loc.line, loc.col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// FbleReportError -- see documentation in fble-loc.h
void FbleReportError(const char* format, FbleLoc loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc.source->str, loc.line, loc.col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}
