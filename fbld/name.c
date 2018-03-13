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

  if (a->paramv->size != b->paramv->size) {
    return false;
  }

  for (size_t i = 0; i < a->paramv->size; ++i) {
    if (!FbldQRefsEqual(a->paramv->xs[i], b->paramv->xs[i])) {
      return false;
    }
  }

  assert(a->r != NULL);
  assert(a->r->decl != NULL);

  assert(b->r != NULL);
  assert(b->r->decl != NULL);

  if (!FbldNamesEqual(a->r->decl->name->name, b->r->decl->name->name)) {
    return false;
  }

  if (!FbldQRefsEqual(a->r->mref, b->r->mref)) {
    return false;
  }

  return true;
}

// FbldImportQRef -- see documentation in fbld.h
FbldQRef* FbldImportQRef(FblcArena* arena, FbldQRef* src, FbldQRef* qref)
{
  // Base case for recursion up to the global name space.
  if (qref == NULL) {
    return NULL;
  }

  // The qref should already have been successfully resolved by this point,
  // otherwise something has gone wrong.
  assert(qref->r != NULL);
  assert(qref->r->decl != NULL);

  if (qref->r->param && qref->r->interf == NULL) {
    // qref refers to a static parameter. Find the matching argument.
    FbldQRef* mref = src;
    while (mref != NULL) {
      // Note that mref may have fewer arguments than decl in case it is
      // a partial qref.
      FbldDecl* decl = mref->r->decl;
      for (size_t i = 0; i < mref->paramv->size; ++i) {
        if (decl->paramv->xs[i] == qref->r->decl) {
          FbldQRef* param = mref->paramv->xs[i];
          FbldQRef* imported = FBLC_ALLOC(arena, FbldQRef);
          imported->name = param->name;
          imported->paramv = FBLC_ALLOC(arena, FbldQRefV);
          FblcVectorInit(arena, *(imported->paramv));
          for (size_t i = 0; i < param->paramv->size; ++i) {
            FbldQRef* p = FbldImportQRef(arena, src, param->paramv->xs[i]);
            FblcVectorAppend(arena, *(imported->paramv), p);
          }

          for (size_t i = 0; i < qref->paramv->size; ++i) {
            FbldQRef* p = FbldImportQRef(arena, src, qref->paramv->xs[i]);
            FblcVectorAppend(arena, *(imported->paramv), p);
          }

          imported->mref = param->mref;
          imported->r = param->r;
          return imported;
        }
      }

      bool interf = mref->r->interf != NULL;
      mref = mref->r->mref;
      if (interf) {
        assert(mref->r->decl->tag == FBLD_MODULE_DECL);
        FbldModule* module = (FbldModule*)mref->r->decl;
        mref = FbldImportQRef(arena, mref, module->iref);
      }
    }

    // No match found for the static parameter. We must be importing it into a
    // context with the parameter available in scope, so leave it as is.
    return qref;
  }

  if (qref->r->param && qref->r->interf != NULL) {
    // qref refers to an interface prototype. Find the matching module from
    // the context.
    for (FbldQRef* mref = src; mref != NULL; mref = mref->r->mref) {
      if (mref->r->decl->tag == FBLD_MODULE_DECL) {
        FbldModule* module = (FbldModule*)mref->r->decl;
        if (module->iref->r->decl == &qref->r->interf->_base) {
          FbldQRef* imported = FBLC_ALLOC(arena, FbldQRef);
          imported->name = qref->name;
          imported->paramv = FBLC_ALLOC(arena, FbldQRefV);
          FblcVectorInit(arena, *(imported->paramv));
          for (size_t i = 0; i < qref->paramv->size; ++i) {
            FbldQRef* p = FbldImportQRef(arena, src, qref->paramv->xs[i]);
            FblcVectorAppend(arena, *(imported->paramv), p);
          }

          imported->mref = mref;

          FbldR* r = FBLC_ALLOC(arena, FbldR);
          r->decl = qref->r->decl;
          r->mref = imported->mref;
          r->param = false;
          r->interf = qref->r->interf;
          imported->r = r;
          return imported;
        }
      }
    }

    assert(false && "Failed to match interface.");
    return NULL;
  }

  FbldQRef* imported = FBLC_ALLOC(arena, FbldQRef);
  imported->name = qref->name;
  imported->paramv = FBLC_ALLOC(arena, FbldQRefV);
  FblcVectorInit(arena, *(imported->paramv));
  for (size_t i = 0; i < qref->paramv->size; ++i) {
    FbldQRef* p = FbldImportQRef(arena, src, qref->paramv->xs[i]);
    FblcVectorAppend(arena, *(imported->paramv), p);
  }
  imported->mref = FbldImportQRef(arena, src, qref->r->mref);

  FbldR* r = FBLC_ALLOC(arena, FbldR);
  r->decl = qref->r->decl;
  r->mref = imported->mref;
  r->param = false;
  r->interf = qref->r->interf;
  imported->r = r;
  return imported;
}
