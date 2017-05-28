// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <string.h>   // for strcmp

#include "fblc.h"
#include "fbld.h"

// Vars --
//   List of variables in scope, stored in increasing order of variable id
//   starting from variable id 0.
typedef struct Vars {
  FbldName name;
  FbldConcreteTypeDecl* type;
  struct Vars* next;
} Vars;

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

static FbldConcreteTypeDecl* CompileExpr(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldMDefn* mctx, Vars* vars, FbldExpr* expr, FblcExpr** compiled);
static FblcDecl* CompileDecl(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldMDefn* mctx, FbldQualifiedName* entity);

// CompileExpr --
//   Compile the given fbld expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   mdefnv - vector of module definitions in which to find the entities to compile
//   codev - collection of declarations that have already been compiled.
//   mctx - The module definition to use as the context for unqualified entities.
//   vars - The variables currently in scope.
//   expr - the fbld expression to compile.
//   compiled - output parameter for result of compiled expression.
//
// Result:
//   The fbld type of the compiled expression.
//
// Side effects:
//   Sets 'compiled' to the result of compiling the expression.
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
static FbldConcreteTypeDecl* CompileExpr(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldMDefn* mctx, Vars* vars, FbldExpr* expr, FblcExpr** compiled)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FbldVarExpr* source = (FbldVarExpr*)expr;
      FblcVarExpr* var_expr = arena->alloc(arena, sizeof(FblcVarExpr));
      FbldConcreteTypeDecl* type = NULL;
      var_expr->_base.tag = FBLC_VAR_EXPR;
      var_expr->_base.id = 0xDEAD;    // unused
      var_expr->var = FBLC_NULL_ID;
      for (size_t i = 0; vars != NULL; ++i) {
        if (strcmp(vars->name, source->var->name) == 0) {
          var_expr->var = i;
          type = vars->type;
          break;
        }
        vars = vars->next;
      }
      assert(var_expr->var != FBLC_NULL_ID && "variable not found");
      *compiled = &var_expr->_base;
      return type;
    }

    case FBLC_APP_EXPR: {
      FbldAppExpr* source = (FbldAppExpr*)expr;
      FblcAppExpr* app_expr = arena->alloc(arena, sizeof(FblcAppExpr));
      app_expr->_base.tag = FBLC_APP_EXPR;
      app_expr->_base.id = 0xDEAD;    // unused
      app_expr->func = CompileDecl(arena, mdefnv, codev, mctx, source->func);
      FblcVectorInit(arena, app_expr->argv);
      for (size_t i = 0; i < source->argv->size; ++i) {
        CompileExpr(arena, mdefnv, codev, mctx, vars, source->argv->xs[i], FblcVectorExtend(arena, app_expr->argv));
      }
      *compiled = &app_expr->_base;

      FbldDecl* decl = FbldLookupQDecl(mdefnv, mctx, source->func);
      if (decl->tag == FBLD_STRUCT_DECL) {
          return (FbldConcreteTypeDecl*)decl;
      }
      FbldFuncDecl* func_decl = (FbldFuncDecl*)decl;
      return (FbldConcreteTypeDecl*)FbldLookupQDecl(mdefnv, mctx, func_decl->return_type);
    }

    case FBLC_UNION_EXPR: {
      FbldUnionExpr* source = (FbldUnionExpr*)expr;
      FblcUnionExpr* union_expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      union_expr->_base.tag = FBLC_UNION_EXPR;
      union_expr->_base.id = 0xDEAD;  // unused
      union_expr->type = (FblcTypeDecl*)CompileDecl(arena, mdefnv, codev, mctx, source->type);
      FbldUnionDecl* union_decl = (FbldUnionDecl*)FbldLookupQDecl(mdefnv, mctx, source->type);
      union_expr->field = FBLC_NULL_ID;
      for (size_t i = 0; i < union_decl->fieldv->size; ++i) {
        if (strcmp(source->field->name, union_decl->fieldv->xs[i]->name->name) == 0) {
          union_expr->field = i;
          break;
        }
      }
      assert(union_expr->field != FBLC_NULL_ID && "invalid field name");
      CompileExpr(arena, mdefnv, codev, mctx, vars, source->arg, &union_expr->arg);
      *compiled = &union_expr->_base;
      return union_decl;
   }

    case FBLC_ACCESS_EXPR: {
      FbldAccessExpr* source = (FbldAccessExpr*)expr;
      FblcAccessExpr* access_expr = arena->alloc(arena, sizeof(FblcAccessExpr));
      access_expr->_base.tag = FBLC_ACCESS_EXPR;
      access_expr->_base.id = 0xDEAD;   // unused
      FbldConcreteTypeDecl* obj_type = CompileExpr(arena, mdefnv, codev, mctx, vars, source->obj, &access_expr->obj);
      access_expr->field = FBLC_NULL_ID;
      for (size_t i = 0; i < obj_type->fieldv->size; ++i) {
        if (strcmp(source->field->name, obj_type->fieldv->xs[i]->name->name) == 0) {
          access_expr->field = i;
          *compiled = &access_expr->_base;
          return (FbldConcreteTypeDecl*)FbldLookupQDecl(mdefnv, mctx, obj_type->fieldv->xs[i]->type);
        }
      }

      assert(false && "Invalid field for access expr.");
      return NULL;
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* source = (FbldCondExpr*)expr;
      FblcCondExpr* cond_expr = arena->alloc(arena, sizeof(FblcCondExpr));
      cond_expr->_base.tag = FBLC_COND_EXPR;
      cond_expr->_base.id = 0xDEAD;   // unused
      CompileExpr(arena, mdefnv, codev, mctx, vars, source->select, &cond_expr->select);
      FblcVectorInit(arena, cond_expr->argv);
      FbldConcreteTypeDecl* type = NULL;
      for (size_t i = 0; i < source->argv->size; ++i) {
        type = CompileExpr(arena, mdefnv, codev, mctx, vars, source->argv->xs[i], FblcVectorExtend(arena, cond_expr->argv));
      }
      *compiled = &cond_expr->_base;
      assert(type != NULL);
      return type;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* source = (FbldLetExpr*)expr;
      FblcLetExpr* let_expr = arena->alloc(arena, sizeof(FblcLetExpr));
      let_expr->_base.tag = FBLC_LET_EXPR;
      let_expr->_base.id = 0xDEAD;  // unused
      let_expr->type = (FblcTypeDecl*)CompileDecl(arena, mdefnv, codev, mctx, source->type);
      FbldConcreteTypeDecl* def_type = CompileExpr(arena, mdefnv, codev, mctx, vars, source->def, &let_expr->def);
      Vars nvars = {
        .name = source->var->name,
        .type = def_type,
        .next = vars
      };
      *compiled = &let_expr->_base;
      return CompileExpr(arena, mdefnv, codev, mctx, &nvars, source->body, &let_expr->body);
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
//   mctx - The module definition to use as the context for unqualified entities.
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
static FblcDecl* CompileDecl(FblcArena* arena, FbldMDefnV* mdefnv, CompiledDeclV* codev, FbldMDefn* mctx, FbldQualifiedName* entity)
{
  FbldName module = FbldResolveModule(mctx, entity);

  // Check if the entity has already been compiled.
  for (size_t i = 0; i < codev->size; ++i) {
    if (strcmp(codev->xs[i]->module, module) == 0
        && strcmp(codev->xs[i]->name, entity->name->name) == 0) {
      return codev->xs[i]->decl;
    }
  }

  // Find the fbld definition of the entity.
  assert(module != NULL);
  mctx = FbldLookupMDefn(mdefnv, module);
  assert(mctx != NULL && "Failed to find module for entity");

  FbldDecl* src_decl = FbldLookupDecl(mctx, entity->name);
  if (src_decl == NULL) {
      fprintf(stderr, "Failed to find %s in %s\n", entity->name->name, mctx->name->name);
      assert(false && "Entry definition not found");
  }

  // Compile the declaration
  CompiledDecl* c = arena->alloc(arena, sizeof(CompiledDecl));
  c->module = module;
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
            arena, mdefnv, codev, mctx,
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
            arena, mdefnv, codev, mctx,
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
      Vars nvars[src_func_decl->argv->size];
      Vars* vars = NULL;
      for (size_t i = 0; i < src_func_decl->argv->size; ++i) {
        FblcTypeDecl* arg = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, mctx,
            src_func_decl->argv->xs[i]->type);
        FblcVectorAppend(arena, func_decl->argv, arg);
        nvars[i].name = src_func_decl->argv->xs[i]->name->name;
        nvars[i].type = (FbldConcreteTypeDecl*)FbldLookupQDecl(mdefnv, mctx, src_func_decl->argv->xs[i]->type);
        nvars[i].next = vars;
        vars = nvars + i;
      }
      func_decl->return_type = (FblcTypeDecl*)CompileDecl(
            arena, mdefnv, codev, mctx,
            src_func_decl->return_type);
      CompileExpr(arena, mdefnv, codev, mctx, vars, src_func_decl->body, &func_decl->body);
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
FblcDecl* FbldCompile(FblcArena* arena, FbldMDefnV* mdefnv, FbldQualifiedName* entity)
{
  CompiledDeclV codev;
  FblcVectorInit(arena, codev);
  FblcDecl* compiled = CompileDecl(arena, mdefnv, &codev, NULL, entity);
  arena->free(arena, codev.xs);
  return compiled;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldMDefnV* mdefnv, FbldValue* value)
{
  FbldDecl* type_decl = FbldLookupQDecl(mdefnv, NULL, value->type);
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
        if (strcmp(union_decl->fieldv->xs[i]->name->name, value->tag->name) == 0) {
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
