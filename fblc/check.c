// check.c --
//   This file implements routines for checking a program is well formed and
//   well typed.

#include <assert.h>     // for assert

#include "fblcs.h"

#define UNREACHABLE(x) assert(false && x)

// Vars --
//   A mapping from variable id to type.
//
//   We typically allocate the Vars mapping on the stack, so it is not safe to
//   return or otherwise capture a Vars* outside of the frame where it is
//   allocated.
typedef struct Vars {
  FblcType* type;
  struct Vars* next;
} Vars;

// Ports --
//   A mapping from port id to port type and polarity.
//
//   We typically allocate the Ports mapping on the stack, so it is not safe
//   to return or otherwise capture a Ports* outside of the frame where it is
//   allocated.
typedef struct Ports {
  FblcType* type;
  FblcPolarity polarity;
  struct Ports* next;
} Ports;

static void CheckTypesMatch(FblcsProgram* sprog, FblcsLoc* loc, FblcType* expected, FblcType* actual, bool* error);
static Vars* AddVar(Vars* vars, FblcType* type, Vars* next);
static Ports* AddPort(Ports* vars, FblcType* type, FblcPolarity polarity, Ports* next);
static FblcType* CheckExpr(FblcsProgram* sprog, FblcsExprV* exprv, Vars* vars, FblcExpr* expr, bool* error);
static FblcType* CheckActn(FblcsProgram* sprog, FblcsActnV* actnv, FblcsExprV* exprv, Vars* vars, Ports* ports, FblcActn* actn, bool* error);

// AddVar --
//   Add a variable to the given scope.
//
// Inputs:
//   vars - Memory to use for the new scope.
//   type - The type of the variable.
//   next - The scope to add it to.
//
// Results:
//   The 'vars' scope with the added variable.
//
// Side effects:
//   Sets vars to a scope including the given variable and next scope.
static Vars* AddVar(Vars* vars, FblcType* type, Vars* next)
{
  vars->type = type;
  vars->next = next;
  return vars;
}

// AddPort --
//
//   Add a port to the given scope.
//
// Inputs:
//   ports - Memory to use for the new scope.
//   type - The type of the port.
//   polarity - The polarity of the port to add.
//   next - The scope to add it to.
//
// Results:
//   The 'ports' scope with the added get port.
//
// Side effects:
//   Sets ports to a scope including the given port and next scope.

static Ports* AddPort(Ports* ports, FblcType* type, FblcPolarity polarity, Ports* next)
{
  ports->type = type;
  ports->polarity = polarity;
  ports->next = next;
  return ports;
}

// CheckTypesMatch --
//   Check that the given types match.
//
// Inputs:
//   sprog - The program environment.
//   loc - The location of the reference to this declaration.
//   expected - The expected type.
//   actual - The actual type.
//   error - Out parameter set to false on error.
//
// Results:
//   None.
//
// Side effects:
//   If the actual type does not match the expected type, set error to true
//   and print an error message to stderr.
//   No message is printed if either type is invalid, under the assumption
//   that an error has already been reported in that case.
static void CheckTypesMatch(FblcsProgram* sprog, FblcsLoc* loc, FblcType* expected, FblcType* actual, bool* error)
{
  if (expected == NULL || actual == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  if (expected != actual) {
    FblcsReportError("Expected type %s, but found type %s.\n", loc, FblcsTypeName(sprog, expected), FblcsTypeName(sprog, actual));
    *error = true;
  }
}

// CheckExpr --
//   Verify the given expression is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   sprog - The program environment.
//   exprv - Symbol information for the expressions of the current declaration.
//   vars - The types of the variables in scope.
//   expr - The expression to check.
//   error - An out parameter to indicate error.
//
// Result:
//   The type of the expression, or NULL if the expression is not well formed
//   and well typed.
//
// Side effects:
//   If the expression is not well formed, an error message is printed to
//   stderr describing the problem with the expression and error is set to
//   true.
static FblcType* CheckExpr(FblcsProgram* sprog, FblcsExprV* exprv, Vars* vars, FblcExpr* expr, bool* error)
{
  FblcsExpr* sexpr = exprv->xs[expr->id];
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      for (size_t i = 0; vars != NULL && i < var_expr->var; ++i) {
        vars = vars->next;
      }
      if (vars == NULL) {
        FblcsVarExpr* svar_expr = (FblcsVarExpr*)sexpr;
        FblcsReportError("Variable '%s' not defined.\n", svar_expr->var.loc, svar_expr->var.name);
        *error = true;
        return NULL;
      }
      return vars->type;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      FblcsAppExpr* sapp_expr = (FblcsAppExpr*)sexpr;
      FblcFunc* func = app_expr->func;
      FblcTypeV* argv = &func->argv;
      if (argv != NULL && argv->size != app_expr->argv.size) {
        FblcsReportError("Expected %d arguments to %s, but %d were provided.\n", sapp_expr->func.loc, argv->size, sapp_expr->func.name, app_expr->argv.size);
        argv = NULL;
        *error = true;
      }

      for (size_t i = 0; i < app_expr->argv.size; ++i) {
        FblcType* arg_type = CheckExpr(sprog, exprv, vars, app_expr->argv.xs[i], error);
        if (argv != NULL) {
          CheckTypesMatch(sprog, exprv->xs[app_expr->argv.xs[i]->id]->loc, argv->xs[i], arg_type, error);
        }
      }
      return func->return_type;
    }

    case FBLC_STRUCT_EXPR: {
      FblcStructExpr* struct_expr = (FblcStructExpr*)expr;
      FblcsAppExpr* sapp_expr = (FblcsAppExpr*)sexpr;
      FblcType* type = struct_expr->type;
      FblcTypeV* argv = &type->fieldv;
      if (type->kind != FBLC_STRUCT_KIND) {
        FblcsReportError("Cannot do application on union type %s.\n", sapp_expr->func.loc, sapp_expr->func.name);
        *error = true;
      }

      if (argv != NULL && argv->size != struct_expr->argv.size) {
        FblcsReportError("Expected %d arguments to %s, but %d were provided.\n", sapp_expr->func.loc, argv->size, sapp_expr->func.name, struct_expr->argv.size);
        argv = NULL;
        *error = true;
      }

      for (size_t i = 0; i < struct_expr->argv.size; ++i) {
        FblcType* arg_type = CheckExpr(sprog, exprv, vars, struct_expr->argv.xs[i], error);
        if (argv != NULL) {
          CheckTypesMatch(sprog, exprv->xs[struct_expr->argv.xs[i]->id]->loc, argv->xs[i], arg_type, error);
        }
      }
      return type;
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcsAccessExpr* saccess_expr = (FblcsAccessExpr*)sexpr;
      FblcType* type = CheckExpr(sprog, exprv, vars, access_expr->obj, error);
      if (type == NULL) {
        return NULL;
      }

      if (access_expr->field < type->fieldv.size) {
        return type->fieldv.xs[access_expr->field];
      } else {
        FblcsReportError("%s is not a field of type %s\n", saccess_expr->field.loc, saccess_expr->field.name, FblcsTypeName(sprog, type));
        *error = true;
      }
      return NULL;
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      FblcsUnionExpr* sunion_expr = (FblcsUnionExpr*)sexpr;

      FblcType* field_type = NULL;
      if (union_expr->type->kind == FBLC_UNION_KIND) {
        if (union_expr->field < union_expr->type->fieldv.size) {
          field_type = union_expr->type->fieldv.xs[union_expr->field];
        } else {
          FblcsReportError("%s is not a valid field of the type %s", sunion_expr->field.loc, sunion_expr->field.name, sunion_expr->type.name);
          *error = true;
        }
      } else {
        FblcsReportError("%s does not refer to a union type.\n", sunion_expr->type.loc, sunion_expr->type.name);
        *error = true;
      }

      FblcType* expr_type = CheckExpr(sprog, exprv, vars, union_expr->arg, error);
      CheckTypesMatch(sprog, exprv->xs[union_expr->arg->id]->loc, field_type, expr_type, error);
      return union_expr->type;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      FblcsLetExpr* slet_expr = (FblcsLetExpr*)sexpr;
      if (let_expr->type == NULL) {
        FblcsReportError("%s does not refer to a type.\n", slet_expr->type.loc, slet_expr->type.name);
        *error = true;
      }
      FblcType* def_type = CheckExpr(sprog, exprv, vars, let_expr->def, error);
      CheckTypesMatch(sprog, exprv->xs[let_expr->def->id]->loc, let_expr->type, def_type, error);

      Vars nvars;
      AddVar(&nvars, let_expr->type, vars);
      return CheckExpr(sprog, exprv, &nvars, let_expr->body, error);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      FblcsCondExpr* scond_expr = (FblcsCondExpr*)sexpr;
      FblcType* type = CheckExpr(sprog, exprv, vars, cond_expr->select, error);
      if (type != NULL) {
        if (type->kind == FBLC_UNION_KIND) {
          if (type->fieldv.size != cond_expr->argv.size) {
            FblcsReportError("Expected %d arguments, but %d were provided.\n", scond_expr->_base.loc, type->fieldv.size, cond_expr->argv.size);
            *error = true;
          }
        } else {
          FblcsReportError("The condition has type %s, which is not a union type.\n", exprv->xs[cond_expr->select->id]->loc, FblcsTypeName(sprog, type));
          *error = true;
        }
      }

      assert(cond_expr->argv.size > 0);
      FblcType* result_type = NULL;
      for (size_t i = 0; i < cond_expr->argv.size; ++i) {
        FblcType* arg_type = CheckExpr(sprog, exprv, vars, cond_expr->argv.xs[i], error);
        if (i == 0) {
          result_type = arg_type;
        } else {
          CheckTypesMatch(sprog, exprv->xs[cond_expr->argv.xs[i]->id]->loc, result_type, arg_type, error);
        }
      }
      return result_type;
    }
  }
  UNREACHABLE("Invalid expression tag.");
  return NULL;
}

// CheckActn --
//   Verify the given action is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   sprog - The program environment.
//   actnv - Symbol information for the actions of the current declaration.
//   exprv - Symbol information for the expressions of the current declaration.
//   vars - The names and types of the variables in scope.
//   ports - The names, types, and polarities of the ports in scope.
//   actn - The action to verify.
//   error - An out parameter to indicate error.
//
// Result:
//   The type of the action or NULL if the expression is not well formed and
//   well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to stderr describing the problem with the action and error is set
//   to true.
static FblcType* CheckActn(FblcsProgram* sprog, FblcsActnV* actnv, FblcsExprV* exprv, Vars* vars, Ports* ports, FblcActn* actn, bool* error)
{
  FblcsActn* sactn = actnv->xs[actn->id];
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return CheckExpr(sprog, exprv, vars, eval_actn->arg, error);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      FblcsGetActn* sget_actn = (FblcsGetActn*)sactn;
      for (size_t i = 0; ports != NULL && i < get_actn->port; ++i) {
        ports = ports->next;
      }

      FblcType* port_type = NULL;
      if (ports == NULL) {
        FblcsReportError("Port '%s' not defined.\n", sget_actn->port.loc, sget_actn->port.name);
        *error = true;
      } else if (ports->polarity != FBLC_GET_POLARITY) {
        FblcsReportError("Port '%s' should have get polarity, but has put polarity.\n", sget_actn->port.loc, sget_actn->port.name);
        *error = true;
      } else {
        port_type = ports->type;
      }
      return port_type;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      FblcsPutActn* sput_actn = (FblcsPutActn*)sactn;
      for (size_t i = 0; ports != NULL && i < put_actn->port; ++i) {
        ports = ports->next;
      }

      FblcType* port_type = NULL;
      if (ports == NULL) {
        FblcsReportError("Port '%s' not defined.\n", sput_actn->port.loc, sput_actn->port.name);
        *error = true;
      } else if (ports->polarity != FBLC_PUT_POLARITY) {
        FblcsReportError("Port '%s' should have put polarity, but has get polarity.\n", sput_actn->port.loc, sput_actn->port.name);
        *error = true;
      } else {
        port_type = ports->type;
      }

      FblcType* arg_type = CheckExpr(sprog, exprv, vars, put_actn->arg, error);
      CheckTypesMatch(sprog, exprv->xs[put_actn->arg->id]->loc, port_type, arg_type, error);
      return port_type;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      FblcsCallActn* scall_actn = (FblcsCallActn*)sactn;
      FblcProc* proc_decl = call_actn->proc;

      bool check_ports = proc_decl != NULL;
      if (proc_decl != NULL && proc_decl->portv.size != call_actn->portv.size) {
        FblcsReportError("Expected %d port arguments to %s, but %d were provided.\n", scall_actn->proc.loc, proc_decl->portv.size, scall_actn->proc.name, call_actn->portv.size);
        *error = true;
        check_ports = false;
      }

      for (size_t i = 0; i < call_actn->portv.size; ++i) {
        Ports* curr = ports;
        for (size_t port_id = 0; curr != NULL && port_id < call_actn->portv.xs[i]; ++port_id) {
          curr = curr->next;
        }
        if (curr == NULL) {
          FblcsReportError("Port '%s' not defined.\n", scall_actn->portv.xs[i].loc, scall_actn->portv.xs[i].name);
          *error = true;
        } else {
          if (check_ports) {
            if (curr->polarity != proc_decl->portv.xs[i].polarity) {
              FblcsReportError("Port '%s' has wrong polarity. Expected '%s', but found '%s'.\n",
                  scall_actn->portv.xs[i].loc, scall_actn->portv.xs[i].name,
                  proc_decl->portv.xs[i].polarity == FBLC_PUT_POLARITY ? "put" : "get",
                  curr->polarity == FBLC_PUT_POLARITY ? "put" : "get");
              *error = true;
            }
            CheckTypesMatch(sprog, scall_actn->portv.xs[i].loc, proc_decl->portv.xs[i].type, curr->type, error);
          }
        }
      }

      bool check_args = proc_decl != NULL;
      if (proc_decl != NULL && proc_decl->argv.size != call_actn->argv.size) {
        FblcsReportError("Expected %d arguments to %s, but %d were provided.\n", scall_actn->proc.loc, proc_decl->argv.size, scall_actn->proc.name, call_actn->argv.size);
        *error = true;
        check_args = false;
      }

      for (size_t i = 0; i < call_actn->argv.size; ++i) {
        FblcType* arg_type = CheckExpr(sprog, exprv, vars, call_actn->argv.xs[i], error);
        if (check_args) {
          assert(proc_decl != NULL);
          CheckTypesMatch(sprog, exprv->xs[call_actn->argv.xs[i]->id]->loc, proc_decl->argv.xs[i], arg_type, error);
        }
      }

      return proc_decl == NULL ? NULL : proc_decl->return_type;
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      FblcsLinkActn* slink_actn = (FblcsLinkActn*)sactn;
      if (link_actn->type == NULL) {
        FblcsReportError("%s does not refer to a type.\n", slink_actn->type.loc, slink_actn->type.name);
        *error = true;
      }

      Ports getport;
      Ports putport;
      AddPort(&getport, link_actn->type, FBLC_GET_POLARITY, ports);
      AddPort(&putport, link_actn->type, FBLC_PUT_POLARITY, &getport);
      return CheckActn(sprog, actnv, exprv, vars, &putport, link_actn->body, error);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      FblcsExecActn* sexec_actn = (FblcsExecActn*)sactn;
      Vars vars_data[exec_actn->execv.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execv.size; ++i) {
        FblcExec* exec = exec_actn->execv.xs + i;
        if (exec->type == NULL) {
          FblcsTypedName* sexec = sexec_actn->execv.xs + i;
          FblcsReportError("%s does not refer to a type.\n", sexec->type.loc, sexec->type.name);
          *error = true;
        }
        FblcType* def_type = CheckActn(sprog, actnv, exprv, vars, ports, exec->actn, error);
        CheckTypesMatch(sprog, actnv->xs[exec->actn->id]->loc, exec->type, def_type, error);
        nvars = AddVar(vars_data+i, exec->type, nvars);
      }
      return CheckActn(sprog, actnv, exprv, nvars, ports, exec_actn->body, error);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      FblcsCondActn* scond_actn = (FblcsCondActn*)sactn;
      FblcType* type = CheckExpr(sprog, exprv, vars, cond_actn->select, error);
      if (type != NULL) {
        if (type->kind == FBLC_UNION_KIND) {
          if (type->fieldv.size != cond_actn->argv.size) {
            FblcsReportError("Expected %d arguments, but %d were provided.\n", scond_actn->_base.loc, type->fieldv.size, cond_actn->argv.size);
            *error = true;
          }
        } else {
          FblcsReportError("The condition has type %s, which is not a union type.\n", exprv->xs[cond_actn->select->id]->loc, FblcsTypeName(sprog, type));
          *error = true;
        }
      }

      assert(cond_actn->argv.size > 0);
      FblcType* result_type = NULL;
      for (size_t i = 0; i < cond_actn->argv.size; ++i) {
        FblcType* arg_type = CheckActn(sprog, actnv, exprv, vars, ports, cond_actn->argv.xs[i], error);
        if (i == 0) {
          result_type = arg_type;
        } else {
          CheckTypesMatch(sprog, actnv->xs[cond_actn->argv.xs[i]->id]->loc, result_type, arg_type, error);
        }
      }
      return result_type;
    }
  }
  UNREACHABLE("Invalid action tag.");
  return NULL;
}

// FblcsCheckProgram -- see documentation in fblcs.h
bool FblcsCheckProgram(FblcsProgram* sprog)
{
  bool error = false;

  // Check type declarations.
  for (size_t type_id = 0; type_id < sprog->program->typev.size; ++type_id) {
    FblcType* type = sprog->program->typev.xs[type_id];
    FblcsType* stype = sprog->stypev.xs[type_id];
    assert(type->kind == FBLC_STRUCT_KIND || type->fieldv.size > 0);
    for (FblcFieldId field_id = 0; field_id < type->fieldv.size; ++field_id) {
      if (type->fieldv.xs[field_id] == NULL) {
        FblcsTypedName* sfield = stype->fieldv.xs + field_id;
        FblcsReportError("%s does not refer to a type.\n", sfield->type.loc, sfield->type.name);
        error = true;
      }
    }
  }

  // Check func declarations
  for (size_t func_id = 0; func_id < sprog->program->funcv.size; ++func_id) {
    FblcFunc* func = sprog->program->funcv.xs[func_id];
    FblcsFunc* sfunc = sprog->sfuncv.xs[func_id];
    Vars nvars[func->argv.size];
    Vars* vars = NULL;
    for (size_t i = 0; i < func->argv.size; i++) {
      if (func->argv.xs[i] == NULL) {
        FblcsTypedName* sarg = sfunc->argv.xs + i;
        FblcsReportError("%s does not refer to a type.\n", sarg->type.loc, sarg->type.name);
        error = true;
      }
      vars = AddVar(nvars+i, func->argv.xs[i], vars);
    }
    if (func->return_type == NULL) {
      FblcsReportError("%s does not refer to a type.\n", sfunc->return_type.loc, sfunc->return_type.name);
      error = true;
    }
    FblcType* expr_type = CheckExpr(sprog, &sfunc->exprv, vars, func->body, &error);
    CheckTypesMatch(sprog, sfunc->exprv.xs[func->body->id]->loc, func->return_type, expr_type, &error);
  }

  // Check proc declarations
  for (size_t proc_id = 0; proc_id < sprog->program->procv.size; ++proc_id) {
    FblcProc* proc = sprog->program->procv.xs[proc_id];
    FblcsProc* sproc = sprog->sprocv.xs[proc_id];
    Ports nports[proc->portv.size];
    Ports* ports = NULL;
    for (size_t i = 0; i < proc->portv.size; ++i) {
      FblcPort* port = proc->portv.xs + i;
      if (port->type == NULL) {
        FblcsTypedName* sport = sproc->portv.xs + i;
        FblcsReportError("%s does not refer to a type.\n", sport->type.loc, sport->type.name);
        error = true;
      }
      ports = AddPort(nports + i, port->type, port->polarity, ports);
    }
    Vars nvars[proc->argv.size];
    Vars* vars = NULL;
    for (size_t i = 0; i < proc->argv.size; i++) {
      if (proc->argv.xs[i] == NULL) {
        FblcsTypedName* sarg = sproc->argv.xs + i;
        FblcsReportError("%s does not refer to a type.\n", sarg->type.loc, sarg->type.name);
        error = true;
      }
      vars = AddVar(nvars+i, proc->argv.xs[i], vars);
    }
    if (proc->return_type == NULL) {
      FblcsReportError("%s does not refer to a type.\n", sproc->return_type.loc, sproc->return_type.name);
      error = true;
    }
    FblcType* body_type = CheckActn(sprog, &sproc->actnv, &sproc->exprv, vars, ports, proc->body, &error);
    CheckTypesMatch(sprog, sproc->actnv.xs[proc->body->id]->loc, proc->return_type, body_type, &error);
  }
  return !error;
}
