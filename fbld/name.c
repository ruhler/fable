// name.c --
//   This file implements utility routines for manipulating fbld names.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp

#include "fbld.h"


// FbldNamesEqual -- see documentation in fbld.h
bool FbldNamesEqual(const char* a, const char* b)
{
  return strcmp(a, b) == 0;
}

// FbldQRefsEqual -- see documentation in fbld.h
bool FbldQRefsEqual(FbldQRef* a, FbldQRef* b)
{
  if (a == NULL && b == NULL) {
    return true;
  }

  if (a == NULL || b == NULL) {
    return false;
  }

  assert(a->r.state == FBLD_RSTATE_RESOLVED);
  assert(b->r.state == FBLD_RSTATE_RESOLVED);
  if (!FbldNamesEqual(a->r.decl->name->name, b->r.decl->name->name)) {
    return false;
  }

  if (!FbldQRefsEqual(a->r.mref, b->r.mref)) {
    return false;
  }
  return true;
}
