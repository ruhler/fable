// fbld.c --
//   This file implements utility routines for manipulating fbld programs.

#include <assert.h>   // for assert
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
    if (mdefn->declv->xs[i]->tag != FBLD_IMPORT_DECL
        && strcmp(mdefn->declv->xs[i]->name->name, name->name) == 0) {
      return mdefn->declv->xs[i];
    }
  }
  return NULL;
}

// FbldLookupQDecl -- see documentation in fbld.h
FbldDecl* FbldLookupQDecl(FbldModuleV* env, FbldMDefn* mdefn, FbldQualifiedName* entity)
{
  assert(entity->module->name != NULL);

  if (mdefn == NULL || strcmp(mdefn->name->name, entity->module->name) != 0) {
    mdefn = FbldLookupMDefn(env, entity->module->name);
  }
  return mdefn == NULL ? NULL : FbldLookupDecl(mdefn, entity->name);
}
