// load.c --
//   This file implements routines for loading an fble program.

#include <assert.h>   // for assert

#include "internal.h"

// FbleLoad -- see documentation in fble-syntax.h
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root)
{
  FbleProgram* program = FbleAlloc(arena, FbleProgram);
  FbleVectorInit(arena, program->modules);
  FbleModuleRefV module_refs;
  FbleVectorInit(arena, module_refs);
  program->main = FbleParse(arena, filename, root, &module_refs);
  if (program->main == NULL) {
    return NULL;
  }

  assert(module_refs.size == 0 && "TODO: Support loading of dependant modules");
  return program;
}
