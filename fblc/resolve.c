// resolve.c --
//   This file provides routines for performing resolution of references to
//   variables, ports, declarations, and fields of an fblc program.

#include <assert.h>     // for assert

#include "fblcs.h"

#define UNREACHABLE(x) assert(false && x)

typedef struct Vars {
  FblcType* type;
  FblcsName name;
  struct Vars* next;
} Vars;

static Vars* AddVar(Vars* vars, FblcType* type, FblcsName name, Vars* next);
static FblcType* LookupType(FblcsProgram* sprog, FblcsNameL* type);
static FblcProc* LookupProc(FblcsProgram* sprog, FblcsNameL* proc);
static FblcFieldId LookupField(FblcsProgram* sprog, FblcType* type, FblcsNameL* field);
static FblcType* FieldType(FblcsProgram* sprog, FblcType* type, FblcFieldId field_id);
static FblcType* ResolveExpr(FblcsProgram* sprog, FblcsExprV* exprv, Vars* vars, FblcExpr* expr);
static FblcType* ResolveActn(FblcsProgram* sprog, FblcsActnV* actnv, FblcsExprV* exprv, Vars* vars, Vars* ports, FblcActn* actn);

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
static Vars* AddVar(Vars* vars, FblcType* type, FblcsName name, Vars* next)
{
  vars->type = type;
  vars->name = name;
  vars->next = next;
  return vars;
}

// LookupType --
//   Look up the type for the give type name.
//
// Inputs:
//   sprog - The program environment.
//   type - The name and location of the type to look up.
//
// Results:
//   The type referred to at that given location of the program, or
//   NULL if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcType* LookupType(FblcsProgram* sprog, FblcsNameL* type)
{
  FblcType* decl = FblcsLookupType(sprog, type->name);
  if (decl == NULL) {
    FblcsReportError("Type %s not declared.\n", type->loc, type->name);
    return NULL;
  }
  return decl;
}

// LookupProc --
//   Look up the process declaration for the give process name.
//
// Inputs:
//   sprog - The program environment.
//   proc - The name and location of the process to look up.
//
// Results:
//   The process referred to at that given location of the program, or
//   NULL if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcProc* LookupProc(FblcsProgram* sprog, FblcsNameL* proc)
{
  FblcProc* decl = FblcsLookupProc(sprog, proc->name);
  if (decl == NULL) {
    FblcsReportError("Process %s not declared.\n", proc->loc, proc->name);
    return NULL;
  }
  return decl;
}

// LookupField --
//   Look up the field id for the field referred to from the given location.
//
// Inputs:
//   sprog - The program environment.
//   type - The type to look up the field in.
//   field - The name and location of the field to look up.
//
// Results:
//   The field id corresponding to the field referred to at that given
//   location of the program, or FBLC_NULL_ID if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.

static FblcFieldId LookupField(FblcsProgram* sprog, FblcType* type, FblcsNameL* field)
{
  FblcFieldId id = FblcsLookupField(sprog, type, field->name);
  if (id == FBLC_NULL_ID) {
    FblcsReportError("'%s' is not a field of the type '%s'.\n", field->loc, field->name, FblcsTypeName(sprog, type));
    return FBLC_NULL_ID;
  }
  return id;
}

// FieldType --
//   Look up the type for the given field of a type declaration.
//
// Inputs:
//   sprog - The program environment.
//   type - The type declaration.
//   field_id - The id of the field.
//
// Results:
//   The type for the type of the given field in the given type
//   declaration, or NULL if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcType* FieldType(FblcsProgram* sprog, FblcType* type, FblcFieldId field_id)
{
  assert(field_id < type->fieldv.size);
  FblcsType* stype = sprog->stypev.xs[type->id];
  return LookupType(sprog, &stype->fieldv.xs[field_id].type);
}

// ResolveExpr --
//   Perform id/name resolution for references to variables, ports,
//   declarations, and fields in the given expression.
//
// Inputs:
//   sprog - The program environment.
//   exprv - Symbol information for the expressions from the current declaration.
//   vars - The variables currently in scope.
//   expr - The expression to perform name resolution on.
//
// Results:
//   The type of the expression, or NULL if name resolution failed.
//
// Side effects:
//   IDs in the expression are resolved.
//   Prints error messages to stderr in case of error.
static FblcType* ResolveExpr(FblcsProgram* sprog, FblcsExprV* exprv, Vars* vars, FblcExpr* expr)
{
  FblcsExpr* sexpr = exprv->xs[expr->id];
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      FblcsVarExpr* svar_expr = (FblcsVarExpr*)sexpr;
      for (size_t i = 0; vars != NULL; ++i) {
        if (FblcsNamesEqual(vars->name, svar_expr->var.name)) {
          var_expr->var = i;
          return vars->type;
        }
        vars = vars->next;
      }
      FblcsReportError("variable '%s' not defined.", svar_expr->var.loc, svar_expr->var.name);
      return NULL;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      FblcsAppExpr* sapp_expr = (FblcsAppExpr*)sexpr;

      for (size_t i = 0; i < app_expr->argv.size; ++i) {
        if (ResolveExpr(sprog, exprv, vars, app_expr->argv.xs[i]) == NULL) {
          return NULL;
        }
      }

      FblcFunc* func = FblcsLookupFunc(sprog, sapp_expr->func.name); 
      if (func != NULL) {
        app_expr->func = func;
        FblcsFunc* sfunc = sprog->sfuncv.xs[func->id];
        return LookupType(sprog, &sfunc->return_type);
      }

      FblcType* type = FblcsLookupType(sprog, sapp_expr->func.name);
      if (type != NULL) {
        // This is a struct expression. Treat it as such.
        expr->tag = FBLC_STRUCT_EXPR;
        FblcStructExpr* struct_expr = (FblcStructExpr*)expr;
        struct_expr->type = type;
        return type;
      }

      FblcsReportError("'%s' not defined.\n", sapp_expr->func.loc, sapp_expr->func.name);
      return NULL;
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcsAccessExpr* saccess_expr = (FblcsAccessExpr*)sexpr;

      FblcType* type = ResolveExpr(sprog, exprv, vars, access_expr->obj);
      if (type == NULL) {
        return NULL;
      }

      access_expr->field = LookupField(sprog, type, &saccess_expr->field);
      if (access_expr->field == FBLC_NULL_ID) {
        return NULL;
      }
      return FieldType(sprog, type, access_expr->field);
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      FblcsUnionExpr* sunion_expr = (FblcsUnionExpr*)sexpr;

      union_expr->type = LookupType(sprog, &sunion_expr->type);
      if (union_expr->type == NULL) {
        return NULL;
      }

      union_expr->field = LookupField(sprog, union_expr->type, &sunion_expr->field);
      if (union_expr->field == FBLC_NULL_ID) {
        return NULL;
      }

      if (ResolveExpr(sprog, exprv, vars, union_expr->arg) == NULL) {
        return NULL;
      }
      return union_expr->type;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      FblcsLetExpr* slet_expr = (FblcsLetExpr*)sexpr;

      let_expr->type = LookupType(sprog, &slet_expr->type);
      if (let_expr->type == NULL) {
        return NULL;
      }

      if (ResolveExpr(sprog, exprv, vars, let_expr->def) == NULL) {
        return NULL;
      }

      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, slet_expr->name.name)) {
          FblcsReportError("Redefinition of variable '%s'\n", slet_expr->name.loc, slet_expr->name.name);
          return NULL;
        }
      }

      Vars nvars;
      AddVar(&nvars, let_expr->type, slet_expr->name.name, vars);
      return ResolveExpr(sprog, exprv, &nvars, let_expr->body);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      if (ResolveExpr(sprog, exprv, vars, cond_expr->select) == NULL) {
        return NULL;
      }

      FblcType* result_type = NULL;
      assert(cond_expr->argv.size > 0);
      for (size_t i = 0; i < cond_expr->argv.size; ++i) {
        result_type = ResolveExpr(sprog, exprv, vars, cond_expr->argv.xs[i]);
        if (result_type == NULL) {
          return NULL;
        }
      }
      return result_type;
    }

    default: {
      UNREACHABLE("Invalid Case");
      return NULL;
    }
  }
}

// ResolveActn --
//   Perform id/name resolution for references to variables, ports,
//   declarations, and fields in the given action.
//
// Inputs:
//   sprog - The program environment.
//   actnv - Symbol information for the actions from the current declaration.
//   exprv - Symbol information for the expressions from the current declaration.
//   vars - The variables currently in scope.
//   ports - The ports currently in scope.
//   actn - The action to perform name resolution on.
//
// Results:
//   The type of the action, or NULL if name resolution failed.
//
// Side effects:
//   IDs in the action are resolved.
//   Prints error messages to stderr in case of error.
static FblcType* ResolveActn(FblcsProgram* sprog, FblcsActnV* actnv, FblcsExprV* exprv, Vars* vars, Vars* ports, FblcActn* actn)
{
  FblcsActn* sactn = actnv->xs[actn->id];
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return ResolveExpr(sprog, exprv, vars, eval_actn->arg);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      FblcsGetActn* sget_actn = (FblcsGetActn*)sactn;

      for (size_t i = 0; ports != NULL; ++i) {
        if (FblcsNamesEqual(ports->name, sget_actn->port.name)) {
          get_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      FblcsReportError("port '%s' not defined.\n", sget_actn->port.loc, sget_actn->port.name);
      return NULL;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      FblcsPutActn* sput_actn = (FblcsPutActn*)sactn;

      if (ResolveExpr(sprog, exprv, vars, put_actn->arg) == NULL) {
        return NULL;
      }

      for (size_t i = 0; ports != NULL; ++i) {
        if (FblcsNamesEqual(ports->name, sput_actn->port.name)) {
          put_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      FblcsReportError("port '%s' not defined.\n", sput_actn->port.loc, sput_actn->port.name);
      return NULL;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      FblcsCallActn* scall_actn = (FblcsCallActn*)sactn;

      call_actn->proc = LookupProc(sprog, &scall_actn->proc);
      if (call_actn->proc == NULL) {
        return NULL;
      }

      for (size_t i = 0; i < call_actn->portv.size; ++i) {
        call_actn->portv.xs[i] = FBLC_NULL_ID;
        FblcsNameL* port = scall_actn->portv.xs + i;
        Vars* curr = ports;
        for (size_t port_id = 0; curr != NULL; ++port_id) {
          if (FblcsNamesEqual(curr->name, port->name)) {
            call_actn->portv.xs[i] = port_id;
            break;
          }
          curr = curr->next;
        }
        if (call_actn->portv.xs[i] == FBLC_NULL_ID) {
          FblcsReportError("port '%s' not defined.\n", port->loc, port->name);
          return NULL;
        }
      }

      for (size_t i = 0 ; i < call_actn->argv.size; ++i) {
        if (ResolveExpr(sprog, exprv, vars, call_actn->argv.xs[i]) == NULL) {
          return NULL;
        }
      }

      FblcsProc* sproc = sprog->sprocv.xs[call_actn->proc->id];
      return LookupType(sprog, &sproc->return_type);
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      FblcsLinkActn* slink_actn = (FblcsLinkActn*)sactn;
      link_actn->type = LookupType(sprog, &slink_actn->type);
      if (link_actn->type == NULL) {
        return NULL;
      }

      for (Vars* curr = ports; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, slink_actn->get.name)) {
          FblcsReportError("Redefinition of port '%s'\n", slink_actn->get.loc, slink_actn->get.name);
          return NULL;
        }
        if (FblcsNamesEqual(curr->name, slink_actn->put.name)) {
          FblcsReportError("Redefinition of port '%s'\n", slink_actn->put.loc, slink_actn->put.name);
          return NULL;
        }
      }

      Vars getport;
      Vars putport;
      AddVar(&getport, link_actn->type, slink_actn->get.name, ports);
      AddVar(&putport, link_actn->type, slink_actn->put.name, &getport);
      return ResolveActn(sprog, actnv, exprv, vars, &putport, link_actn->body);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      FblcsExecActn* sexec_actn = (FblcsExecActn*)sactn;

      Vars vars_data[exec_actn->execv.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execv.size; ++i) {
        FblcExec* exec = exec_actn->execv.xs + i;
        FblcsTypedName* var = sexec_actn->execv.xs + i;
        exec->type = LookupType(sprog, &var->type);
        if (ResolveActn(sprog, actnv, exprv, vars, ports, exec->actn) == NULL) {
          return NULL;
        }
        nvars = AddVar(vars_data+i, exec->type, var->name.name, nvars);
      }
      return ResolveActn(sprog, actnv, exprv, nvars, ports, exec_actn->body);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      if (ResolveExpr(sprog, exprv, vars, cond_actn->select) == NULL) {
        return NULL;
      }

      FblcType* result_type = NULL;
      assert(cond_actn->argv.size > 0);
      for (size_t i = 0; i < cond_actn->argv.size; ++i) {
        result_type = ResolveActn(sprog, actnv, exprv, vars, ports, cond_actn->argv.xs[i]);
        if (result_type == NULL) {
          return NULL;
        }
      }
      return result_type;
    }

    default: {
      UNREACHABLE("Invalid Case");
      return NULL;
    }
  }
}

// FblcsResolveProgram -- see fblcs.h for documentation.
bool FblcsResolveProgram(FblcsProgram* sprog)
{
  // Resolve type declarations.
  for (size_t type_id = 0; type_id < sprog->program->typev.size; ++type_id) {
    FblcType* type = sprog->program->typev.xs[type_id];
    FblcsType* stype = sprog->stypev.xs[type_id];

    if (FblcsLookupType(sprog, stype->name.name) != type) {
      FblcsReportError("Redefinition of %s\n", stype->name.loc, stype->name.name);
      return false;
    }

    for (FblcFieldId field_id = 0; field_id < type->fieldv.size; ++field_id) {
      FblcsNameL* field = &stype->fieldv.xs[field_id].name;
      for (FblcFieldId i = 0; i < field_id; ++i) {
        if (FblcsNamesEqual(field->name, stype->fieldv.xs[i].name.name)) {
          FblcsReportError("Redefinition of field %s\n", field->loc, field->name);
          return false;
        }
      }

      type->fieldv.xs[field_id] = FieldType(sprog, type, field_id);
      if (type->fieldv.xs[field_id] == NULL) {
        return false;
      }
    }
  }

  for (size_t func_id = 0; func_id < sprog->program->funcv.size; ++func_id) {
    FblcFunc* func = sprog->program->funcv.xs[func_id];
    FblcsFunc* sfunc = sprog->sfuncv.xs[func_id];

    if (FblcsLookupType(sprog, sfunc->name.name) != NULL ||
        FblcsLookupFunc(sprog, sfunc->name.name) != func) {
      FblcsReportError("Redefinition of %s\n", sfunc->name.loc, sfunc->name.name);
      return false;
    }

    Vars nvars[func->argv.size];
    Vars* vars = NULL;
    for (size_t i = 0; i < func->argv.size; ++i) {
      FblcsTypedName* var = sfunc->argv.xs + i;
      func->argv.xs[i] = LookupType(sprog, &var->type);
      if (func->argv.xs[i] == NULL) {
        return false;
      }
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, var->name.name)) {
          FblcsReportError("Redefinition of argument '%s'\n", var->name.loc, var->name.name);
          return false;
        }
      }
      vars = AddVar(nvars+i, func->argv.xs[i], var->name.name, vars);
    }

    func->return_type = LookupType(sprog, &sfunc->return_type);
    if (func->return_type == NULL) {
      return false;
    }

    if (ResolveExpr(sprog, &sfunc->exprv, vars, func->body) == NULL) {
      return false;
    }
  }

  for (size_t proc_id = 0; proc_id < sprog->program->procv.size; ++proc_id) {
    FblcProc* proc = sprog->program->procv.xs[proc_id];
    FblcsProc* sproc = sprog->sprocv.xs[proc_id];

    if (FblcsLookupType(sprog, sproc->name.name) != NULL ||
        FblcsLookupFunc(sprog, sproc->name.name) != NULL ||
        FblcsLookupProc(sprog, sproc->name.name) != proc) {
      FblcsReportError("Redefinition of %s\n", sproc->name.loc, sproc->name.name);
      return false;
    }

    Vars nports[proc->portv.size];
    Vars* ports = NULL;
    for (size_t i = 0; i < proc->portv.size; ++i) {
      FblcsTypedName* port = sproc->portv.xs + i;
      proc->portv.xs[i].type = LookupType(sprog, &port->type);
      if (proc->portv.xs[i].type == NULL) {
        return false;
      }
      for (Vars* curr = ports; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, port->name.name)) {
          FblcsReportError("Redefinition of port '%s'\n", port->name.loc, port->name.name);
          return false;
        }
      }
      ports = AddVar(nports+i, proc->portv.xs[i].type, port->name.name, ports);
    }

    Vars nvars[proc->argv.size];
    Vars* vars = NULL;
    for (size_t i = 0; i < proc->argv.size; ++i) {
      FblcsTypedName* var = sproc->argv.xs + i;
      proc->argv.xs[i] = LookupType(sprog, &var->type);
      if (proc->argv.xs[i] == NULL) {
        return false;
      }
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, var->name.name)) {
          FblcsReportError("Redefinition of argument '%s'\n", var->name.loc, var->name.name);
          return false;
        }
      }
      vars = AddVar(nvars+i, proc->argv.xs[i], var->name.name, vars);
    }

    proc->return_type = LookupType(sprog, &sproc->return_type);
    if (proc->return_type == NULL) {
      return false;
    }

    if (ResolveActn(sprog, &sproc->actnv, &sproc->exprv, vars, ports, proc->body) == NULL) {
      return false;
    }
  }
  return true;
}
