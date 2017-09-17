// program.c --
//   This file implements routines for working with loaded fbld programs.

#include <stdlib.h>   // for NULL

#include "fblc.h"
#include "fbld.h"

// FbldLookupMDefn -- see documentation in fbld.h
FbldMDefn* FbldLookupMDefn(FbldProgram* prgm, FbldName* name)
{
  for (size_t i = 0; i < prgm->mdeclv.size; ++i) {
    FbldMDefn* mdefn = prgm->mdeclv.xs[i];
    if (FbldNamesEqual(name->name, mdefn->name->name)) {
      return mdefn;
    }
  }
  return NULL;
}

// FbldLookupType -- see documentation in fbld.h
FbldType* FbldLookupType(FbldProgram* prgm, FbldQName* entity)
{
  FbldMDefn* mdefn = FbldLookupMDefn(prgm, entity->mref->name);
  if (mdefn == NULL) {
    return NULL;
  }

  for (size_t j = 0; j < mdefn->typev->size; ++j) {
    FbldType* type = mdefn->typev->xs[j];
    if (FbldNamesEqual(entity->name->name, type->name->name)) {
      return type;
    }
  }
  return NULL;
}

// FbldLookupFunc -- see documentation in fbld.h
FbldFunc* FbldLookupFunc(FbldProgram* prgm, FbldQName* entity)
{
  FbldMDefn* mdefn = FbldLookupMDefn(prgm, entity->mref->name);
  if (mdefn == NULL) {
    return NULL;
  }

  for (size_t j = 0; j < mdefn->funcv->size; ++j) {
    FbldFunc* func = mdefn->funcv->xs[j];
    if (FbldNamesEqual(entity->name->name, func->name->name)) {
      return func;
    }
  }
  return NULL;
}

// FbldImportQName -- See documentation in fbld.h.
FbldQName* FbldImportQName(FblcArena* arena, FbldProgram* prgm, FbldMRef* ctx, FbldQName* entity)
{
  FbldMDefn* mdefn = FbldLookupMDefn(prgm, ctx->name);

  if (entity->mref == NULL) {
    // Check to see if this is a type parameter.
    for (size_t i = 0; i < mdefn->targs->size; ++i) {
      if (FbldNamesEqual(entity->name->name, mdefn->targs->xs[i]->name)) {
        return ctx->targs->xs[i];
      }
    }
  }

  FbldQName* resolved = FBLC_ALLOC(arena, FbldQName);
  resolved->name = entity->name;
  resolved->mref = FbldImportMRef(arena, prgm, ctx, entity->mref);
  return resolved;
}

// FbldImportMRef -- See documentation in fbld.h
FbldMRef* FbldImportMRef(FblcArena* arena, FbldProgram* prgm, FbldMRef* ctx, FbldMRef* mref)
{
  if (mref == NULL) {
    // This must be a locally defined entity.
    return ctx;
  }

  if (mref->targs == NULL) {
    // This must be a module parameter.
    FbldMDefn* mdefn = FbldLookupMDefn(prgm, ctx->name);
    for (size_t i = 0; i < mdefn->margs->size; ++i) {
      if (FbldNamesEqual(mref->name->name, mdefn->margs->xs[i]->name->name)) {
        return ctx->margs->xs[i];
      }
    }
    return NULL;
  }

  FbldMRef* resolved = FBLC_ALLOC(arena, FbldMRef);
  resolved->name = mref->name;
  resolved->targs = FBLC_ALLOC(arena, FbldQNameV);
  FblcVectorInit(arena, *resolved->targs);
  for (size_t i = 0; i < mref->targs->size; ++i) {
    FbldQName* targ = FbldImportQName(arena, prgm, ctx, mref->targs->xs[i]);
    FblcVectorAppend(arena, *resolved->targs, targ);
  }

  resolved->margs = FBLC_ALLOC(arena, FbldMRefV);
  FblcVectorInit(arena, *resolved->margs);
  for (size_t i = 0; i < mref->margs->size; ++i) {
    FbldMRef* marg = FbldImportMRef(arena, prgm, ctx, mref->margs->xs[i]);
    FblcVectorAppend(arena, *resolved->margs, marg);
  }
  return resolved;
}
