
#include "fble-name.h"

#include <string.h>   // for strcmp

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
  fprintf(stream, "%s", name.name->str);
  switch (name.space) {
    case FBLE_NORMAL_NAME_SPACE:
      // Nothing to add here
      break;

    case FBLE_TYPE_NAME_SPACE:
      fprintf(stream, "@");
      break;
  }
}
