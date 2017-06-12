// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf

#include "fblc.h"
#include "fbld.h"

// CompiledDecl --
//   A single compiled fblc declaration.
//
// Fields:
//   module - The module the declaration was defined in.
//   name - The name of the compiled declaration.
//   decl - The compiled declaration.
typedef struct {
  FbldName module;
  FbldName name;
  FblcDecl* decl;
} CompiledDecl;

// CompiledDeclV --
//   A vector of compiled declarations.
typedef struct {
  size_t size;
  CompiledDecl** xs;
} CompiledDeclV;

static FblcExpr* CompileExpr(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldAccessLocV* accessv, FbldExpr* expr);
static FblcDecl* CompileDecl(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldAccessLocV* accessv, FbldQualifiedName* entity);

// CompileExpr --
//   Compile the given fbld expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   mdefnv - vector of module definitions in which to find the entities to compile
//   codev - collection of declarations that have already been compiled.
//   accessv - collection of access expression locations.
//   expr - the fbld expression to compile.
//
// Result:
//   The compiled expression.
//
// Side effects:
//   Compiles and updates codev with the compiled code for this and every
//   other entity the expression depends on that has not already been
//   compiled.
//   Updates accessv with the location of any newly compiled access
//   expressions.
//
//   The behavior is undefined if the modules or expression is not well
//   formed.
//
// Allocations:
//   The caller is responsible for tracking the allocations through the arena
//   and freeing them when no longer needed. This function will allocate
//   memory proportional to the size of the expression and all declarations
//   compiled.
static FblcExpr* CompileExpr(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldAccessLocV* accessv, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FbldVarExpr* var_dexpr = (FbldVarExpr*)expr;
      FblcVarExpr* var_cexpr = arena->alloc(arena, sizeof(FblcVarExpr));
      var_cexpr->_base.tag = FBLC_VAR_EXPR;
      var_cexpr->_base.id = 0xDEAD;    // unused
      var_cexpr->var = var_dexpr->id;
      assert(var_cexpr->var != FBLC_NULL_ID);
      return &var_cexpr->_base;
    }

    case FBLC_APP_EXPR: {
      FbldAppExpr* app_dexpr = (FbldAppExpr*)expr;
      FblcAppExpr* app_cexpr = arena->alloc(arena, sizeof(FblcAppExpr));
      app_cexpr->_base.tag = FBLC_APP_EXPR;
      app_cexpr->_base.id = 0xDEAD;    // unused
      app_cexpr->func = CompileDecl(arena, mdefnv, codev, accessv, app_dexpr->func);
      FblcVectorInit(arena, app_cexpr->argv);
      for (size_t i = 0; i < app_dexpr->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, mdefnv, codev, accessv, app_dexpr->argv->xs[i]);
        FblcVectorAppend(arena, app_cexpr->argv, arg);
      }
      return &app_cexpr->_base;
    }

    case FBLC_UNION_EXPR: {
      FbldUnionExpr* union_dexpr = (FbldUnionExpr*)expr;
      FblcUnionExpr* union_cexpr = arena->alloc(arena, sizeof(FblcUnionExpr));
      union_cexpr->_base.tag = FBLC_UNION_EXPR;
      union_cexpr->_base.id = 0xDEAD;  // unused
      union_cexpr->type = (FblcTypeDecl*)CompileDecl(arena, mdefnv, codev, accessv, union_dexpr->type);
      union_cexpr->field = union_dexpr->field.id;
      assert(union_cexpr->field != FBLC_NULL_ID);
      union_cexpr->arg = CompileExpr(arena, mdefnv, codev, accessv, union_dexpr->arg);
      return &union_cexpr->_base;
   }

    case FBLC_ACCESS_EXPR: {
      FbldAccessExpr* access_dexpr = (FbldAccessExpr*)expr;
      FblcAccessExpr* access_cexpr = arena->alloc(arena, sizeof(FblcAccessExpr));
      access_cexpr->_base.tag = FBLC_ACCESS_EXPR;
      access_cexpr->_base.id = 0xDEAD;   // unused
      access_cexpr->obj = CompileExpr(arena, mdefnv, codev, accessv, access_dexpr->obj);
      access_cexpr->field = access_dexpr->field.id;
      assert(access_cexpr->field != FBLC_NULL_ID);
      FbldAccessLoc* access_loc = FblcVectorExtend(arena, *accessv);
      access_loc->expr = &access_cexpr->_base;
      access_loc->loc = expr->loc;
      return &access_cexpr->_base;
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* cond_dexpr = (FbldCondExpr*)expr;
      FblcCondExpr* cond_cexpr = arena->alloc(arena, sizeof(FblcCondExpr));
      cond_cexpr->_base.tag = FBLC_COND_EXPR;
      cond_cexpr->_base.id = 0xDEAD;   // unused
      cond_cexpr->select = CompileExpr(arena, mdefnv, codev, accessv, cond_dexpr->select);
      FblcVectorInit(arena, cond_cexpr->argv);
      for (size_t i = 0; i < cond_dexpr->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, mdefnv, codev, accessv, cond_dexpr->argv->xs[i]);
        FblcVectorAppend(arena, cond_cexpr->argv, arg);
      }
      return &cond_cexpr->_base;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* let_dexpr = (FbldLetExpr*)expr;
      FblcLetExpr* let_cexpr = arena->alloc(arena, sizeof(FblcLetExpr));
      let_cexpr->_base.tag = FBLC_LET_EXPR;
      let_cexpr->_base.id = 0xDEAD;  // unused
      let_cexpr->type = (FblcTypeDecl*)CompileDecl(arena, mdefnv, codev, accessv, let_dexpr->type);
      let_cexpr->def = CompileExpr(arena, mdefnv, codev, accessv, let_dexpr->def);
      let_cexpr->body = CompileExpr(arena, mdefnv, codev, accessv, let_dexpr->body);
      return &let_cexpr->_base;
    }

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
//   codev - collection of declarations that have already been compiled.
//   accessv - collection of access expression locations.
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
//   Updates accessv with the location of any newly compiled access
//   expressions.
//
//   The behavior is undefined if the modules are not well formed or the
//   entity name does not refer to a valid entity.
//
// Allocations:
//   The caller is responsible for tracking the allocations through the arena
//   and freeing them when no longer needed. This function will allocate
//   memory proportional to the size of all declarations compiled.
static FblcDecl* CompileDecl(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldAccessLocV* accessv, FbldQualifiedName* entity)
{
  // Check if the entity has already been compiled.
  for (size_t i = 0; i < codev->size; ++i) {
    if (FbldNamesEqual(codev->xs[i]->module, entity->module->name)
        && FbldNamesEqual(codev->xs[i]->name, entity->name->name)) {
      return codev->xs[i]->decl;
    }
  }

  // Find the fbld definition of the entity.
  FbldDecl* ddecl = FbldLookupDecl(mdefnv, NULL, entity);
  assert(ddecl != NULL && "Entry definition not found");

  // Compile the declaration
  CompiledDecl* c = arena->alloc(arena, sizeof(CompiledDecl));
  c->module = entity->module->name;
  c->name = entity->name->name;
  c->decl = NULL;
  FblcVectorAppend(arena, *codev, c);
  switch (ddecl->tag) {
    case FBLD_IMPORT_DECL:
      assert(false && "Cannot compile an import declaration");
      break;

    case FBLD_ABSTRACT_TYPE_DECL:
      assert(false && "Cannot compile a abstract type declaration");
      break;

    case FBLD_UNION_DECL: {
      FbldUnionDecl* union_ddecl = (FbldUnionDecl*)ddecl;
      FblcUnionDecl* union_cdecl = arena->alloc(arena, sizeof(FblcUnionDecl));
      c->decl = &union_cdecl->_base;
      union_cdecl->_base.tag = FBLC_UNION_DECL;
      union_cdecl->_base.id = 0xDEAD;   // id field unused
      FblcVectorInit(arena, union_cdecl->fieldv);
      for (size_t i = 0; i < union_ddecl->fieldv->size; ++i) {
        FblcTypeDecl* type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            union_ddecl->fieldv->xs[i]->type);
        FblcVectorAppend(arena, union_cdecl->fieldv, type);
      }
      break;
    }

    case FBLD_STRUCT_DECL: {
      FbldStructDecl* struct_ddecl = (FbldStructDecl*)ddecl;
      FblcStructDecl* struct_cdecl = arena->alloc(arena, sizeof(FblcStructDecl));
      c->decl = &struct_cdecl->_base;
      struct_cdecl->_base.tag = FBLC_STRUCT_DECL;
      struct_cdecl->_base.id = 0xDEAD;   // id field unused
      FblcVectorInit(arena, struct_cdecl->fieldv);
      for (size_t i = 0; i < struct_ddecl->fieldv->size; ++i) {
        FblcTypeDecl* type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            struct_ddecl->fieldv->xs[i]->type);
        FblcVectorAppend(arena, struct_cdecl->fieldv, type);
      }
      break;
    }

    case FBLD_FUNC_DECL: {
      FbldFuncDecl* func_ddecl = (FbldFuncDecl*)ddecl;
      FblcFuncDecl* func_cdecl = arena->alloc(arena, sizeof(FblcFuncDecl));
      c->decl = &func_cdecl->_base;
      func_cdecl->_base.tag = FBLC_FUNC_DECL;
      func_cdecl->_base.id = 0xDEAD;     // id field unused
      FblcVectorInit(arena, func_cdecl->argv);
      for (size_t i = 0; i < func_ddecl->argv->size; ++i) {
        FblcTypeDecl* arg = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            func_ddecl->argv->xs[i]->type);
        FblcVectorAppend(arena, func_cdecl->argv, arg);
      }
      func_cdecl->return_type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            func_ddecl->return_type);
      func_cdecl->body = CompileExpr(arena, mdefnv, codev, accessv, func_ddecl->body);
      break;
    }

    case FBLD_PROC_DECL:
      assert(false && "TODO: Compile a proc declaration");
      break;

    default:
      assert(false && "Invalid declaration tag.");
      break;
  }

  assert(c->decl != NULL);
  return c->decl;
}

// FbldCompile -- see documentation in fbld.h
FblcDecl* FbldCompile(FblcArena* arena, FbldAccessLocV* accessv, FbldMDefnV* mdefnv, FbldQualifiedName* entity)
{
  CompiledDeclV codev;
  FblcVectorInit(arena, codev);
  FblcDecl* compiled = CompileDecl(arena, mdefnv, &codev, accessv, entity);
  arena->free(arena, codev.xs);
  return compiled;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldMDefnV* mdefnv, FbldValue* value)
{
  FbldDecl* type_decl = FbldLookupDecl(mdefnv, NULL, value->type);
  assert(type_decl != NULL);
  switch (value->kind) {
    case FBLD_STRUCT_KIND: {
      FbldStructDecl* struct_decl = (FbldStructDecl*)type_decl;
      size_t fieldc = struct_decl->fieldv->size;
      FblcValue* struct_value = FblcNewStruct(arena, fieldc);
      for (size_t i = 0; i < fieldc; ++i) {
        struct_value->fields[i] = FbldCompileValue(arena, mdefnv, value->fieldv->xs[i]);
      }
      return struct_value;
    }

    case FBLD_UNION_KIND: {
      FbldUnionDecl* union_decl = (FbldUnionDecl*)type_decl;
      FblcValue* arg = FbldCompileValue(arena, mdefnv, value->fieldv->xs[0]);
      size_t tag = FBLC_NULL_ID;
      for (size_t i = 0; i < union_decl->fieldv->size; ++i) {
        if (FbldNamesEqual(union_decl->fieldv->xs[i]->name->name, value->tag->name)) {
          tag = i;
          break;
        }
      }
      assert(tag != FBLC_NULL_ID && "Invalid union tag");
      return FblcNewUnion(arena, union_decl->fieldv->size, tag, arg);
    }
  }
  assert(false && "invalid fbld value kind");
  return NULL;
}
