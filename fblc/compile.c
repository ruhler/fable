// compile.c --
//   This file implements routines for compiling an fblcs program into an fblc
//   program.

#include <assert.h>     // for assert

#include "fblcs.h"

#define UNREACHABLE(x) assert(false && x)

static FblcType* LookupType(FblcsProgram* sprog, FblcProgram* prog, FblcsName name);
static FblcFunc* LookupFunc(FblcsProgram* sprog, FblcProgram* prog, FblcsName name);
static FblcProc* LookupProc(FblcsProgram* sprog, FblcProgram* prog, FblcsName name);
static FblcExpr* CompileExpr(FblcArena* arena, FblcsProgram* sprog, FblcProgram* prog, FblcsExpr* expr);
static FblcActn* CompileActn(FblcArena* arena, FblcsProgram* sprog, FblcProgram* prog, FblcsActn* actn);

// LookupType
//   Look up the fblc type with the given name.
//
// Inputs:
//   sprog - The fblcs program environment that has names.
//   proc - The fblc program environment that has the fblc types.
//   name - The name of the type to look up.
//
// Results:
//   The fblc type with the given name, or NULL to indicate no such type
//   was found.
//
// Side effects:
//   None.
FblcType* LookupType(FblcsProgram* sprog, FblcProgram* prog, FblcsName name)
{
  for (size_t i = 0; i < sprog->typev.size; ++i) {
    if (FblcsNamesEqual(sprog->typev.xs[i]->name.name, name)) {
      return prog->typev.xs[i];
    }
  }
  return NULL;
}

// LookupFunc
//   Look up the fblc function with the given name.
//
// Inputs:
//   sprog - The fblcs program environment that has names.
//   proc - The fblc program environment that has the fblc functions.
//   name - The name of the function to look up.
//
// Results:
//   The fblc function with the given name, or NULL to indicate no such
//   function was found.
//
// Side effects:
//   None.
FblcFunc* LookupFunc(FblcsProgram* sprog, FblcProgram* prog, FblcsName name)
{
  for (size_t i = 0; i < sprog->funcv.size; ++i) {
    if (FblcsNamesEqual(sprog->funcv.xs[i]->name.name, name)) {
      return prog->funcv.xs[i];
    }
  }
  return NULL;
}

// LookupProc
//   Look up the fblc process with the given name.
//
// Inputs:
//   sprog - The fblcs program environment that has names.
//   proc - The fblc program environment that has the fblc processes.
//   name - The name of the process to look up.
//
// Results:
//   The fblc process with the given name, or NULL to indicate no such process
//   was found.
//
// Side effects:
//   None.
FblcProc* LookupProc(FblcsProgram* sprog, FblcProgram* prog, FblcsName name)
{
  for (size_t i = 0; i < sprog->procv.size; ++i) {
    if (FblcsNamesEqual(sprog->procv.xs[i]->name.name, name)) {
      return prog->procv.xs[i];
    }
  }
  return NULL;
}

// CompileExpr --
//   Compile an fblcs expression to fblc.
//
// Inputs:
//   arena - The arena to use for allocations.
//   sprog - The fblcs program environment.
//   prog - The fblc program environment.
//   expr - The expression to compile.
//
// Results:
//   The compiled expression.
//
// Side effects:
//   Allocations the compiled expression.
//   Behavior is undefined if the expression and or program environment is not
//   well formed.
static FblcExpr* CompileExpr(FblcArena* arena, FblcsProgram* sprog, FblcProgram* prog, FblcsExpr* expr)
{
  switch (expr->tag) {
    case FBLCS_VAR_EXPR: {
      FblcsVarExpr* svar_expr = (FblcsVarExpr*)expr;
      FblcVarExpr* var_expr = FBLC_ALLOC(arena, FblcVarExpr);
      var_expr->_base.tag = FBLC_VAR_EXPR;
      var_expr->var = svar_expr->var.id;
      return &var_expr->_base;
    }

    case FBLCS_APP_EXPR: {
      FblcsAppExpr* sapp_expr = (FblcsAppExpr*)expr;
      FblcFunc* func = LookupFunc(sprog, prog, sapp_expr->func.name);
      if (func != NULL) {
        FblcAppExpr* app_expr = FBLC_ALLOC(arena, FblcAppExpr);
        FblcVectorInit(arena, app_expr->argv);
        app_expr->_base.tag = FBLC_APP_EXPR;
        app_expr->func = func;
        for (size_t i = 0; i < sapp_expr->argv.size; ++i) {
          FblcExpr* arg = CompileExpr(arena, sprog, prog, sapp_expr->argv.xs[i]);
          FblcVectorAppend(arena, app_expr->argv, arg);
        }
        return &app_expr->_base;
      } else {
        FblcStructExpr* struct_expr = FBLC_ALLOC(arena, FblcStructExpr);
        FblcVectorInit(arena, struct_expr->argv);
        struct_expr->_base.tag = FBLC_STRUCT_EXPR;
        struct_expr->type = LookupType(sprog, prog, sapp_expr->func.name);
        for (size_t i = 0; i < sapp_expr->argv.size; ++i) {
          FblcExpr* arg = CompileExpr(arena, sprog, prog, sapp_expr->argv.xs[i]);
          FblcVectorAppend(arena, struct_expr->argv, arg);
        }
        return &struct_expr->_base;
      }
    }

    case FBLCS_ACCESS_EXPR: {
      FblcsAccessExpr* saccess_expr = (FblcsAccessExpr*)expr;
      FblcAccessExpr* access_expr = FBLC_ALLOC(arena, FblcAccessExpr);
      access_expr->_base.tag = FBLC_ACCESS_EXPR;
      access_expr->obj = CompileExpr(arena, sprog, prog, saccess_expr->obj);
      access_expr->field = saccess_expr->field.id;
      return &access_expr->_base;
    }

    case FBLCS_UNION_EXPR: {
      FblcsUnionExpr* sunion_expr = (FblcsUnionExpr*)expr;
      FblcUnionExpr* union_expr = FBLC_ALLOC(arena, FblcUnionExpr);
      union_expr->_base.tag = FBLC_UNION_EXPR;
      union_expr->type = LookupType(sprog, prog, sunion_expr->type.name);
      union_expr->field = sunion_expr->field.id;
      union_expr->arg = CompileExpr(arena, sprog, prog, sunion_expr->arg);
      return &union_expr->_base;
    }

    case FBLCS_LET_EXPR: {
      FblcsLetExpr* slet_expr = (FblcsLetExpr*)expr;
      FblcLetExpr* let_expr = FBLC_ALLOC(arena, FblcLetExpr);
      let_expr->_base.tag = FBLC_LET_EXPR;
      let_expr->type = LookupType(sprog, prog, slet_expr->type.name);
      let_expr->def = CompileExpr(arena, sprog, prog, slet_expr->def);
      let_expr->body = CompileExpr(arena, sprog, prog, slet_expr->body);
      return &let_expr->_base;
    }

    case FBLCS_COND_EXPR: {
      FblcsCondExpr* scond_expr = (FblcsCondExpr*)expr;
      FblcCondExpr* cond_expr = FBLC_ALLOC(arena, FblcCondExpr);
      cond_expr->_base.tag = FBLC_COND_EXPR;
      cond_expr->select = CompileExpr(arena, sprog, prog, scond_expr->select);
      FblcVectorInit(arena, cond_expr->argv);
      for (size_t i = 0; i < scond_expr->argv.size; ++i) {
        FblcExpr* arg = CompileExpr(arena, sprog, prog, scond_expr->argv.xs[i]);
        FblcVectorAppend(arena, cond_expr->argv, arg);
      }
      return &cond_expr->_base;
    }

    default: {
      UNREACHABLE("invalid fblcs expression tag");
      return NULL;
    }
  }
}

// CompileActn --
//   Compile the given fblcs action to fblc.
//
// Inputs:
//   arena - Arena used for allocations.
//   sprog - The fblcs program environment.
//   prog - The fblc program environment.
//   actn - The action to compile.
//
// Results:
//   The compiled action.
//
// Side effects:
//   Allocates the compiled action. Behavior is undefined if the action and or
//   program environment is not well formed.
static FblcActn* CompileActn(FblcArena* arena, FblcsProgram* sprog, FblcProgram* prog, FblcsActn* actn)
{
  switch (actn->tag) {
    case FBLCS_EVAL_ACTN: {
      FblcsEvalActn* seval_actn = (FblcsEvalActn*)actn;
      FblcEvalActn* eval_actn = FBLC_ALLOC(arena, FblcEvalActn);
      eval_actn->_base.tag = FBLC_EVAL_ACTN;
      eval_actn->arg = CompileExpr(arena, sprog, prog, seval_actn->arg);
      return &eval_actn->_base;
    }

    case FBLCS_GET_ACTN: {
      FblcsGetActn* sget_actn = (FblcsGetActn*)actn;
      FblcGetActn* get_actn = FBLC_ALLOC(arena, FblcGetActn);
      get_actn->_base.tag = FBLC_GET_ACTN;
      get_actn->port = sget_actn->port.id;
      return &get_actn->_base;
    }

    case FBLCS_PUT_ACTN: {
      FblcsPutActn* sput_actn = (FblcsPutActn*)actn;
      FblcPutActn* put_actn = FBLC_ALLOC(arena, FblcPutActn);
      put_actn->_base.tag = FBLC_PUT_ACTN;
      put_actn->port = sput_actn->port.id;
      put_actn->arg = CompileExpr(arena, sprog, prog, sput_actn->arg);
      return &put_actn->_base;
    }

    case FBLCS_CALL_ACTN: {
      FblcsCallActn* scall_actn = (FblcsCallActn*)actn;
      FblcCallActn* call_actn = FBLC_ALLOC(arena, FblcCallActn);
      call_actn->_base.tag = FBLC_CALL_ACTN;
      call_actn->proc = LookupProc(sprog, prog, scall_actn->proc.name);
      FblcVectorInit(arena, call_actn->portv);
      for (size_t i = 0; i < scall_actn->portv.size; ++i) {
        FblcPortId port = scall_actn->portv.xs[i].id;
        FblcVectorAppend(arena, call_actn->portv, port);
      }
      FblcVectorInit(arena, call_actn->argv);
      for (size_t i = 0; i < scall_actn->argv.size; ++i) {
        FblcExpr* arg = CompileExpr(arena, sprog, prog, scall_actn->argv.xs[i]);
        FblcVectorAppend(arena, call_actn->argv, arg);
      }
      return &call_actn->_base;
    }

    case FBLCS_LINK_ACTN: {
      FblcsLinkActn* slink_actn = (FblcsLinkActn*)actn;
      FblcLinkActn* link_actn = FBLC_ALLOC(arena, FblcLinkActn);
      link_actn->_base.tag = FBLC_LINK_ACTN;
      link_actn->type = LookupType(sprog, prog, slink_actn->type.name);
      link_actn->body = CompileActn(arena, sprog, prog, slink_actn->body);
      return &link_actn->_base;
    }

    case FBLCS_EXEC_ACTN: {
      FblcsExecActn* sexec_actn = (FblcsExecActn*)actn;
      FblcExecActn* exec_actn = FBLC_ALLOC(arena, FblcExecActn);
      exec_actn->_base.tag = FBLC_EXEC_ACTN;
      FblcVectorInit(arena, exec_actn->execv);
      for (size_t i = 0; i < sexec_actn->execv.size; ++i) {
        FblcExec* exec = FblcVectorExtend(arena, exec_actn->execv);
        exec->type = LookupType(sprog, prog, sexec_actn->execv.xs[i].type.name);
        exec->actn = CompileActn(arena, sprog, prog, sexec_actn->execv.xs[i].actn);
      }
      exec_actn->body = CompileActn(arena, sprog, prog, sexec_actn->body);
      return &exec_actn->_base;
    }

    case FBLCS_COND_ACTN: {
      FblcsCondActn* scond_actn = (FblcsCondActn*)actn;
      FblcCondActn* cond_actn = FBLC_ALLOC(arena, FblcCondActn);
      cond_actn->_base.tag = FBLC_COND_ACTN;
      cond_actn->select = CompileExpr(arena, sprog, prog, scond_actn->select);
      FblcVectorInit(arena, cond_actn->argv);
      for (size_t i = 0; i < scond_actn->argv.size; ++i) {
        FblcActn* arg = CompileActn(arena, sprog, prog, scond_actn->argv.xs[i]);
        FblcVectorAppend(arena, cond_actn->argv, arg);
      }
      return &cond_actn->_base;
    }

    default: {
      UNREACHABLE("invalid fblcs actn tag");
      return NULL;
    }
  }
}

// FblcsCompileProgram -- see fblcs.h for documentation.
FblcsLoaded* FblcsCompileProgram(FblcArena* arena, FblcsProgram* sprog, FblcsName entry)
{
  if (!FblcsLookupProc(sprog, entry)) {
    FblcsFunc* func = FblcsLookupFunc(sprog, entry);
    if (func == NULL) {
      fprintf(stderr, "failed to find function or process named '%s'\n", entry);
      return NULL;
    }

    // The main entry is a function, not a process. Add a wrapper process to
    // the program to use as the main entry process.
    FblcsProc* proc = FBLC_ALLOC(arena, FblcsProc);
    proc->name = func->name;
    FblcVectorInit(arena, proc->portv);
    proc->argv = func->argv;
    proc->return_type = func->return_type;
    FblcsEvalActn* body = FBLC_ALLOC(arena, FblcsEvalActn);
    body->_base.tag = FBLCS_EVAL_ACTN;
    body->_base.loc = func->body->loc;
    body->arg = func->body;
    proc->body = &body->_base;
    FblcVectorAppend(arena, sprog->procv, proc);
  }

  // We store compiled types, functions, and processes in an FblcProgram that
  // parallels the given FblcsProgram. e.g. prog->funcv[i] is where the
  // compiled version of sprog->funcv[i] will be stored.
  FblcProgram* prog = FBLC_ALLOC(arena, FblcProgram);
  FblcVectorInit(arena, prog->typev);
  FblcVectorInit(arena, prog->funcv);
  FblcVectorInit(arena, prog->procv);

  // Pass 1: Pre-allocate all of the compiled entities so they can be referenced
  // when compiling other entities.
  for (size_t i = 0; i < sprog->typev.size; ++i) {
    FblcType* type = FBLC_ALLOC(arena, FblcType);
    FblcVectorAppend(arena, prog->typev, type);
  }
  for (size_t i = 0; i < sprog->funcv.size; ++i) {
    FblcFunc* func = FBLC_ALLOC(arena, FblcFunc);
    FblcVectorAppend(arena, prog->funcv, func);
  }
  for (size_t i = 0; i < sprog->procv.size; ++i) {
    FblcProc* proc = FBLC_ALLOC(arena, FblcProc);
    FblcVectorAppend(arena, prog->procv, proc);
  }

  // Pass 2: Compile all of the entities.
  for (size_t type_id = 0; type_id < sprog->typev.size; ++type_id) {
    FblcsType* stype = sprog->typev.xs[type_id];
    FblcType* type = prog->typev.xs[type_id];
    switch (stype->kind) {
      case FBLCS_STRUCT_KIND: type->kind = FBLC_STRUCT_KIND; break;
      case FBLCS_UNION_KIND: type->kind = FBLC_UNION_KIND; break;
      default: UNREACHABLE("invalid fblcs kind");
    }

    FblcVectorInit(arena, type->fieldv);
    for (size_t field_id = 0; field_id < stype->fieldv.size; ++field_id) {
      FblcType* field_type = LookupType(sprog, prog, stype->fieldv.xs[field_id].type.name);
      FblcVectorAppend(arena, type->fieldv, field_type);
    }
  }

  for (size_t func_id = 0; func_id < sprog->funcv.size; ++func_id) {
    FblcsFunc* sfunc = sprog->funcv.xs[func_id];
    FblcFunc* func = prog->funcv.xs[func_id];
    FblcVectorInit(arena, func->argv);
    for (size_t arg_id = 0; arg_id < sfunc->argv.size; ++arg_id) {
      FblcType* arg_type = LookupType(sprog, prog, sfunc->argv.xs[arg_id].type.name);
      FblcVectorAppend(arena, func->argv, arg_type);
    }
    func->return_type = LookupType(sprog, prog, sfunc->return_type.name);
    func->body = CompileExpr(arena, sprog, prog, sfunc->body);
  }

  for (size_t proc_id = 0; proc_id < sprog->procv.size; ++proc_id) {
    FblcsProc* sproc = sprog->procv.xs[proc_id];
    FblcProc* proc = prog->procv.xs[proc_id];

    FblcVectorInit(arena, proc->portv);
    for (size_t port_id = 0; port_id < sproc->portv.size; ++port_id) {
      FblcPort* port = FblcVectorExtend(arena, proc->portv);
      port->type = LookupType(sprog, prog, sproc->portv.xs[port_id].type.name);
      switch (sproc->portv.xs[port_id].polarity) {
        case FBLCS_GET_POLARITY: port->polarity = FBLC_GET_POLARITY; break;
        case FBLCS_PUT_POLARITY: port->polarity = FBLC_PUT_POLARITY; break;
        default: UNREACHABLE("invalid fblcs polarity");
      }
    }

    FblcVectorInit(arena, proc->argv);
    for (size_t arg_id = 0; arg_id < sproc->argv.size; ++arg_id) {
      FblcType* arg_type = LookupType(sprog, prog, sproc->argv.xs[arg_id].type.name);
      FblcVectorAppend(arena, proc->argv, arg_type);
    }

    proc->return_type = LookupType(sprog, prog, sproc->return_type.name);
    proc->body = CompileActn(arena, sprog, prog, sproc->body);
  }

  FblcsLoaded* loaded = FBLC_ALLOC(arena, FblcsLoaded);
  loaded->prog = sprog;
  loaded->sproc = FblcsLookupProc(sprog, entry);
  assert(loaded->sproc != NULL);
  loaded->proc = LookupProc(sprog, prog, entry);
  assert(loaded->proc != NULL);
  return loaded;
}
