// program.c --
//   This file implements routines for working with loaded fbld programs.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fblc.h"
#include "fbld.h"

// FbldLookupModule -- see documentation in fbld.h
FbldModule* FbldLookupModule(FbldProgram* prgm, FbldName* name)
{
  assert(false && "TODO");
//  for (size_t i = 0; i < prgm->mheaderv.size; ++i) {
//    FbldModule* module = prgm->mheaderv.xs[i];
//    if (FbldNamesEqual(name->name, module->name->name)) {
//      return module;
//    }
//  }
  return NULL;
}

// FbldLookupType -- see documentation in fbld.h
FbldType* FbldLookupType(FbldProgram* prgm, FbldQRef* entity)
{
  assert(false && "TODO");
//  FbldModule* module = FbldLookupModule(prgm, entity->rmref->name);
//  if (module == NULL) {
//    return NULL;
//  }
//
//  for (size_t j = 0; j < module->typev->size; ++j) {
//    FbldType* type = module->typev->xs[j];
//    if (FbldNamesEqual(entity->rname->name, type->name->name)) {
//      return type;
//    }
//  }
  return NULL;
}

// FbldLookupFunc -- see documentation in fbld.h
FbldFunc* FbldLookupFunc(FbldProgram* prgm, FbldQRef* entity)
{
  assert(false && "TODO");
//  FbldModule* module = FbldLookupModule(prgm, entity->rmref->name);
//  if (module == NULL) {
//    return NULL;
//  }
//
//  for (size_t j = 0; j < module->funcv->size; ++j) {
//    FbldFunc* func = module->funcv->xs[j];
//    if (FbldNamesEqual(entity->rname->name, func->name->name)) {
//      return func;
//    }
//  }
  return NULL;
}

// FbldLookupProc -- see documentation in fbld.h
FbldProc* FbldLookupProc(FbldProgram* prgm, FbldQRef* entity)
{
  assert(false && "TODO");
//  FbldModule* module = FbldLookupModule(prgm, entity->rmref->name);
//  if (module == NULL) {
//    return NULL;
//  }
//
//  for (size_t j = 0; j < module->procv->size; ++j) {
//    FbldProc* proc = module->procv->xs[j];
//    if (FbldNamesEqual(entity->rname->name, proc->name->name)) {
//      return proc;
//    }
//  }
  return NULL;
}

// FbldImportQRef -- See documentation in fbld.h.
FbldQRef* FbldImportQRef(FblcArena* arena, FbldProgram* prgm, FbldQRef* ctx, FbldQRef* qref)
{
  assert(false && "TODO");
//  FbldModule* module = FbldLookupModule(prgm, ctx->name);
//
//  if (entity->rmref == NULL) {
//    // Check to see if this is a type parameter.
//    for (size_t i = 0; i < module->targv->size; ++i) {
//      if (FbldNamesEqual(entity->rname->name, module->targv->xs[i]->name)) {
//        return ctx->targv->xs[i];
//      }
//    }
//  }
//
//  FbldQRef* resolved = FBLC_ALLOC(arena, FbldQRef);
//  resolved->uname = entity->rname;
//  resolved->umref = FbldImportMRef(arena, prgm, ctx, entity->rmref);
//  resolved->rname = resolved->uname;
//  resolved->rmref = resolved->umref;
//  return resolved;
  return NULL;
}

//// FbldImportMRef -- See documentation in fbld.h
//FbldMRef* FbldImportMRef(FblcArena* arena, FbldProgram* prgm, FbldMRef* ctx, FbldMRef* mref)
//{
//  if (mref == NULL) {
//    // This must be a locally defined entity.
//    return ctx;
//  }
//
//  if (mref->targv == NULL) {
//    // This must be a module parameter.
//    FbldModule* module = FbldLookupModule(prgm, ctx->name);
//    for (size_t i = 0; i < module->margv->size; ++i) {
//      if (FbldNamesEqual(mref->name->name, module->margv->xs[i]->name->name)) {
//        return ctx->margv->xs[i];
//      }
//    }
//    return NULL;
//  }
//
//  FbldMRef* resolved = FBLC_ALLOC(arena, FbldMRef);
//  resolved->name = mref->name;
//  resolved->targv = FBLC_ALLOC(arena, FbldQRefV);
//  FblcVectorInit(arena, *resolved->targv);
//  for (size_t i = 0; i < mref->targv->size; ++i) {
//    FbldQRef* targ = FbldImportQRef(arena, prgm, ctx, mref->targv->xs[i]);
//    FblcVectorAppend(arena, *resolved->targv, targ);
//  }
//
//  resolved->margv = FBLC_ALLOC(arena, FbldMRefV);
//  FblcVectorInit(arena, *resolved->margv);
//  for (size_t i = 0; i < mref->margv->size; ++i) {
//    FbldMRef* marg = FbldImportMRef(arena, prgm, ctx, mref->margv->xs[i]);
//    FblcVectorAppend(arena, *resolved->margv, marg);
//  }
//  return resolved;
// }

// FbldPrintQRef -- see documentation in fbld.h
void FbldPrintQRef(FILE* stream, FbldQRef* qref)
{
  assert(false && "TODO");
//  fprintf(stream, "%s", type->rname->name);
//  if (type->rmref != NULL) {
//    fprintf(stream, "@");
//    FbldPrintMRef(stream, type->rmref);
//  }
}

//// FbldPrintMRef -- see documentation in fbld.h
//void FbldPrintMRef(FILE* stream, FbldMRef* mref)
//{
//  fprintf(stream, "%s<", mref->name->name);
//
//  if (mref->targv != NULL) {
//    for (size_t i = 0; i < mref->targv->size; ++i) {
//      if (i > 0) {
//        fprintf(stream, ",");
//      }
//      FbldPrintType(stream, mref->targv->xs[i]);
//    }
//    fprintf(stream, ";");
//
//    for (size_t i = 0; i < mref->margv->size; ++i) {
//      if (i > 0) {
//        fprintf(stream, ",");
//      }
//      FbldPrintMRef(stream, mref->margv->xs[i]);
//    }
//  }
//  fprintf(stream, ">");
//}
