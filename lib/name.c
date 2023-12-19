/**
 * @file name.c
 *  FbleName routines.
 */

#include <fble/fble-name.h>

#include <string.h>   // for strcmp

#include "parse.h"    // for FbleIsPlainWord

// FbleCopyName -- see documentation in fble-name.h
FbleName FbleCopyName(FbleName name)
{
  FbleName copy = {
    .name = FbleCopyString(name.name),
    .space = name.space,
    .loc = FbleCopyLoc(name.loc)
  };
  return copy;
}

// FbleFreeName -- see documentation in fble-name.h
void FbleFreeName(FbleName name)
{
  FbleFreeString(name.name);
  FbleFreeLoc(name.loc);
}

// FbleNamesEqual -- see documentation in fble-name.h
bool FbleNamesEqual(FbleName a, FbleName b)
{
  return a.space == b.space && strcmp(a.name->str, b.name->str) == 0;
}

// FblePrintName -- see documentation in fble-name.h
void FblePrintName(FILE* stream, FbleName name)
{
  bool quoted = !FbleIsPlainWord(name.name->str);
  if (quoted) {
    fprintf(stream, "'");
  }

  for (const char* c = name.name->str; *c != '\0'; ++c) {
    if (*c == '\'') {
      fprintf(stream, "'");
    }
    fprintf(stream, "%c", *c);
  }

  if (quoted) {
    fprintf(stream, "'");
  }

  switch (name.space) {
    case FBLE_NORMAL_NAME_SPACE:
      // Nothing to add here
      break;

    case FBLE_TYPE_NAME_SPACE:
      fprintf(stream, "@");
      break;
  }
}
