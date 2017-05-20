// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <string.h>   // for strcmp

#include "fblc.h"
#include "fbld.h"

// CompiledDecl --
//   A single compiled fblc declaration.
//
// Fields:
//   name - The name of the compiled declaration.
//   decl - The compiled declaration.
typedef struct {
  FbldName name;
  FblcDecl* decl;
} CompiledDecl;

// CompiledDeclV --
//   A vector of compiled declarations.
typedef struct {
  size_t size;
  CompiledDecl** xs;
} CompiledDeclV;

// CompiledModule --
//   A collection of declarations compiled so far for a given module.
//
// Fields:
//   name - The name of the module the code is for.
//   declv - The declarations compiled for the module so far, in no particular
//           order.
typedef struct {
  FbldName name;
  CompiledDeclV declv;
} CompiledModule;

// CompiledModuleV --
//   A vector of codes used to store all compiled declarations for a set of
//   modules.
typedef struct {
  size_t size;
  CompiledModule** xs;
} CompiledModuleV;

static FblcExpr* CompileExpr(FblcArena* arena, FbldMDefnV* mdefnv, CompiledModuleV* codev, FbldMDefn* mctx, CompiledModule* cctx, FbldExpr* expr);
static FblcDecl* CompileDecl(FblcArena* arena, FbldMDefnV* mdefnv, CompiledModuleV* codev, FbldMDefn* mctx, CompiledModule* cctx, FbldQualifiedName* entity);

// CompileExpr --
//   Compile the given fbld expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   mdefnv - vector of module definitions in which to find the entities to compile
//   codev - collection of entities that have already been compiled.
//   mctx - The module definition to use as the context for unqualified entities.
//   cctx - The code already compiled for the mctx module.
//   expr - the fbld expression to compile.
//   TODO: Add context for tracking variable ids.
//
// Result:
//   An fblc expression compiled from the given fbld expression.
//
// Side effects:
//   Compiles and updates codev with the compiled code for every other entity
//   the expression depends on that has not already been compiled.
//
//   The behavior is undefined if the modules or expression is not well
//   formed.
//
// Allocations:
//   The caller is responsible for tracking the allocations through the arena
//   and freeing them when no longer needed. This function will allocate
//   memory proportional to the size of the expression and all declarations
//   compiled.
static FblcExpr* CompileExpr(FblcArena* arena, FbldMDefnV* mdefnv, CompiledModuleV* codev, FbldMDefn* mctx, CompiledModule* cctx, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR:
      assert(false && "TODO: Compile var expr");
      return NULL;

    case FBLC_APP_EXPR: {
      FbldAppExpr* source = (FbldAppExpr*)expr;
      FblcAppExpr* app_expr = arena->alloc(arena, sizeof(FblcAppExpr));
      app_expr->_base.tag = FBLC_APP_EXPR;
      app_expr->_base.id = 0xDEAD;    // unused
      app_expr->func = CompileDecl(arena, mdefnv, codev, mctx, cctx, source->func);
      FblcVectorInit(arena, app_expr->argv);
      for (size_t i = 0; i < source->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, mdefnv, codev, mctx, cctx, source->argv->xs[i]);
        FblcVectorAppend(arena, app_expr->argv, arg);
      }
      return &app_expr->_base;
    }

    case FBLC_UNION_EXPR:
      assert(false && "TODO: Compile union expr");
      return NULL;

    case FBLC_ACCESS_EXPR:
      assert(false && "TODO: Compile access expr");
      return NULL;

    case FBLC_COND_EXPR:
      assert(false && "TODO: Compile cond expr");
      return NULL;

    case FBLC_LET_EXPR:
      assert(false && "TODO: Compile let expr");
      return NULL;

    default:
      assert(false && "Invalid expr tag");
      return NULL;
  }
}

// CompileDecl --
//   Compile as needed and return the fblc declaration for the named fbld
//   entity.
//
// Inputs:
//   arena - arena to use for allocations.
//   mdefnv - vector of module definitions in which to find the entity to compile
//   codev - collection of entities that have already been compiled.
//   mctx - The module definition to use as the context for unqualified entities.
//          May be NULL only if the entity is explicitly qualified.
//   cctx - The code already compiled for the mctx module.
//          May be NULL only if the entity is explicitly qualified.
//   entity - the name of the entity to compile.
//
// Result:
//   An fblc declaration compiled from the fbld declaration of the named
//   entity.
//
// Side effects:
//   Compiles and updates codev with the compiled code for the named entity
//   and every other entity that depends on that has not already been
//   compiled.
//
//   The behavior is undefined if the modules are not well formed or the
//   entity name does not refer to a valid entity.
//
// Allocations:
//   The caller is responsible for tracking the allocations through the arena
//   and freeing them when no longer needed. This function will allocate
//   memory proportional to the size of all declarations compiled.
static FblcDecl* CompileDecl(FblcArena* arena, FbldMDefnV* mdefnv, CompiledModuleV* codev, FbldMDefn* mctx, CompiledModule* cctx, FbldQualifiedName* entity)
{
  // Find the module and code context for the entity.
  if (entity->module != NULL) {
    mctx = NULL;
    for (size_t i = 0; i < mdefnv->size; ++i) {
      if (strcmp(mdefnv->xs[i]->name->name, entity->module->name) == 0) {
        mctx = mdefnv->xs[i];
        break;
      }
    }

    cctx = NULL;
    for (size_t i = 0; i < codev->size; ++i) {
      if (strcmp(codev->xs[i]->name, entity->module->name) == 0) {
        cctx = codev->xs[i];
        break;
      }
    }

    if (cctx == NULL) {
      cctx = arena->alloc(arena, sizeof(CompiledModule));
      cctx->name = entity->module->name;
      FblcVectorInit(arena, cctx->declv);
      FblcVectorAppend(arena, *codev, cctx);
    }
  }
  assert(mctx != NULL && "Failed to find module for entity");
  assert(cctx != NULL && "Code context not provided for unqualified entity");

  // Check if the entity has already been compiled.
  for (size_t i = 0; i < cctx->declv.size; ++i) {
    if (strcmp(cctx->declv.xs[i]->name, entity->name->name) == 0) {
      return cctx->declv.xs[i]->decl;
    }
  }

  // Find the fbld definition of the entity.
  FbldDefn* defn = NULL;
  for (size_t i = 0; i < mctx->defns->size; ++i) {
    if (strcmp(mctx->defns->xs[i]->decl->name->name, entity->name->name) == 0) {
      defn = mctx->defns->xs[i];
      break;
    }
  }
  assert(defn != NULL && "Entiry definition not found");

  FblcDecl* decl = NULL;
  switch (defn->decl->tag) {
    case FBLD_IMPORT_DECL:
      assert(false && "Cannot compile an import declaration");
      break;

    case FBLD_TYPE_DECL:
      assert(false && "Cannot compile a abstract type declaration");
      break;

    case FBLD_UNION_DECL:
      assert(false && "TODO: Compile a union declaration");
      break;

    case FBLD_STRUCT_DECL: {
      FbldStructDefn* struct_defn = (FbldStructDefn*)defn;
      FblcStructDecl* struct_decl = arena->alloc(arena, sizeof(FblcStructDecl));
      struct_decl->_base.tag = FBLC_STRUCT_DECL;
      struct_decl->_base.id = 0xDEAD;   // id field unused
      FblcVectorInit(arena, struct_decl->fieldv);
      for (size_t i = 0; i < struct_defn->decl->fieldv->size; ++i) {
        FblcTypeDecl* type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, mctx, cctx,
            struct_defn->decl->fieldv->xs[i]->type);
        FblcVectorAppend(arena, struct_decl->fieldv, type);
      }
      decl = &struct_decl->_base;
      break;
    }

    case FBLD_FUNC_DECL: {
      FbldFuncDefn* func_defn = (FbldFuncDefn*)defn;
      FblcFuncDecl* func_decl = arena->alloc(arena, sizeof(FblcFuncDecl));
      func_decl->_base.tag = FBLC_FUNC_DECL;
      func_decl->_base.id = 0xDEAD;     // id field unused
      FblcVectorInit(arena, func_decl->argv);
      for (size_t i = 0; i < func_defn->decl->argv->size; ++i) {
        FblcTypeDecl* arg = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, mctx, cctx,
            func_defn->decl->argv->xs[i]->type);
        FblcVectorAppend(arena, func_decl->argv, arg);
      }
      func_decl->return_type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, mctx, cctx,
            func_defn->decl->return_type);
      func_decl->body = CompileExpr(
            arena, mdefnv, codev, mctx, cctx, func_defn->body);
      decl = &func_decl->_base;
      break;
    }

    case FBLD_PROC_DECL:
      assert(false && "TODO: Compile a proc declaration");
      break;

    default:
      assert(false && "Invalid declaration tag.");
      break;
  }

  assert(decl != NULL);
  CompiledDecl* c = arena->alloc(arena, sizeof(CompiledDecl));
  c->name = entity->name->name;
  c->decl = decl;
  FblcVectorAppend(arena, cctx->declv, c);
  return decl;
}

// FbldCompile -- see documentation in fbld.h
FblcDecl* FbldCompile(FblcArena* arena, FbldMDefnV* mdefnv, FbldQualifiedName* entity)
{
  CompiledModuleV codev;
  FblcVectorInit(arena, codev);
  FblcDecl* compiled = CompileDecl(arena, mdefnv, &codev, NULL, NULL, entity);
  arena->free(arena, codev.xs);
  return compiled;
}
