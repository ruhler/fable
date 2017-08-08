// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf

#include "fblc.h"
#include "fbld.h"


// FbldCompile -- see documentation in fbld.h
FblcProc* FbldCompile(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity)
{
  assert(false && "TODO: FbldCompile");
  return NULL;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  assert(false && "TODO: FbldCompileValue");
  return NULL;
}
