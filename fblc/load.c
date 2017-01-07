// load.c --
//
//   This file implements a routine for loading a text fblc program, including
//   parsing, name resolution, and type checking.

#include "fblcs.h"

// SLoadProgram -- see documentation in fblcs.h
SProgram* SLoadProgram(FblcArena* arena, const char* filename)
{
  SProgram* sprog = ParseProgram(arena, filename);
  if (sprog == NULL) {
    return NULL;
  }

  if (!ResolveProgram(sprog)) {
    return NULL;
  }

  if (!CheckProgram(sprog)) {
    return NULL;
  }
  return sprog;
}
