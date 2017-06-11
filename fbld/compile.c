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
      FbldVarExpr* source = (FbldVarExpr*)expr;
      FblcVarExpr* var_expr = arena->alloc(arena, sizeof(FblcVarExpr));
      var_expr->_base.tag = FBLC_VAR_EXPR;
      var_expr->_base.id = 0xDEAD;    // unused
      var_expr->var = source->id;
      assert(var_expr->var != FBLC_NULL_ID);
      return &var_expr->_base;
    }

    case FBLC_APP_EXPR: {
      FbldAppExpr* source = (FbldAppExpr*)expr;
      FblcAppExpr* app_expr = arena->alloc(arena, sizeof(FblcAppExpr));
      app_expr->_base.tag = FBLC_APP_EXPR;
      app_expr->_base.id = 0xDEAD;    // unused
      app_expr->func = CompileDecl(arena, mdefnv, codev, accessv, source->func);
      FblcVectorInit(arena, app_expr->argv);
      for (size_t i = 0; i < source->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, mdefnv, codev, accessv, source->argv->xs[i]);
        FblcVectorAppend(arena, app_expr->argv, arg);
      }
      return &app_expr->_base;
    }

    case FBLC_UNION_EXPR: {
      FbldUnionExpr* source = (FbldUnionExpr*)expr;
      FblcUnionExpr* union_expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      union_expr->_base.tag = FBLC_UNION_EXPR;
      union_expr->_base.id = 0xDEAD;  // unused
      union_expr->type = (FblcTypeDecl*)CompileDecl(arena, mdefnv, codev, accessv, source->type);
      union_expr->field = source->field.id;
      assert(union_expr->field != FBLC_NULL_ID);
      union_expr->arg = CompileExpr(arena, mdefnv, codev, accessv, source->arg);
      return &union_expr->_base;
   }

    case FBLC_ACCESS_EXPR: {
      FbldAccessExpr* source = (FbldAccessExpr*)expr;
      FblcAccessExpr* access_expr = arena->alloc(arena, sizeof(FblcAccessExpr));
      access_expr->_base.tag = FBLC_ACCESS_EXPR;
      access_expr->_base.id = 0xDEAD;   // unused
      access_expr->obj = CompileExpr(arena, mdefnv, codev, accessv, source->obj);
      access_expr->field = source->field.id;
      assert(access_expr->field != FBLC_NULL_ID);
      FbldAccessLoc* access_loc = FblcVectorExtend(arena, *accessv);
      access_loc->expr = &access_expr->_base;
      access_loc->loc = expr->loc;
      return &access_expr->_base;
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* source = (FbldCondExpr*)expr;
      FblcCondExpr* cond_expr = arena->alloc(arena, sizeof(FblcCondExpr));
      cond_expr->_base.tag = FBLC_COND_EXPR;
      cond_expr->_base.id = 0xDEAD;   // unused
      cond_expr->select = CompileExpr(arena, mdefnv, codev, accessv, source->select);
      FblcVectorInit(arena, cond_expr->argv);
      for (size_t i = 0; i < source->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, mdefnv, codev, accessv, source->argv->xs[i]);
        FblcVectorAppend(arena, cond_expr->argv, arg);
      }
      return &cond_expr->_base;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* source = (FbldLetExpr*)expr;
      FblcLetExpr* let_expr = arena->alloc(arena, sizeof(FblcLetExpr));
      let_expr->_base.tag = FBLC_LET_EXPR;
      let_expr->_base.id = 0xDEAD;  // unused
      let_expr->type = (FblcTypeDecl*)CompileDecl(arena, mdefnv, codev, accessv, source->type);
      let_expr->def = CompileExpr(arena, mdefnv, codev, accessv, source->def);
      let_expr->body = CompileExpr(arena, mdefnv, codev, accessv, source->body);
      return &let_expr->_base;
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
  FbldDecl* src_decl = FbldLookupDecl(mdefnv, NULL, entity);
  assert(src_decl != NULL && "Entry definition not found");

  // Compile the declaration
  CompiledDecl* c = arena->alloc(arena, sizeof(CompiledDecl));
  c->module = entity->module->name;
  c->name = entity->name->name;
  c->decl = NULL;
  FblcVectorAppend(arena, *codev, c);
  switch (src_decl->tag) {
    case FBLD_IMPORT_DECL:
      assert(false && "Cannot compile an import declaration");
      break;

    case FBLD_ABSTRACT_TYPE_DECL:
      assert(false && "Cannot compile a abstract type declaration");
      break;

    case FBLD_UNION_DECL: {
      FbldUnionDecl* src_union_decl = (FbldUnionDecl*)src_decl;
      FblcUnionDecl* union_decl = arena->alloc(arena, sizeof(FblcUnionDecl));
      c->decl = &union_decl->_base;
      union_decl->_base.tag = FBLC_UNION_DECL;
      union_decl->_base.id = 0xDEAD;   // id field unused
      FblcVectorInit(arena, union_decl->fieldv);
      for (size_t i = 0; i < src_union_decl->fieldv->size; ++i) {
        FblcTypeDecl* type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            src_union_decl->fieldv->xs[i]->type);
        FblcVectorAppend(arena, union_decl->fieldv, type);
      }
      break;
    }

    case FBLD_STRUCT_DECL: {
      FbldStructDecl* src_struct_decl = (FbldStructDecl*)src_decl;
      FblcStructDecl* struct_decl = arena->alloc(arena, sizeof(FblcStructDecl));
      c->decl = &struct_decl->_base;
      struct_decl->_base.tag = FBLC_STRUCT_DECL;
      struct_decl->_base.id = 0xDEAD;   // id field unused
      FblcVectorInit(arena, struct_decl->fieldv);
      for (size_t i = 0; i < src_struct_decl->fieldv->size; ++i) {
        FblcTypeDecl* type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            src_struct_decl->fieldv->xs[i]->type);
        FblcVectorAppend(arena, struct_decl->fieldv, type);
      }
      break;
    }

    case FBLD_FUNC_DECL: {
      FbldFuncDecl* src_func_decl = (FbldFuncDecl*)src_decl;
      FblcFuncDecl* func_decl = arena->alloc(arena, sizeof(FblcFuncDecl));
      c->decl = &func_decl->_base;
      func_decl->_base.tag = FBLC_FUNC_DECL;
      func_decl->_base.id = 0xDEAD;     // id field unused
      FblcVectorInit(arena, func_decl->argv);
      for (size_t i = 0; i < src_func_decl->argv->size; ++i) {
        FblcTypeDecl* arg = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            src_func_decl->argv->xs[i]->type);
        FblcVectorAppend(arena, func_decl->argv, arg);
      }
      func_decl->return_type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, accessv,
            src_func_decl->return_type);
      func_decl->body = CompileExpr(arena, mdefnv, codev, accessv, src_func_decl->body);
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
