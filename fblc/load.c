// load.c --
//
//   This file implements a routine for loading a text fblc program, including
//   parsing, name resolution, and type checking.

#include "fblcs.h"

// FblcsLoadProgram -- see documentation in fblcs.h
FblcsLoaded* FblcsLoadProgram(FblcArena* arena, const char* filename, const char* entry)
{
  FblcsProgram* sprog = FblcsParseProgram(arena, filename);
  if (sprog == NULL) {
    return NULL;
  }

  if (!FblcsCheckProgram(sprog)) {
    return NULL;
  }
  return FblcsCompileProgram(arena, sprog, entry);
}
