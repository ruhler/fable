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

// FbldImportQRef -- see documentation in fbld.h
FbldQRef* FbldImportQRef(FblcArena* arena, FbldQRef* src, FbldQRef* qref)
{
  // The qref should already have been resolved by this point, otherwise
  // something has gone wrong.
  assert(qref->r != NULL);
  switch (qref->r->tag) {
    case FBLD_FAILED_R: {
      assert(false && "ForeignType with failed src?");
      return NULL;
    }

    case FBLD_ENTITY_R: {
      FbldEntityR* entity = (FbldEntityR*)qref->r;
      if (entity->mref == NULL) {
        // This is a top level declaration.
        return qref;
      }

      // TODO: Avoid allocation if the foreign module is the same?
      FbldEntityR* ient = FBLC_ALLOC(arena, FbldEntityR);
      ient->_base.tag = FBLD_ENTITY_R;
      ient->decl = entity->decl;
      ient->mref = FbldImportQRef(arena, src, entity->mref);
      ient->source = entity->source;

      FbldQRef* imported = FBLC_ALLOC(arena, FbldQRef);
      imported->name = qref->name;
      imported->targv = FBLC_ALLOC(arena, FbldQRefV);
      FblcVectorInit(arena, *(imported->targv));
      for (size_t i = 0; i < qref->targv->size; ++i) {
        FbldQRef* p = FbldImportQRef(arena, src, qref->targv->xs[i]);
        FblcVectorAppend(arena, *(imported->targv), p);
      }

      imported->margv = FBLC_ALLOC(arena, FbldQRefV);
      FblcVectorInit(arena, *(imported->margv));
      for (size_t i = 0; i < qref->margv->size; ++i) {
        FbldQRef* p = FbldImportQRef(arena, src, qref->margv->xs[i]);
        FblcVectorAppend(arena, *(imported->margv), p);
      }

      imported->mref = ient->mref;
      imported->r = &ient->_base;
      return imported;
    }

    case FBLD_PARAM_R: {
      FbldParamR* param = (FbldParamR*)qref->r;

      FbldQRef* qdecl = src;
      assert(qdecl->r->tag == FBLD_ENTITY_R);
      FbldEntityR* qent = (FbldEntityR*)qdecl->r;
      while (true) {
        if (param->index == FBLD_INTERF_PARAM_INDEX) {
          if (qent->decl->tag == FBLD_MODULE_DECL) {
            FbldModule* module = (FbldModule*)qent->decl;
            assert(module->iref->r->tag == FBLD_ENTITY_R);
            FbldEntityR* ient = (FbldEntityR*)module->iref->r;
            assert(ient->decl->tag == FBLD_INTERF_DECL);
            if (ient->decl == param->decl) {
              return qdecl;
            }
          }
        } else {
          // TODO: Is it safe to match by name here instead of decl?
          // Using decl has the problem of considering the interf and module
          // versions of the same declaration as different.
          if (FbldNamesEqual(qent->decl->name->name, param->decl->name->name)) {
            if (param->interf == NULL) {
              assert(param->index < qdecl->targv->size);
              return qdecl->targv->xs[param->index];
            }
            assert(param->index < qdecl->margv->size);
            return qdecl->margv->xs[param->index];
          }
        }

        // We didn't find a match. Continue up the hierarchy.
        FbldEntitySource source = qent->source;
        qdecl = qent->mref;
        assert(qdecl != NULL && "Failed to match param");
        assert(qdecl->r->tag == FBLD_ENTITY_R);
        qent = (FbldEntityR*)qdecl->r;
        if (param->index != FBLD_INTERF_PARAM_INDEX && source == FBLD_INTERF_SOURCE) {
          assert(qent->decl->tag == FBLD_MODULE_DECL);
          FbldModule* mmod = (FbldModule*)qent->decl;
          qdecl = mmod->iref;
          assert(qdecl->r->tag == FBLD_ENTITY_R);
          qent = (FbldEntityR*)qdecl->r;
        }
      }
      assert(false && "Infinite loop terminated?");
    }

    default: {
      assert(false && "Invalid R tag");
      return NULL;
    }
  }
}
