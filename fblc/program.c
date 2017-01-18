// Program.c --
//
//   The file contains utilities for working with the abstract syntax for 
//   programs.

#include <assert.h>     // For assert
#include <stdarg.h>     // For va_start
#include <stdio.h>      // For fprintf

#include "fblcs.h"


// NamesEqual --
//
//   Test whether two names are the same.
//
// Inputs:
//   a - The first name for the comparison.
//   b - The second name for the comparison.
//
// Result:
//   The value true if the names 'a' and 'b' are the same, false otherwise.
//
// Side effects:
//   None

bool NamesEqual(Name a, Name b)
{
  return strcmp(a, b) == 0;
}

// ReportError --
//
//   Prints a formatted error message to standard error with location
//   information. The format is the same as for printf, with the first
//   argument for conversion following the loc argument.
//   
// Inputs:
//   format - A printf style format string.
//   loc - The location associated with the error.
//   ... - Subsequent arguments for conversion based on the format string.
//
// Result:
//   None.
//
// Side effects:
//   Prints an error message to standard error.

void ReportError(const char* format, Loc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}
