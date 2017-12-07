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

  assert(a->r != NULL);
  assert(b->r != NULL);
  if (a->r->tag != b->r->tag) {
    return false;
  }

  switch (a->r->tag) {
    case FBLD_FAILED_R:
      assert(false);
      return false;

    case FBLD_ENTITY_R: {
      FbldEntityR* aent = (FbldEntityR*)a->r;
      FbldEntityR* bent = (FbldEntityR*)b->r;
      if (!FbldNamesEqual(aent->decl->name->name, bent->decl->name->name)) {
        return false;
      }

      if (!FbldQRefsEqual(aent->mref, bent->mref)) {
        return false;
      }
      return true;
    }

    case FBLD_PARAM_R: {
      FbldParamR* apar = (FbldParamR*)a->r;
      FbldParamR* bpar = (FbldParamR*)b->r;

      if (!FbldNamesEqual(apar->decl->name->name, bpar->decl->name->name)) {
        return false;
      }

      if (apar->index != bpar->index) {
        return false;
      }
      return true;
    }

    default:
      assert(false && "Invalid R tag");
      return false;
  }
}
