// parse.c --
//   This file implements routines to parse an fbld declaration from a text
//   file into abstract syntax form.

#include <stdio.h>    // for fprintf, stderr, NULL

#include "fblc.h"     // for FblcArena
#include "fbld.h"

// FbldParseMDecl -- see documentation in fbld.h
FbldMDecl* FbldParseMDecl(FblcArena* arena, const char* filename)
{
  fprintf(stderr, "TODO: implement FbldParseMDecl\n");
  return NULL;
}
