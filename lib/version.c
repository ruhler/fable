/**
 * @file version.c
 *  Routines related to the fble version number.
 */

#include "fble/fble-version.h"

#include <stdio.h>    // for FILE, fprintf

/**
 * @value[FbleBuildStamp] A string describing the particular build of fble.
 *  The string includes information about the version and particular commit
 *  fble was built from. For debug purposes only.
 *
 *  @type[const char*]
 */
extern const char* FbleBuildStamp;

// See documentation in fble-version.h
void FblePrintVersion(FILE* stream, const char* tool)
{
  if (tool) {
    fprintf(stream, "%s ", tool);
  }
  fprintf(stream, "%s (%s)\n", FBLE_VERSION, FbleBuildStamp);
}
