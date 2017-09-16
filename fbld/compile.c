// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf

#include "fblc.h"
#include "fbld.h"

#define UNREACHABLE(x) assert(false && x)

// CompiledType
//   The compiled type for a named entity.
typedef struct {
  FbldQName* entity;
  FblcType* compiled;
} CompiledType;

// CompiledTypeV
//   A vector of compiled types.
typedef struct {
  size_t size;
  CompiledType* xs;
} CompiledTypeV;

// CompiledFunc
//   The compiled function for a named entity.
typedef struct {
  FbldQName* entity;
  FblcFunc* compiled;
} CompiledFunc;

// CompiledFuncV
//   A vector of compiled functions.
typedef struct {
  size_t size;
  CompiledFunc* xs;
} CompiledFuncV;

// Compiled
//   A collection of already compiled entities.
typedef struct {
  CompiledTypeV typev;
  CompiledFuncV funcv;
} Compiled;

static FbldType* LookupType(FbldProgram* prgm, FbldQName* entity);
static FbldFunc* LookupFunc(FbldProgram* prgm, FbldQName* entity);
static FbldQName* ResolveEntity(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity);
static FblcExpr* CompileExpr(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldMRef* mref, FbldExpr* expr, Compiled* compiled);
static FblcType* CompileType(FblcArena* arena, FbldProgram* prgm, FbldQName* entity, Compiled* compiled);
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity, Compiled* compiled);
static FblcType* CompileForeignType(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity, Compiled* compiled);

// LookupType --
//   Look up a type entity in the program.
//
// Inputs:
//   prgm - The program to look in.
//   entity - The entity to look up.
//
// Results:
//   The type entity or NULL if no such function was found.
//
// Side effects:
//   None
static FbldType* LookupType(FbldProgram* prgm, FbldQName* entity)
{
  for (size_t i = 0; i < prgm->mdefnv.size; ++i) {
    FbldMDefn* mdefn = prgm->mdefnv.xs[i];
    if (FbldNamesEqual(entity->mref->name->name, mdefn->name->name)) {
      for (size_t j = 0; j < mdefn->typev->size; ++j) {
        FbldType* type = mdefn->typev->xs[j];
        if (FbldNamesEqual(entity->name->name, type->name->name)) {
          return type;
        }
      }
    }
  }
  return NULL;
}

// LookupFunc --
//   Look up a function entity in the program.
//
// Inputs:
//   prgm - The program to look in.
//   entity - The entity to look up.
//
// Results:
//   The function entity or NULL if no such function was found.
//
// Side effects:
//   None
static FbldFunc* LookupFunc(FbldProgram* prgm, FbldQName* entity)
{
  for (size_t i = 0; i < prgm->mdefnv.size; ++i) {
    FbldMDefn* mdefn = prgm->mdefnv.xs[i];
    if (FbldNamesEqual(entity->mref->name->name, mdefn->name->name)) {
      for (size_t j = 0; j < mdefn->funcv->size; ++j) {
        FbldFunc* func = mdefn->funcv->xs[j];
        if (FbldNamesEqual(entity->name->name, func->name->name)) {
          return func;
        }
      }
    }
  }
  return NULL;
}

// ResolveEntity --
//   Resolve all type and module arguments in the given entity specification.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program context.
//   mref - The context the entity is being referred to from.
//   entity - The entity to resolve.
//
// Results:
//   A resolved form of the entity, with type and module variables replaced
//   based on the provided mref context.
//
// Side effects:
//   Allocates a new entity that somebody should probably clean up somehow.
static FbldQName* ResolveEntity(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity)
{
  FbldQName* resolved = FBLC_ALLOC(arena, FbldQName);
  resolved->name = entity->name;

  if (entity->mref == NULL) {
    resolved->mref = mref;
    return resolved;
  }

  // TODO: Substitute type and module parameters in entity->mref based on the
  // substitution map derived from mref and the module for entity->mref->name
  // (or something like that).
  assert(false && "TODO: ResolveEntity");
  return NULL;
}

// CompileExpr --
//   Return a compiled fblc expr for the given expression.
//
// Inputs:
//   arena - Arena to use for allocations.
//   accessv - Collection of access expression debug info to return.
//   prgm - The program environment.
//   mref - The current module context.
//   expr - The expression to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc expr.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds additional compiled types and functions to 'compiled' as needed.
static FblcExpr* CompileExpr(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldMRef* mref, FbldExpr* expr, Compiled* compiled)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FbldVarExpr* var_expr_d = (FbldVarExpr*)expr;
      FblcVarExpr* var_expr_c = FBLC_ALLOC(arena, FblcVarExpr);
      var_expr_c->_base.tag = FBLC_VAR_EXPR;
      var_expr_c->var = var_expr_d->var.id;
      return &var_expr_c->_base;
    }

    case FBLC_APP_EXPR: {
      FbldAppExpr* app_expr_d = (FbldAppExpr*)expr;
      FbldQName* entity = ResolveEntity(arena, prgm, mref, app_expr_d->func);
      FbldFunc* func = LookupFunc(prgm, entity);
      if (func != NULL) {
        FblcAppExpr* app_expr_c = FBLC_ALLOC(arena, FblcAppExpr);
        app_expr_c->_base.tag = FBLC_APP_EXPR;
        app_expr_c->func = CompileFunc(arena, accessv, prgm, entity, compiled);
        FblcVectorInit(arena, app_expr_c->argv);
        for (size_t i = 0; i < app_expr_d->argv->size; ++i) {
          FblcExpr* arg = CompileExpr(arena, accessv, prgm, mref, app_expr_d->argv->xs[i], compiled);
          FblcVectorAppend(arena, app_expr_c->argv, arg);
        }
        return &app_expr_c->_base;
      } else {
        FblcStructExpr* struct_expr = FBLC_ALLOC(arena, FblcStructExpr);
        struct_expr->_base.tag = FBLC_STRUCT_EXPR;
        struct_expr->type = CompileType(arena, prgm, entity, compiled);
        FblcVectorInit(arena, struct_expr->argv);
        for (size_t i = 0; i < app_expr_d->argv->size; ++i) {
          FblcExpr* arg = CompileExpr(arena, accessv, prgm, mref, app_expr_d->argv->xs[i], compiled);
          FblcVectorAppend(arena, struct_expr->argv, arg);
        }
        return &struct_expr->_base;
      }
    }

    case FBLC_STRUCT_EXPR: {
      // It's the compiler's job to mark an app expression as a struct
      // expression when appropriate, which means nobody else should have been
      // making struct expressions yet. TODO: Why not define a separate
      // FBLD_*_EXPR enum that doesn't have the STRUCT expr option?
      UNREACHABLE("Unexpected STRUCT expression");
      return NULL;
    }

    case FBLC_UNION_EXPR: {
      FbldUnionExpr* union_expr_d = (FbldUnionExpr*)expr;
      FblcUnionExpr* union_expr_c = FBLC_ALLOC(arena, FblcUnionExpr);
      union_expr_c->_base.tag = FBLC_UNION_EXPR;
      union_expr_c->type = CompileForeignType(arena, prgm, mref, union_expr_d->type, compiled);
      union_expr_c->field = union_expr_d->field.id;
      union_expr_c->arg = CompileExpr(arena, accessv, prgm, mref, union_expr_d->arg, compiled);
      return &union_expr_c->_base;
    }

    case FBLC_ACCESS_EXPR: {
      FbldAccessExpr* access_expr_d = (FbldAccessExpr*)expr;
      FblcAccessExpr* access_expr_c = FBLC_ALLOC(arena, FblcAccessExpr);
      access_expr_c->_base.tag = FBLC_ACCESS_EXPR;
      access_expr_c->obj = CompileExpr(arena, accessv, prgm, mref, access_expr_d->obj, compiled);
      access_expr_c->field = access_expr_d->field.id;

      FbldAccessLoc* loc = FblcVectorExtend(arena, *accessv);
      loc->expr = &access_expr_c->_base;
      loc->loc = access_expr_d->_base.loc;
      return &access_expr_c->_base;
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* cond_expr_d = (FbldCondExpr*)expr;
      FblcCondExpr* cond_expr_c = FBLC_ALLOC(arena, FblcCondExpr);
      cond_expr_c->_base.tag = FBLC_COND_EXPR;
      cond_expr_c->select = CompileExpr(arena, accessv, prgm, mref, cond_expr_d->select, compiled);
      FblcVectorInit(arena, cond_expr_c->argv);
      for (size_t i = 0; i < cond_expr_d->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, accessv, prgm, mref, cond_expr_d->argv->xs[i], compiled);
        FblcVectorAppend(arena, cond_expr_c->argv, arg);
      }
      return &cond_expr_c->_base;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* let_expr_d = (FbldLetExpr*)expr;
      FblcLetExpr* let_expr_c = FBLC_ALLOC(arena, FblcLetExpr);
      let_expr_c->_base.tag = FBLC_LET_EXPR;
      let_expr_c->type = CompileForeignType(arena, prgm, mref, let_expr_d->type, compiled);
      let_expr_c->def = CompileExpr(arena, accessv, prgm, mref, let_expr_d->def, compiled);
      let_expr_c->body = CompileExpr(arena, accessv, prgm, mref, let_expr_d->body, compiled);
      return &let_expr_c->_base;
    }

    default: {
      UNREACHABLE("invalid fbld expression tag");
      return NULL;
    }
  }
}

// CompileType --
//   Return a compiled fblc type for the named type.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program environment.
//   entity - The type to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc type, or NULL in case of error.
//
// Side effects:
//   Adds the compiled type to 'compiled' if it is newly compiled.
static FblcType* CompileType(FblcArena* arena, FbldProgram* prgm, FbldQName* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->typev.size; ++i) {
    if (FbldQNamesEqual(compiled->typev.xs[i].entity, entity)) {
      return compiled->typev.xs[i].compiled;
    }
  }

  FbldType* type_d = LookupType(prgm, entity);
  assert(type_d != NULL);

  FblcType* type_c = FBLC_ALLOC(arena, FblcType);
  CompiledType* compiled_type = FblcVectorExtend(arena, compiled->typev);
  compiled_type->entity = entity;
  compiled_type->compiled = type_c;

  switch (type_d->kind) {
    case FBLD_STRUCT_KIND: type_c->kind = FBLC_STRUCT_KIND; break;
    case FBLD_UNION_KIND: type_c->kind = FBLC_UNION_KIND; break;
    case FBLD_ABSTRACT_KIND: UNREACHABLE("abstract kind encountered in compiler"); break;
  }

  FblcVectorInit(arena, type_c->fieldv);
  for (size_t i = 0; i < type_d->fieldv->size; ++i) {
    FbldArg* arg_d = type_d->fieldv->xs[i];
    FblcType* arg_c = CompileForeignType(arena, prgm, entity->mref, arg_d->type, compiled);
    FblcVectorAppend(arena, type_c->fieldv, arg_c);
  }

  return type_c;
}

// CompileFunc --
//   Return a compiled fblc func for the named function.
//
// Inputs:
//   arena - Arena to use for allocations.
//   accessv - Collection of access expression debug info to return.
//   prgm - The program environment.
//   entity - The function to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc func, or NULL in case of error.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds the compiled function to 'compiled' if it is newly compiled.
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->funcv.size; ++i) {
    if (FbldQNamesEqual(compiled->funcv.xs[i].entity, entity)) {
      return compiled->funcv.xs[i].compiled;
    }
  }

  FbldFunc* func_d = LookupFunc(prgm, entity);
  assert(func_d != NULL);

  FblcFunc* func_c = FBLC_ALLOC(arena, FblcFunc);
  FblcVectorInit(arena, func_c->argv);
  for (size_t arg_id = 0; arg_id < func_d->argv->size; ++arg_id) {
    FblcType* arg_type = CompileForeignType(arena, prgm, entity->mref, func_d->argv->xs[arg_id]->type, compiled);
    FblcVectorAppend(arena, func_c->argv, arg_type);
  }
  func_c->return_type = CompileForeignType(arena, prgm, entity->mref, func_d->return_type, compiled);
  func_c->body = CompileExpr(arena, accessv, prgm, entity->mref, func_d->body, compiled);

  // TODO: Add func to compiled!
  return func_c;
}

// CompileForeignType --
//   Compile a type referred to from another context.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program environment.
//   mref - The context from which the type is referred to.
//   entity - The type to compile.
//   compiled - The collection of compiled entities.
static FblcType* CompileForeignType(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity, Compiled* compiled)
{
  FbldQName* resolved = ResolveEntity(arena, prgm, mref, entity);
  return CompileType(arena, prgm, resolved, compiled);
}

// FbldCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity)
{
  FbldFunc* func_d = LookupFunc(prgm, entity);
  if (func_d == NULL) {
    fprintf(stderr, "main entry not found\n");
    return NULL;
  }

  Compiled compiled;
  FblcVectorInit(arena, compiled.typev);
  FblcVectorInit(arena, compiled.funcv);
  FblcFunc* func_c = CompileFunc(arena, accessv, prgm, entity, &compiled);

  // Create an FblcProc wrapper for the compiled function.
  // TODO: Once we support procs in fbld, move the wrapper to the fbld side of
  // compilation.
  FblcEvalActn* body = FBLC_ALLOC(arena, FblcEvalActn);
  body->_base.tag = FBLC_EVAL_ACTN;
  body->arg = func_c->body;

  FblcProc* proc = FBLC_ALLOC(arena, FblcProc);
  FblcVectorInit(arena, proc->portv);
  proc->argv.size = func_c->argv.size;
  proc->argv.xs = func_c->argv.xs;
  proc->return_type = func_c->return_type;
  proc->body = &body->_base;

  FbldLoaded* loaded = FBLC_ALLOC(arena, FbldLoaded);
  loaded->prog = prgm;
  loaded->proc_d = func_d;
  loaded->proc_c = proc;
  return loaded;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  FbldType* type = LookupType(prgm, value->type);
  assert(type != NULL);

  switch (value->kind) {
    case FBLD_STRUCT_KIND: {
      FblcValue* value_c = FblcNewStruct(arena, type->fieldv->size);
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        value_c->fields[i] = FbldCompileValue(arena, prgm, value->fieldv->xs[i]);
      }
      return value_c;
    }

    case FBLD_UNION_KIND: {
      FblcValue* arg = FbldCompileValue(arena, prgm, value->fieldv->xs[0]);
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (FbldNamesEqual(value->tag->name, type->fieldv->xs[i]->name->name)) {
          return FblcNewUnion(arena, type->fieldv->size, i, arg);
        }
      }
      assert(false && "Invalid union tag");
      return NULL;
    }

    case FBLD_ABSTRACT_KIND: {
      UNREACHABLE("attempt to compile type with abstract kind");
      return NULL;
    }

    default: {
      UNREACHABLE("invalid value kind");
      return NULL;
    }
  }
}
