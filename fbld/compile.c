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

// Compiled
//   A collection of already compiled entities.
typedef struct {
  CompiledTypeV typev;
  CompiledFuncV funcv;
  CompiledProcV procv;
} Compiled;

static FblcExpr* CompileExpr(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* mref, FbldExpr* expr, Compiled* compiled);
static FblcActn* CompileActn(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* mref, FbldActn* actn, Compiled* compiled);
static FblcType* CompileType(FblcArena* arena, FbldProgram* prgm, FbldQRef* entity, Compiled* compiled);
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity, Compiled* compiled);
static FblcProc* CompileProc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity, Compiled* compiled);
static FblcType* CompileForeignType(FblcArena* arena, FbldProgram* prgm, FbldQRef* mref, FbldQRef* entity, Compiled* compiled);

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
static FblcExpr* CompileExpr(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* mref, FbldExpr* expr, Compiled* compiled)
{
  switch (expr->tag) {
    case FBLD_VAR_EXPR: {
      FbldVarExpr* var_expr_d = (FbldVarExpr*)expr;
      FblcVarExpr* var_expr_c = FBLC_ALLOC(arena, FblcVarExpr);
      var_expr_c->_base.tag = FBLC_VAR_EXPR;
      var_expr_c->var = var_expr_d->var.id;
      return &var_expr_c->_base;
    }

    case FBLD_APP_EXPR: {
      FbldAppExpr* app_expr_d = (FbldAppExpr*)expr;
      FbldQRef* entity = FbldImportQRef(arena, prgm, mref, app_expr_d->func);
      FbldFunc* func = FbldLookupFunc(prgm, entity);
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

    case FBLD_UNION_EXPR: {
      FbldUnionExpr* union_expr_d = (FbldUnionExpr*)expr;
      FblcUnionExpr* union_expr_c = FBLC_ALLOC(arena, FblcUnionExpr);
      union_expr_c->_base.tag = FBLC_UNION_EXPR;
      union_expr_c->type = CompileForeignType(arena, prgm, mref, union_expr_d->type, compiled);
      union_expr_c->field = union_expr_d->field.id;
      union_expr_c->arg = CompileExpr(arena, accessv, prgm, mref, union_expr_d->arg, compiled);
      return &union_expr_c->_base;
    }

    case FBLD_ACCESS_EXPR: {
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

    case FBLD_COND_EXPR: {
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

    case FBLD_LET_EXPR: {
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

// CompileActn --
//   Return a compiled fblc actn for the given action.
//
// Inputs:
//   arena - Arena to use for allocations.
//   accessv - Collection of access expression debug info to return.
//   prgm - The program environment.
//   mref - The current module context.
//   actn - The action to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc actn.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds additional compiled types, functions, and processes to 'compiled' as needed.
static FblcActn* CompileActn(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* mref, FbldActn* actn, Compiled* compiled)
{
  switch (actn->tag) {
    case FBLD_EVAL_ACTN: {
      FbldEvalActn* eval_actn_d = (FbldEvalActn*)actn;
      FblcEvalActn* eval_actn_c = FBLC_ALLOC(arena, FblcEvalActn);
      eval_actn_c->_base.tag = FBLC_EVAL_ACTN;
      eval_actn_c->arg = CompileExpr(arena, accessv, prgm, mref, eval_actn_d->arg, compiled);
      return &eval_actn_c->_base;
    }

    case FBLD_GET_ACTN: {
      FbldGetActn* get_actn_d = (FbldGetActn*)actn;
      FblcGetActn* get_actn_c = FBLC_ALLOC(arena, FblcGetActn);
      get_actn_c->_base.tag = FBLC_GET_ACTN;
      get_actn_c->port = get_actn_d->port.id;
      return &get_actn_c->_base;
    }

    case FBLD_PUT_ACTN: {
      FbldPutActn* put_actn_d = (FbldPutActn*)actn;
      FblcPutActn* put_actn_c = FBLC_ALLOC(arena, FblcPutActn);
      put_actn_c->_base.tag = FBLC_PUT_ACTN;
      put_actn_c->port = put_actn_d->port.id;
      put_actn_c->arg = CompileExpr(arena, accessv, prgm, mref, put_actn_d->arg, compiled);
      return &put_actn_c->_base;
    }

    case FBLD_COND_ACTN: {
      FbldCondActn* cond_actn_d = (FbldCondActn*)actn;
      FblcCondActn* cond_actn_c = FBLC_ALLOC(arena, FblcCondActn);
      cond_actn_c->_base.tag = FBLC_COND_ACTN;
      cond_actn_c->select = CompileExpr(arena, accessv, prgm, mref, cond_actn_d->select, compiled);
      FblcVectorInit(arena, cond_actn_c->argv);
      for (size_t i = 0; i < cond_actn_d->argv->size; ++i) {
        FblcActn* arg = CompileActn(arena, accessv, prgm, mref, cond_actn_d->argv->xs[i], compiled);
        FblcVectorAppend(arena, cond_actn_c->argv, arg);
      }
      return &cond_actn_c->_base;
    }

    case FBLD_CALL_ACTN: {
      FbldCallActn* call_actn_d = (FbldCallActn*)actn;
      FblcCallActn* call_actn_c = FBLC_ALLOC(arena, FblcCallActn);
      call_actn_c->_base.tag = FBLC_CALL_ACTN;
      FbldQRef* entity = FbldImportQRef(arena, prgm, mref, call_actn_d->proc);
      call_actn_c->proc = CompileProc(arena, accessv, prgm, entity, compiled);
      FblcVectorInit(arena, call_actn_c->portv);
      for (size_t i = 0; i < call_actn_d->portv->size; ++i) {
        FblcPortId port = call_actn_d->portv->xs[i].id;
        FblcVectorAppend(arena, call_actn_c->portv, port);
      }
      FblcVectorInit(arena, call_actn_c->argv);
      for (size_t i = 0; i < call_actn_d->argv->size; ++i) {
        FblcExpr* arg = CompileExpr(arena, accessv, prgm, mref, call_actn_d->argv->xs[i], compiled);
        FblcVectorAppend(arena, call_actn_c->argv, arg);
      }
      return &call_actn_c->_base;
    }

    case FBLD_LINK_ACTN: {
      FbldLinkActn* link_actn_d = (FbldLinkActn*)actn;
      FblcLinkActn* link_actn_c = FBLC_ALLOC(arena, FblcLinkActn);
      link_actn_c->_base.tag = FBLC_LINK_ACTN;
      FbldQRef* entity = FbldImportQRef(arena, prgm, mref, link_actn_d->type);
      link_actn_c->type = CompileType(arena, prgm, entity, compiled);
      link_actn_c->body = CompileActn(arena, accessv, prgm, mref, link_actn_d->body, compiled);
      return &link_actn_c->_base;
    }

    case FBLD_EXEC_ACTN: {
      FbldExecActn* exec_actn_d = (FbldExecActn*)actn;
      FblcExecActn* exec_actn_c = FBLC_ALLOC(arena, FblcExecActn);
      exec_actn_c->_base.tag = FBLC_EXEC_ACTN;
      FblcVectorInit(arena, exec_actn_c->execv);
      for (size_t i = 0; i < exec_actn_d->execv->size; ++i) {
        FblcExec* exec = FblcVectorExtend(arena, exec_actn_c->execv);
        FbldQRef* entity = FbldImportQRef(arena, prgm, mref, exec_actn_d->execv->xs[i].type);
        exec->type = CompileType(arena, prgm, entity, compiled);
        exec->actn = CompileActn(arena, accessv, prgm, mref, exec_actn_d->execv->xs[i].actn, compiled);
      }
      exec_actn_c->body = CompileActn(arena, accessv, prgm, mref, exec_actn_d->body, compiled);
      return &exec_actn_c->_base;
    }

    default: {
      UNREACHABLE("invalid fbld action tag");
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
static FblcType* CompileType(FblcArena* arena, FbldProgram* prgm, FbldQRef* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->typev.size; ++i) {
    if (FbldQRefsEqual(compiled->typev.xs[i].entity, entity)) {
      return compiled->typev.xs[i].compiled;
    }
  }

  FbldType* type_d = FbldLookupType(prgm, entity);
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
    FblcType* arg_c = CompileForeignType(arena, prgm, entity->rmref, arg_d->type, compiled);
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
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->funcv.size; ++i) {
    if (FbldQRefsEqual(compiled->funcv.xs[i].entity, entity)) {
      return compiled->funcv.xs[i].compiled;
    }
  }

  FbldFunc* func_d = FbldLookupFunc(prgm, entity);
  assert(func_d != NULL);

  FblcFunc* func_c = FBLC_ALLOC(arena, FblcFunc);
  CompiledFunc* compiled_func = FblcVectorExtend(arena, compiled->funcv);
  compiled_func->entity = entity;
  compiled_func->compiled = func_c;

  FblcVectorInit(arena, func_c->argv);
  for (size_t arg_id = 0; arg_id < func_d->argv->size; ++arg_id) {
    FblcType* arg_type = CompileForeignType(arena, prgm, entity->rmref, func_d->argv->xs[arg_id]->type, compiled);
    FblcVectorAppend(arena, func_c->argv, arg_type);
  }
  func_c->return_type = CompileForeignType(arena, prgm, entity->rmref, func_d->return_type, compiled);
  func_c->body = CompileExpr(arena, accessv, prgm, entity->rmref, func_d->body, compiled);

  return func_c;
}

// CompileProc --
//   Return a compiled fblc proc for the named process.
//
// Inputs:
//   arena - Arena to use for allocations.
//   accessv - Collection of access expression debug info to return.
//   prgm - The program environment.
//   entity - The process to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc proc, or NULL in case of error.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds the compiled process to 'compiled' if it is newly compiled.
static FblcProc* CompileProc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->procv.size; ++i) {
    if (FbldQRefsEqual(compiled->procv.xs[i].entity, entity)) {
      return compiled->procv.xs[i].compiled;
    }
  }

  FbldProc* proc_d = FbldLookupProc(prgm, entity);
  assert(proc_d != NULL);

  FblcProc* proc_c = FBLC_ALLOC(arena, FblcProc);
  CompiledProc* compiled_proc = FblcVectorExtend(arena, compiled->procv);
  compiled_proc->entity = entity;
  compiled_proc->compiled = proc_c;

  FblcVectorInit(arena, proc_c->portv);
  for (size_t i = 0; i < proc_d->portv->size; ++i) {
    FblcPort* port = FblcVectorExtend(arena, proc_c->portv);
    port->type = CompileForeignType(arena, prgm, entity->rmref, proc_d->portv->xs[i].type, compiled);
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

  FblcVectorInit(arena, proc_c->argv);
  for (size_t arg_id = 0; arg_id < proc_d->argv->size; ++arg_id) {
    FblcType* arg_type = CompileForeignType(arena, prgm, entity->rmref, proc_d->argv->xs[arg_id]->type, compiled);
    FblcVectorAppend(arena, proc_c->argv, arg_type);
  }
  proc_c->return_type = CompileForeignType(arena, prgm, entity->rmref, proc_d->return_type, compiled);
  proc_c->body = CompileActn(arena, accessv, prgm, entity->rmref, proc_d->body, compiled);
  return proc_c;
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
static FblcType* CompileForeignType(FblcArena* arena, FbldProgram* prgm, FbldQRef* mref, FbldQRef* entity, Compiled* compiled)
{
  FbldQRef* resolved = FbldImportQRef(arena, prgm, mref, entity);
  return CompileType(arena, prgm, resolved, compiled);
}

// FbldCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity)
{
  FbldProc* proc_d = FbldLookupProc(prgm, entity);
  if (proc_d == NULL) {
    FbldFunc* func_d = FbldLookupFunc(prgm, entity);
    if (func_d == NULL) {
      fprintf(stderr, "main entry not found\n");
      return NULL;
    }

    // The main entry is a function, not a process. Add a wrapper process to
    // the program to use as the main entry process.
    proc_d = FBLC_ALLOC(arena, FbldProc);
    proc_d->name = func_d->name;
    proc_d->portv = FBLC_ALLOC(arena, FbldPortV);
    FblcVectorInit(arena, *proc_d->portv);
    proc_d->argv = func_d->argv;
    proc_d->return_type = func_d->return_type;
    FbldEvalActn* body = FBLC_ALLOC(arena, FbldEvalActn);
    body->_base.tag = FBLD_EVAL_ACTN;
    body->_base.loc = func_d->body->loc;
    body->arg = func_d->body;
    proc_d->body = &body->_base;

    FbldModule* module = FbldLookupModule(prgm, entity->rmref->name);
    FblcVectorAppend(arena, *module->procv, proc_d);
  }

  Compiled compiled;
  FblcVectorInit(arena, compiled.typev);
  FblcVectorInit(arena, compiled.funcv);
  FblcVectorInit(arena, compiled.procv);
  FblcProc* proc_c = CompileProc(arena, accessv, prgm, entity, &compiled);

  FbldLoaded* loaded = FBLC_ALLOC(arena, FbldLoaded);
  loaded->prog = prgm;
  loaded->proc_d = proc_d;
  loaded->proc_c = proc_c;
  return loaded;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  FbldType* type = FbldLookupType(prgm, value->type);
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
