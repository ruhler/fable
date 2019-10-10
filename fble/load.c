// load.c --
//   This file implements routines for loading an fble program.

#include "internal.h"

// FbleLoad -- see documentation in fble-syntax.h
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root)
{
  FbleProgram* program = FbleAlloc(arena, FbleProgram);
  program->main = FbleParse(arena, filename, root);
  if (program->main == NULL) {
    return NULL;
  }

  return program;
}
