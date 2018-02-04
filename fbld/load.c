// load.c --
//   This file implements routines for loading fbld programs.

#include "fblc.h"
#include "fbld.h"

// FbldLoadProgram -- see documentation in fbld.h
FbldProgram* FbldLoadProgram(FblcArena* arena, const char* path)
{
  FbldProgram* prgm = FbldParseProgram(arena, path);
  if (prgm == NULL) {
    return NULL;
  }

  if (!FbldCheckProgram(arena, prgm)) {
    return NULL;
  }
  return prgm;
}

// FbldLoadCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldLoadCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, const char* path, FbldQRef* entry)
{
  FbldProgram* prgm = FbldParseProgram(arena, path);
  if (prgm == NULL) {
    return NULL;
  }

  if (!FbldCheckProgram(arena, prgm)) {
    return NULL;
  }

  if (!FbldCheckQRef(arena, prgm, entry)) {
    return NULL;
  }
  return FbldCompileProgram(arena, accessv, prgm, entry);
}
