/**
 * @file version.c
 *  Routines related to the fble version number.
 */

#include "fble/fble-version.h"

#include <stdio.h>    // for FILE, fprintf

// See documentation in fble-version.h
void FblePrintVersion(FILE* stream, const char* tool)
{
  if (tool) {
    fprintf(stream, "%s ", tool);
  }
  fprintf(stream, "%s (%s)\n", FBLE_VERSION, FbleBuildStamp);
}
