// decl.c --
//   This file implements utility routines for manipulating fbld decls.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fbld.h"


// FbldLookupDecl -- see documentation in fbld.h
FbldDecl* FbldLookupDecl(FbldModuleV* env, FbldMDefn* mdefn, FbldQualifiedName* entity)
{
  assert(entity->module->name != NULL);

  if (mdefn == NULL || !FbldNamesEqual(mdefn->name->name, entity->module->name)) {
    for (size_t i = 0; i < env->size; ++i) {
      if (FbldNamesEqual(env->xs[i]->name->name, entity->module->name)) {
        mdefn = env->xs[i];
        break;
      }
    }
  }

  if (mdefn != NULL) {
    for (size_t i = 0; i < mdefn->declv->size; ++i) {
      if (mdefn->declv->xs[i]->tag != FBLD_IMPORT_DECL
          && FbldNamesEqual(mdefn->declv->xs[i]->name->name, entity->name->name)) {
        return mdefn->declv->xs[i];
      }
    }
  }
  return NULL;
}
