// name.c --
//   This file implements utility routines for manipulating fbld names.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp

#include "fbld.h"

//static bool MRefsEqual(FbldMRef* a, FbldMRef* b);
//
//// MRefsEqual --
////   Return true if two resolved mrefs are the same.
////
//// Inputs:
////   a - The first mref, may be null.
////   b - The second mref, may be null.
////
//// Result:
////   true if the two mrefs are the same, false otherwise. An mref is equal to
////   the null mref only if it is itself null.
////
//// Side effects:
////   None.
//static bool MRefsEqual(FbldMRef* a, FbldMRef* b)
//{
//  if (a == NULL || b == NULL) {
//    return a == NULL && b == NULL;
//  }
//
//  if (!FbldNamesEqual(a->name->name, b->name->name)) {
//    return false;
//  }
//
//  if (a->targv == NULL || b->targv == NULL) {
//    return a->targv == NULL && b->targv == NULL;
//  }
//  assert(a->margv != NULL && b->margv != NULL);
//
//  if (a->targv->size != b->targv->size) {
//    return false;
//  }
//
//  for (size_t i = 0; i < a->targv->size; ++i) {
//    if (!FbldQRefsEqual(a->targv->xs[i], b->targv->xs[i])) {
//      return false;
//    }
//  }
//
//  if (a->margv->size != b->margv->size) {
//    return false;
//  }
//
//  for (size_t i = 0; i < a->margv->size; ++i) {
//    if (!MRefsEqual(a->margv->xs[i], b->margv->xs[i])) {
//      return false;
//    }
//  }
//
//  return true;
//}

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
