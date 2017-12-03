// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf
#include <stdlib.h>   // for abort

#include "fblc.h"
#include "fbld.h"

#define UNREACHABLE(x) assert(false && x)

// CompiledType
//   The compiled type for a named entity.
typedef struct {
  FbldQRef* entity;
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
  FbldQRef* entity;
  FblcFunc* compiled;
} CompiledFunc;

// CompiledFuncV
//   A vector of compiled processes.
typedef struct {
  size_t size;
  CompiledFunc* xs;
} CompiledFuncV;

// CompiledProc
//   The compiled process for a named entity.
typedef struct {
  FbldQRef* entity;
  FblcProc* compiled;
} CompiledProc;

// CompiledProcV
//   A vector of compiled processes.
typedef struct {
  size_t size;
  CompiledProc* xs;
} CompiledProcV;

// Context
//   The context for compilation.
//
// Fields:
//   arena - The arena to use for allocations.
//   prgm - The program environment.
//   accessv - Compiled locations of access expressions.
//   typev - The collection of already compiled types.
//   funcv - The collection of already compiled funcs.
//   procv - The collection of already compiled procs.
typedef struct {
  FblcArena* arena;
  FbldProgram* prgm;
  FbldAccessLocV* accessv;
  CompiledTypeV typev;
  CompiledFuncV funcv;
  CompiledProcV procv;
} Context;

static FbldType* LookupType(Context* ctx, FbldQRef* qref);
static FbldFunc* LookupFunc(Context* ctx, FbldQRef* qref);
static FbldProc* LookupProc(Context* ctx, FbldQRef* qref);
static FbldModule* LookupModule(Context* ctx, FbldQRef* qref);
static FblcProc* CompileGivenProc(Context* ctx, FbldQRef* qref, FbldProc* proc);
static FblcType* CompileForeignType(Context* ctx, FbldQRef* mref, FbldQRef* qref);
static FblcType* CompileType(Context* ctx, FbldQRef* qref);
static FblcFunc* CompileFunc(Context* ctx, FbldQRef* qref);
static FblcActn* CompileActn(Context* ctx, FbldQRef* mref, FbldActn* actn);
static FblcExpr* CompileExpr(Context* ctx, FbldQRef* mref, FbldExpr* expr);
static FblcValue* CompileValue(Context* ctx, FbldValue* value);


// LookupType --
//   Lookup the declaration of the type referred to by qref.
//
// Inputs:
//   ctx - The compilation context.
//   qref - A global resolved qref referring to the type to look up.
//
// Results:
//   The declaration of the type.
//
// Side effects:
//   Behavior is undefined if the type could not be found.
static FbldType* LookupType(Context* ctx, FbldQRef* qref)
{
  assert(qref->r.state == FBLD_RSTATE_RESOLVED);
  assert(qref->r.mref != NULL && "type is not a valid top-level declaration");
  assert(qref->r.decl->tag == FBLD_TYPE_DECL);
  FbldName* name = qref->r.decl->name;
  FbldModule* module = LookupModule(ctx, qref->r.mref);
  for (size_t i = 0; i < module->declv->size; ++i) {
    if (FbldNamesEqual(module->declv->xs[i]->name->name, name->name)) {
      assert(module->declv->xs[i]->tag == FBLD_TYPE_DECL);
      return (FbldType*)module->declv->xs[i];
    }
  }
  assert(false && "LookupType failed");
  return NULL;
}

// LookupFunc --
//   Lookup the declaration of the function referred to by qref.
//
// Inputs:
//   ctx - The compilation context.
//   qref - A global resolved qref referring to the function to look up.
//
// Results:
//   The declaration of the function.
//
// Side effects:
//   Behavior is undefined if the function could not be found.
static FbldFunc* LookupFunc(Context* ctx, FbldQRef* qref)
{
  assert(qref->r.state == FBLD_RSTATE_RESOLVED);
  assert(qref->r.mref != NULL && "func is not a valid top-level declaration");
  assert(qref->r.decl->tag == FBLD_FUNC_DECL);
  FbldName* name = qref->r.decl->name;
  FbldModule* module = LookupModule(ctx, qref->r.mref);
  for (size_t i = 0; i < module->declv->size; ++i) {
    if (FbldNamesEqual(module->declv->xs[i]->name->name, name->name)) {
      assert(module->declv->xs[i]->tag == FBLD_FUNC_DECL);
      return (FbldFunc*)module->declv->xs[i];
    }
  }
  assert(false && "LookupFunc failed");
  return NULL;
}

// LookupProc --
//   Lookup the declaration of the process referred to by qref.
//
// Inputs:
//   ctx - The compilation context.
//   qref - A global resolved qref referring to the process to look up.
//
// Results:
//   The declaration of the process.
//
// Side effects:
//   Behavior is undefined if the process could not be found.
static FbldProc* LookupProc(Context* ctx, FbldQRef* qref)
{
  assert(qref->r.state == FBLD_RSTATE_RESOLVED);
  assert(qref->r.mref != NULL && "proc is not a valid top-level declaration");
  assert(qref->r.decl->tag == FBLD_PROC_DECL);
  FbldName* name = qref->r.decl->name;
  FbldModule* module = LookupModule(ctx, qref->r.mref);
  for (size_t i = 0; i < module->declv->size; ++i) {
    if (FbldNamesEqual(module->declv->xs[i]->name->name, name->name)) {
      assert(module->declv->xs[i]->tag == FBLD_PROC_DECL);
      return (FbldProc*)module->declv->xs[i];
    }
  }
  assert(false && "LookupProc failed");
  return NULL;
}

// LookupModule --
//   Lookup the declaration of the module referred to by qref.
//
// Inputs:
//   ctx - The compilation context.
//   qref - A global resolved qref referring to the module to look up.
//
// Results:
//   The declaration of the module.
//
// Side effects:
//   Behavior is undefined if the module could not be found.
static FbldModule* LookupModule(Context* ctx, FbldQRef* qref)
{
  assert(qref->r.state == FBLD_RSTATE_RESOLVED);
  assert(qref->r.decl->tag == FBLD_MODULE_DECL);
  FbldName* name = qref->r.decl->name;
  if (qref->r.mref == NULL) {
    // We are looking for a top-level module declaration.
    for (size_t i = 0; i < ctx->prgm->modulev.size; ++i) {
      if (FbldNamesEqual(ctx->prgm->modulev.xs[i]->_base.name->name, name->name)) {
        return ctx->prgm->modulev.xs[i];
      }
    }
    assert(false && "LookupModule failed");
    return NULL;
  }

  FbldModule* module = LookupModule(ctx, qref->r.mref);
  for (size_t i = 0; i < module->declv->size; ++i) {
    if (FbldNamesEqual(module->declv->xs[i]->name->name, name->name)) {
      assert(module->declv->xs[i]->tag == FBLD_MODULE_DECL);
      return (FbldModule*)module->declv->xs[i];
    }
  }
  assert(false && "LookupModule failed");
  return NULL;
}

// CompileGivenProc --
//   Return a compiled fblc proc for the given process.
//
// Inputs:
//   ctx - The context for compilation.
//   qref - The global resolved reference to the process being compiled.
//   proc - The process to compile.
//
// Returns:
//   A compiled fblc proc.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds the compiled process to 'compiled' if it is newly compiled.
static FblcProc* CompileGivenProc(Context* ctx, FbldQRef* qref, FbldProc* proc)
{
  FbldProc* proc_d = proc;
  FblcProc* proc_c = FBLC_ALLOC(ctx->arena, FblcProc);
  CompiledProc* compiled_proc = FblcVectorExtend(ctx->arena, ctx->procv);
  compiled_proc->entity = qref;
  compiled_proc->compiled = proc_c;

  FblcVectorInit(ctx->arena, proc_c->portv);
  for (size_t i = 0; i < proc_d->portv->size; ++i) {
    FblcPort* port = FblcVectorExtend(ctx->arena, proc_c->portv);
    port->type = CompileForeignType(ctx, qref, proc_d->portv->xs[i].type);
    switch (proc_d->portv->xs[i].polarity) {
      case FBLD_GET_POLARITY:
        port->polarity = FBLC_GET_POLARITY;
        break;

      case FBLD_PUT_POLARITY:
        port->polarity = FBLC_PUT_POLARITY;
        break;

      default:
        UNREACHABLE("Invalid port polarity");
        abort();
    }
  }

  FblcVectorInit(ctx->arena, proc_c->argv);
  for (size_t arg_id = 0; arg_id < proc_d->argv->size; ++arg_id) {
    FblcType* arg_type = CompileForeignType(ctx, qref, proc_d->argv->xs[arg_id]->type);
    FblcVectorAppend(ctx->arena, proc_c->argv, arg_type);
  }
  proc_c->return_type = CompileForeignType(ctx, qref, proc_d->return_type);
  proc_c->body = CompileActn(ctx, qref, proc_d->body);
  return proc_c;
}

// CompileExpr --
//   Return a compiled fblc expr for the given expression.
//
// Inputs:
//   ctx - The compilation context.
//   mref - The current module context.
//   expr - The expression to compile.
//
// Returns:
//   A compiled fblc expr.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds additional compiled types and functions to 'compiled' as needed.
static FblcExpr* CompileExpr(Context* ctx, FbldQRef* mref, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLD_VAR_EXPR: {
      FbldVarExpr* var_expr_d = (FbldVarExpr*)expr;
      FblcVarExpr* var_expr_c = FBLC_ALLOC(ctx->arena, FblcVarExpr);
      var_expr_c->_base.tag = FBLC_VAR_EXPR;
      var_expr_c->var = var_expr_d->var.id;
      return &var_expr_c->_base;
    }

    case FBLD_APP_EXPR: {
      FbldAppExpr* app_expr_d = (FbldAppExpr*)expr;
      FbldQRef* entity = app_expr_d->func;
      assert(entity->r.state == FBLD_RSTATE_RESOLVED);
      if (entity->r.decl->tag == FBLD_FUNC_DECL) {
        FblcAppExpr* app_expr_c = FBLC_ALLOC(ctx->arena, FblcAppExpr);
        app_expr_c->_base.tag = FBLC_APP_EXPR;
        app_expr_c->func = CompileFunc(ctx, entity);
        FblcVectorInit(ctx->arena, app_expr_c->argv);
        for (size_t i = 0; i < app_expr_d->argv->size; ++i) {
          FblcExpr* arg = CompileExpr(ctx, mref, app_expr_d->argv->xs[i]);
          FblcVectorAppend(ctx->arena, app_expr_c->argv, arg);
        }
        return &app_expr_c->_base;
      } else {
        FblcStructExpr* struct_expr = FBLC_ALLOC(ctx->arena, FblcStructExpr);
        struct_expr->_base.tag = FBLC_STRUCT_EXPR;
        struct_expr->type = CompileType(ctx, entity);
        FblcVectorInit(ctx->arena, struct_expr->argv);
        for (size_t i = 0; i < app_expr_d->argv->size; ++i) {
          FblcExpr* arg = CompileExpr(ctx, mref, app_expr_d->argv->xs[i]);
          FblcVectorAppend(ctx->arena, struct_expr->argv, arg);
        }
        return &struct_expr->_base;
      }
    }

    case FBLD_UNION_EXPR: {
      FbldUnionExpr* union_expr_d = (FbldUnionExpr*)expr;
      FblcUnionExpr* union_expr_c = FBLC_ALLOC(ctx->arena, FblcUnionExpr);
      union_expr_c->_base.tag = FBLC_UNION_EXPR;
      union_expr_c->type = CompileForeignType(ctx, mref, union_expr_d->type);
      union_expr_c->field = union_expr_d->field.id;
      union_expr_c->arg = CompileExpr(ctx, mref, union_expr_d->arg);
      return &union_expr_c->_base;
    }

    case FBLD_ACCESS_EXPR: {
      FbldAccessExpr* access_expr_d = (FbldAccessExpr*)expr;
      FblcAccessExpr* access_expr_c = FBLC_ALLOC(ctx->arena, FblcAccessExpr);
      access_expr_c->_base.tag = FBLC_ACCESS_EXPR;
      access_expr_c->obj = CompileExpr(ctx, mref, access_expr_d->obj);
      access_expr_c->field = access_expr_d->field.id;

      FbldAccessLoc* loc = FblcVectorExtend(ctx->arena, *ctx->accessv);
      loc->expr = &access_expr_c->_base;
      loc->loc = access_expr_d->_base.loc;
      return &access_expr_c->_base;
    }

    case FBLD_COND_EXPR: {
      FbldCondExpr* cond_expr_d = (FbldCondExpr*)expr;
      FblcCondExpr* cond_expr_c = FBLC_ALLOC(ctx->arena, FblcCondExpr);
      cond_expr_c->_base.tag = FBLC_COND_EXPR;
      cond_expr_c->select = CompileExpr(ctx, mref, cond_expr_d->select);
      FblcVectorInit(ctx->arena, cond_expr_c->argv);
      for (size_t i = 0; i < cond_expr_d->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(ctx, mref, cond_expr_d->argv->xs[i]);
        FblcVectorAppend(ctx->arena, cond_expr_c->argv, arg);
      }
      return &cond_expr_c->_base;
    }

    case FBLD_LET_EXPR: {
      FbldLetExpr* let_expr_d = (FbldLetExpr*)expr;
      FblcLetExpr* let_expr_c = FBLC_ALLOC(ctx->arena, FblcLetExpr);
      let_expr_c->_base.tag = FBLC_LET_EXPR;
      let_expr_c->type = CompileForeignType(ctx, mref, let_expr_d->type);
      let_expr_c->def = CompileExpr(ctx, mref, let_expr_d->def);
      let_expr_c->body = CompileExpr(ctx, mref, let_expr_d->body);
      return &let_expr_c->_base;
    }

    default: {
      UNREACHABLE("invalid fbld expression tag");
      return NULL;
    }
  }
}
//
// CompileActn --
//   Return a compiled fblc actn for the given action.
//
// Inputs:
//   ctx - The compilation context.
//   mref - The current module context.
//   actn - The action to compile.
//
// Returns:
//   A compiled fblc actn.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds additional compiled types, functions, and processes to 'compiled' as needed.
static FblcActn* CompileActn(Context* ctx, FbldQRef* mref, FbldActn* actn)
{
  switch (actn->tag) {
    case FBLD_EVAL_ACTN: {
      FbldEvalActn* eval_actn_d = (FbldEvalActn*)actn;
      FblcEvalActn* eval_actn_c = FBLC_ALLOC(ctx->arena, FblcEvalActn);
      eval_actn_c->_base.tag = FBLC_EVAL_ACTN;
      eval_actn_c->arg = CompileExpr(ctx, mref, eval_actn_d->arg);
      return &eval_actn_c->_base;
    }

    case FBLD_GET_ACTN: {
      FbldGetActn* get_actn_d = (FbldGetActn*)actn;
      FblcGetActn* get_actn_c = FBLC_ALLOC(ctx->arena, FblcGetActn);
      get_actn_c->_base.tag = FBLC_GET_ACTN;
      get_actn_c->port = get_actn_d->port.id;
      return &get_actn_c->_base;
    }

    case FBLD_PUT_ACTN: {
      FbldPutActn* put_actn_d = (FbldPutActn*)actn;
      FblcPutActn* put_actn_c = FBLC_ALLOC(ctx->arena, FblcPutActn);
      put_actn_c->_base.tag = FBLC_PUT_ACTN;
      put_actn_c->port = put_actn_d->port.id;
      put_actn_c->arg = CompileExpr(ctx, mref, put_actn_d->arg);
      return &put_actn_c->_base;
    }

    case FBLD_COND_ACTN: {
      FbldCondActn* cond_actn_d = (FbldCondActn*)actn;
      FblcCondActn* cond_actn_c = FBLC_ALLOC(ctx->arena, FblcCondActn);
      cond_actn_c->_base.tag = FBLC_COND_ACTN;
      cond_actn_c->select = CompileExpr(ctx, mref, cond_actn_d->select);
      FblcVectorInit(ctx->arena, cond_actn_c->argv);
      for (size_t i = 0; i < cond_actn_d->argv->size; ++i) {
        FblcActn* arg = CompileActn(ctx, mref, cond_actn_d->argv->xs[i]);
        FblcVectorAppend(ctx->arena, cond_actn_c->argv, arg);
      }
      return &cond_actn_c->_base;
    }

    case FBLD_CALL_ACTN: {
      FbldCallActn* call_actn_d = (FbldCallActn*)actn;
      FblcCallActn* call_actn_c = FBLC_ALLOC(ctx->arena, FblcCallActn);
      call_actn_c->_base.tag = FBLC_CALL_ACTN;
      FbldProc* proc = LookupProc(ctx, call_actn_d->proc);
      call_actn_c->proc = CompileGivenProc(ctx, call_actn_d->proc, proc);
      FblcVectorInit(ctx->arena, call_actn_c->portv);
      for (size_t i = 0; i < call_actn_d->portv->size; ++i) {
        FblcPortId port = call_actn_d->portv->xs[i].id;
        FblcVectorAppend(ctx->arena, call_actn_c->portv, port);
      }
      FblcVectorInit(ctx->arena, call_actn_c->argv);
      for (size_t i = 0; i < call_actn_d->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(ctx, mref, call_actn_d->argv->xs[i]);
        FblcVectorAppend(ctx->arena, call_actn_c->argv, arg);
      }
      return &call_actn_c->_base;
    }

    case FBLD_LINK_ACTN: {
      FbldLinkActn* link_actn_d = (FbldLinkActn*)actn;
      FblcLinkActn* link_actn_c = FBLC_ALLOC(ctx->arena, FblcLinkActn);
      link_actn_c->_base.tag = FBLC_LINK_ACTN;
      link_actn_c->type = CompileType(ctx, link_actn_d->type);
      link_actn_c->body = CompileActn(ctx, mref, link_actn_d->body);
      return &link_actn_c->_base;
    }

    case FBLD_EXEC_ACTN: {
      FbldExecActn* exec_actn_d = (FbldExecActn*)actn;
      FblcExecActn* exec_actn_c = FBLC_ALLOC(ctx->arena, FblcExecActn);
      exec_actn_c->_base.tag = FBLC_EXEC_ACTN;
      FblcVectorInit(ctx->arena, exec_actn_c->execv);
      for (size_t i = 0; i < exec_actn_d->execv->size; ++i) {
        FblcExec* exec = FblcVectorExtend(ctx->arena, exec_actn_c->execv);
        exec->type = CompileForeignType(ctx, mref, exec_actn_d->execv->xs[i].type);
        exec->actn = CompileActn(ctx, mref, exec_actn_d->execv->xs[i].actn);
      }
      exec_actn_c->body = CompileActn(ctx, mref, exec_actn_d->body);
      return &exec_actn_c->_base;
    }

    default: {
      UNREACHABLE("invalid fbld action tag");
      return NULL;
    }
  }
}

// CompileFunc --
//   Return a compiled fblc func for the named function.
//
// Inputs:
//   ctx - The context to use for compilation.
//   qref - The function to compile.
//
// Returns:
//   A compiled fblc func.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds the compiled function to 'compiled' if it is newly compiled.
static FblcFunc* CompileFunc(Context* ctx, FbldQRef* qref)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < ctx->funcv.size; ++i) {
    if (FbldQRefsEqual(ctx->funcv.xs[i].entity, qref)) {
      return ctx->funcv.xs[i].compiled;
    }
  }

  FbldFunc* func_d = LookupFunc(ctx, qref);
  assert(func_d != NULL);

  FblcFunc* func_c = FBLC_ALLOC(ctx->arena, FblcFunc);
  CompiledFunc* compiled_func = FblcVectorExtend(ctx->arena, ctx->funcv);
  compiled_func->entity = qref;
  compiled_func->compiled = func_c;

  FblcVectorInit(ctx->arena, func_c->argv);
  for (size_t arg_id = 0; arg_id < func_d->argv->size; ++arg_id) {
    FblcType* arg_type = CompileForeignType(ctx, qref->r.mref, func_d->argv->xs[arg_id]->type);
    FblcVectorAppend(ctx->arena, func_c->argv, arg_type);
  }
  func_c->return_type = CompileForeignType(ctx, qref->r.mref, func_d->return_type);
  func_c->body = CompileExpr(ctx, qref->r.mref, func_d->body);

  return func_c;
}
//
// CompileForeignType --
//   Compile a type referred to from another context.
//
// Inputs:
//   ctx - The compilation context.
//   mref - The context from which the type is referred to.
//   qref - The type to compile.
//
// Results:
//   The compiled fblc type.
//
// Side effects:
//   Adds the compiled type to the context if it is newly compiled.
static FblcType* CompileForeignType(Context* ctx, FbldQRef* mref, FbldQRef* qref)
{
  // TODO: Use mref to resolve parameters in qref.
  // For now we assume qref has no parameters.
  return CompileType(ctx, qref);
}

// CompileType --
//   Compile a type.
//
// Inputs:
//   ctx - The compilation context.
//   qref - The type to compile.
//
// Results:
//   The compiled fblc type.
//
// Side effects:
//   Adds the compiled type to the context if it is newly compiled.
static FblcType* CompileType(Context* ctx, FbldQRef* qref)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < ctx->typev.size; ++i) {
    if (FbldQRefsEqual(ctx->typev.xs[i].entity, qref)) {
      return ctx->typev.xs[i].compiled;
    }
  }

  FbldType* type_d = LookupType(ctx, qref);
  assert(type_d != NULL);

  FblcType* type_c = FBLC_ALLOC(ctx->arena, FblcType);
  CompiledType* compiled_type = FblcVectorExtend(ctx->arena, ctx->typev);
  compiled_type->entity = qref;
  compiled_type->compiled = type_c;

  switch (type_d->kind) {
    case FBLD_STRUCT_KIND: type_c->kind = FBLC_STRUCT_KIND; break;
    case FBLD_UNION_KIND: type_c->kind = FBLC_UNION_KIND; break;
    case FBLD_ABSTRACT_KIND: UNREACHABLE("abstract kind encountered in compiler"); break;
  }

  FblcVectorInit(ctx->arena, type_c->fieldv);
  for (size_t i = 0; i < type_d->fieldv->size; ++i) {
    FbldArg* arg_d = type_d->fieldv->xs[i];
    FblcType* arg_c = CompileForeignType(ctx, qref->mref, arg_d->type);
    FblcVectorAppend(ctx->arena, type_c->fieldv, arg_c);
  }

  return type_c;
}

// FbldCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity)
{
  Context ctx = {
    .arena = arena,
    .prgm = prgm,
    .accessv = accessv
  };
  FblcVectorInit(arena, ctx.typev);
  FblcVectorInit(arena, ctx.funcv);
  FblcVectorInit(arena, ctx.procv);

  FbldProc* proc_d = NULL;
  assert(entity->r.state == FBLD_RSTATE_RESOLVED);
  switch (entity->r.decl->tag) {
    case FBLD_PROC_DECL:
      proc_d = LookupProc(&ctx, entity);
      break;

    case FBLD_FUNC_DECL: {
      FbldFunc* func_d = LookupFunc(&ctx, entity);
      proc_d = FBLC_ALLOC(arena, FbldProc);
      proc_d->_base.name = func_d->_base.name;
      proc_d->portv = FBLC_ALLOC(arena, FbldPortV);
      FblcVectorInit(arena, *proc_d->portv);
      proc_d->_base.targv = func_d->_base.targv;
      proc_d->_base.margv = func_d->_base.margv;
      proc_d->argv = func_d->argv;
      proc_d->return_type = func_d->return_type;
      FbldEvalActn* body = FBLC_ALLOC(arena, FbldEvalActn);
      body->_base.tag = FBLD_EVAL_ACTN;
      body->_base.loc = func_d->body->loc;
      body->arg = func_d->body;
      proc_d->body = &body->_base;
      break;
    }

    default:
      fprintf(stderr, "Entry '%s' does not refer to a proc or func\n", entity->name->name);
      return NULL;
  }

  FblcProc* proc_c = CompileGivenProc(&ctx, entity, proc_d);
  FbldLoaded* loaded = FBLC_ALLOC(arena, FbldLoaded);
  loaded->prog = prgm;
  loaded->proc_d = proc_d;
  loaded->proc_c = proc_c;
  return loaded;
}

// CompileValue --
//   Compile an fbld value to an fblc value.
//
// Inputs:
//   ctx - The compilation context.
//   value - The value to compile.
//
// Result:
//   The compiled value.
//
// Side effects:
//   Behavior is undefined if the value is not well typed.
static FblcValue* CompileValue(Context* ctx, FbldValue* value)
{
  FbldType* type = LookupType(ctx, value->type);

  switch (value->kind) {
    case FBLD_STRUCT_KIND: {
      FblcValue* value_c = FblcNewStruct(ctx->arena, type->fieldv->size);
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        value_c->fields[i] = CompileValue(ctx, value->fieldv->xs[i]);
      }
      return value_c;
    }

    case FBLD_UNION_KIND: {
      FblcValue* arg = CompileValue(ctx, value->fieldv->xs[0]);
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (FbldNamesEqual(value->tag->name, type->fieldv->xs[i]->name->name)) {
          return FblcNewUnion(ctx->arena, type->fieldv->size, i, arg);
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

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  Context ctx = {
    .arena = arena,
    .prgm = prgm,
    .accessv = NULL,
  };
  FblcVectorInit(arena, ctx.typev);
  FblcVectorInit(arena, ctx.funcv);
  FblcVectorInit(arena, ctx.procv);
  return CompileValue(&ctx, value);
}

