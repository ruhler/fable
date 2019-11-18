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
bool FbleNamesEqual(FbleName* a, FbleName* b)
{
  return a->space == b->space && strcmp(a->name, b->name) == 0;
}

// FblePrintName -- see documentation in fble-syntax.h
void FblePrintName(FILE* stream, FbleName* name)
{
  fprintf(stream, name->name);
  switch (name->space) {
    case FBLE_NORMAL_NAME_SPACE:
      // Nothing to add here
      break;

    case FBLE_TYPE_NAME_SPACE:
      fprintf(stream, "@");
      break;

    case FBLE_MODULE_NAME_SPACE:
      fprintf(stream, "%%");
      break;
  }
}
