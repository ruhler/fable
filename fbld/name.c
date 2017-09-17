// name.c --
//   This file implements utility routines for manipulating fbld names.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp

#include "fbld.h"

static bool MRefsEqual(FbldMRef* a, FbldMRef* b);

// MRefsEqual --
//   Return true if two resolved mrefs are the same.
//
// Inputs:
//   a - The first mref, may be null.
//   b - The second mref, may be null.
//
// Result:
//   true if the two mrefs are the same, false otherwise. An mref is equal to
//   the null mref only if it is itself null.
//
// Side effects:
//   None.
static bool MRefsEqual(FbldMRef* a, FbldMRef* b)
{
  if (a == NULL || b == NULL) {
    return a == NULL && b == NULL;
  }

  if (!FbldNamesEqual(a->name->name, b->name->name)) {
    return false;
  }

  if (a->targs == NULL || b->targs == NULL) {
    return a->targs == NULL && b->targs == NULL;
  }
  assert(a->margs != NULL && b->margs != NULL);

  if (a->targs->size != b->targs->size) {
    return false;
  }

  for (size_t i = 0; i < a->targs->size; ++i) {
    if (!FbldQRefsEqual(a->targs->xs[i], b->targs->xs[i])) {
      return false;
    }
  }

  if (a->margs->size != b->margs->size) {
    return false;
  }

  for (size_t i = 0; i < a->margs->size; ++i) {
    if (!MRefsEqual(a->margs->xs[i], b->margs->xs[i])) {
      return false;
    }
  }

  return true;
}

// FbldNamesEqual -- see documentation in fbld.h
bool FbldNamesEqual(const char* a, const char* b)
{
  return strcmp(a, b) == 0;
}

// FbldQRefsEqual -- see documentation in fbld.h
bool FbldQRefsEqual(FbldQRef* a, FbldQRef* b)
{
  return FbldNamesEqual(a->rname->name, b->rname->name) && MRefsEqual(a->rmref, b->rmref);
}
