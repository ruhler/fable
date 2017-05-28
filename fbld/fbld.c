// fbld.c --
//   This file implements utility routines for manipulating fbld programs.

#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr
#include <string.h>   // for strcmp

#include "fbld.h"


// FbldReportError -- see documentation in fbld.h
void FbldReportError(const char* format, FbldLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// FbldResolveModule -- see documentation in fbld.h
FbldName FbldResolveModule(FbldMDefn* mctx, FbldQualifiedName* entity)
{
  // If the entity is explicitly qualified, use the named module.
  if (entity->module != NULL) {
    return entity->module->name;
  }

  // Check if the entity has been imported.
  for (size_t i = 0; i < mctx->declv->size; ++i) {
    if (mctx->declv->xs[i]->tag == FBLD_IMPORT_DECL) {
      FbldImportDecl* decl = (FbldImportDecl*)mctx->declv->xs[i];
      for (size_t j = 0; j < decl->namev->size; ++j) {
        if (strcmp(entity->name->name, decl->namev->xs[j]->name) == 0) {
          return decl->_base.name->name;
        }
      }
    }
  }

  // Otherwise use the local module.
  if (mctx != NULL) {
    return mctx->name->name;
  }

  return NULL;
}

// FbldLookupMDefn -- see documentation in fbld.h
FbldMDefn* FbldLookupMDefn(FbldMDefnV* mdefnv, FbldName name)
{
  for (size_t i = 0; i < mdefnv->size; ++i) {
    if (strcmp(mdefnv->xs[i]->name->name, name) == 0) {
      return mdefnv->xs[i];
    }
  }
  return NULL;
}

// FbldLookupDecl -- see documentation in fbld.h
FbldDecl* FbldLookupDecl(FbldMDefn* mdefn, FbldNameL* name)
{
  for (size_t i = 0; i < mdefn->declv->size; ++i) {
    if (strcmp(mdefn->declv->xs[i]->name->name, name->name) == 0) {
      return mdefn->declv->xs[i];
    }
  }
  return NULL;
}

// FbldLookupQDecl -- see documentation in fbld.h
FbldDecl* FbldLookupQDecl(FbldMDefnV* env, FbldMDefn* mctx, FbldQualifiedName* entity)
{
  FbldName module = FbldResolveModule(mctx, entity);
  if (module == NULL) {
    return NULL;
  }

  FbldMDefn* mdefn = NULL;
  if (mctx != NULL && strcmp(mctx->name->name, module) == 0) {
    mdefn = mctx;
  } else {
    mdefn = FbldLookupMDefn(env, module);
  }

  if (mdefn == NULL) {
    return NULL;
  }
  return FbldLookupDecl(mdefn, entity->name);
}
