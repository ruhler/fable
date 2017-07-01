// check.c --
//   This file implements routines for checking a program is well formed and
//   well typed.

#include <assert.h>     // for assert
#include <stdarg.h>     // For va_start
#include <stdio.h>      // For fprintf

#include "fblcs.h"

#define UNREACHABLE(x) assert(false && x)

// Vars --
//   A mapping from variable name to type.
typedef struct Vars {
  FblcsType* type;
  const char* name;
  struct Vars* next;
} Vars;

// Ports --
//   A mapping from port name to port type and polarity.
typedef struct Ports {
  FblcsType* type;
  const char* name;
  FblcsPolarity polarity;
  struct Ports* next;
} Ports;

void ReportError(const char* format, bool* error, FblcsLoc* loc, ...);
static FblcsType* CheckType(FblcsProgram* prog, FblcsName* name, bool* error);
static void CheckTypesMatch(FblcsLoc* loc, FblcsType* expected, FblcsType* actual, bool* error);
static FblcsType* CheckExpr(FblcsProgram* prog, Vars* vars, FblcsExpr* expr, bool* error);
static FblcsType* CheckActn(FblcsProgram* prog, Vars* vars, Ports* ports, FblcsActn* actn, bool* error);

// ReportError --
//   Report an error message associated with a location in a source file.
//   Used instead of FblcsReportError to make it easier to remember to set the
//   global error variable to false if any error is reported.
//
// Inputs:
//   format - A printf format string for the error message.
//   error - A boolean variable to set to false.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
//   Sets 'error' to false.
void ReportError(const char* format, bool* error, FblcsLoc* loc, ...)
{
  *error = true;

  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// CheckType --
//   Check that a type name is valid.
//  
// Inputs:
//   prog - The program environment.
//   name - The type name to check.
//   error - Boolean to set to false on error.
//
// Returns:
//   The type declaration referred to by the type name, or NULL if there is no
//   such type defined.
//
// Side effects:
//   Prints an error message to stderr and sets error to false if the type
//   name is not valid.
static FblcsType* CheckType(FblcsProgram* prog, FblcsName* name, bool* error)
{
  FblcsType* type = FblcsLookupType(prog, name->name);
  if (type == NULL) {
    ReportError("%s does not refer to a type.\n", error, name->loc, name->name);
  }
  return type;
}

// CheckTypesMatch --
//   Check that the given types match.
//
// Inputs:
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
static void CheckTypesMatch(FblcsLoc* loc, FblcsType* expected, FblcsType* actual, bool* error)
{
  if (expected == NULL || actual == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  if (expected != actual) {
    ReportError("Expected type %s, but found type %s.\n", error, loc, expected->name.name, actual->name.name, error);
  }
}

// CheckExpr --
//   Verify the given expression is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   prog - The program environment.
//   vars - The variables currently in scope.
//   expr - The expression to perform name resolution on.
//   error - An out parameter to indicate error.
//
// Results:
//   The type of the expression, or NULL if the expression is not well formed
//   and well typed.
//
// Side effects:
//   If the expression is not well formed, an error message is printed to
//   stderr describing the problem with the expression and error is set to
//   true.
static FblcsType* CheckExpr(FblcsProgram* prog, Vars* vars, FblcsExpr* expr, bool* error)
{
  switch (expr->tag) {
    case FBLCS_VAR_EXPR: {
      FblcsVarExpr* var_expr = (FblcsVarExpr*)expr;
      for (size_t i = 0; vars != NULL; ++i) {
        if (FblcsNamesEqual(vars->name, var_expr->var.name.name)) {
          var_expr->var.id = i;
          return vars->type;
        }
        vars = vars->next;
      }
      ReportError("variable '%s' not defined.", error, var_expr->var.name.loc, var_expr->var.name.name);
      return NULL;
    }

    case FBLCS_APP_EXPR: {
      FblcsAppExpr* app_expr = (FblcsAppExpr*)expr;

      FblcsType* arg_types[app_expr->argv.size];
      for (size_t i = 0; i < app_expr->argv.size; ++i) {
        arg_types[i] = CheckExpr(prog, vars, app_expr->argv.xs[i], error);
      }

      FblcsType* return_type = NULL;
      FblcsArgV* argv = NULL;

      FblcsFunc* func = FblcsLookupFunc(prog, app_expr->func.name);
      if (func != NULL) {
        argv = &func->argv;
        return_type = FblcsLookupType(prog, func->return_type.name);
      } else {
        FblcsType* type = FblcsLookupType(prog, app_expr->func.name);
        if (type != NULL) {
          if (type->kind != FBLCS_STRUCT_KIND) {
            ReportError("Cannot do application on union type %s.\n", error, app_expr->func.loc, app_expr->func.name);
            return NULL;
          }
          argv = &type->fieldv;
          return_type = type;
        } else {
          ReportError("'%s' not defined.\n", error, app_expr->func.loc, app_expr->func.name);
          return NULL;
        }
      }

      if (argv->size == app_expr->argv.size) {
        for (size_t i = 0; i < argv->size; ++i) {
          FblcsType* expected = FblcsLookupType(prog, argv->xs[i].type.name);
          CheckTypesMatch(app_expr->argv.xs[i]->loc, expected, arg_types[i], error);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", error, app_expr->func.loc, argv->size, app_expr->func.name, app_expr->argv.size);
      }
      return return_type;
    }

    case FBLCS_ACCESS_EXPR: {
      FblcsAccessExpr* access_expr = (FblcsAccessExpr*)expr;
      FblcsType* type = CheckExpr(prog, vars, access_expr->obj, error);
      if (type == NULL) {
        return NULL;
      }

      for (size_t i = 0; i < type->fieldv.size; ++i) {
        if (FblcsNamesEqual(access_expr->field.name.name, type->fieldv.xs[i].name.name)) {
          access_expr->field.id = i;
          return FblcsLookupType(prog, type->fieldv.xs[i].type.name);
        }
      }
      ReportError("%s is not a field of type %s\n", error, access_expr->field.name.loc, access_expr->field.name.name, type->name.name);
      return NULL;
    }

    case FBLCS_UNION_EXPR: {
      FblcsUnionExpr* union_expr = (FblcsUnionExpr*)expr;
      FblcsType* arg_type = CheckExpr(prog, vars, union_expr->arg, error);
      FblcsType* type = CheckType(prog, &union_expr->type, error);
      if (type == NULL) {
        return NULL;
      }

      if (type->kind != FBLCS_UNION_KIND) {
        ReportError("%s does not refer to a union type.\n", error, union_expr->type.loc, union_expr->type.name);
        return NULL;
      }

      for (size_t i = 0; i < type->fieldv.size; ++i) {
        if (FblcsNamesEqual(union_expr->field.name.name, type->fieldv.xs[i].name.name)) {
          union_expr->field.id = i;
          FblcsType* expected = FblcsLookupType(prog, type->fieldv.xs[i].type.name);
          CheckTypesMatch(union_expr->arg->loc, expected, arg_type, error);
          return type;
        }
      }
      ReportError("%s is not a field of type %s\n", error, union_expr->field.name.loc, union_expr->field.name.name, type->name.name);
      return NULL;
    }

    case FBLCS_LET_EXPR: {
      FblcsLetExpr* let_expr = (FblcsLetExpr*)expr;
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, let_expr->name.name)) {
          ReportError("Redefinition of variable '%s'\n", error, let_expr->name.loc, let_expr->name.name);
          return NULL;
        }
      }

      FblcsType* var_type = CheckType(prog, &let_expr->type, error);
      FblcsType* def_type = CheckExpr(prog, vars, let_expr->def, error);
      CheckTypesMatch(let_expr->def->loc, var_type, def_type, error);

      Vars nvars = {
        .type = var_type,
        .name = let_expr->name.name,
        .next = vars
      };
      return CheckExpr(prog, &nvars, let_expr->body, error);
    }

    case FBLCS_COND_EXPR: {
      FblcsCondExpr* cond_expr = (FblcsCondExpr*)expr;
      FblcsType* type = CheckExpr(prog, vars, cond_expr->select, error);
      if (type != NULL) {
        if (type->kind == FBLCS_UNION_KIND) {
          if (type->fieldv.size != cond_expr->argv.size) {
            ReportError("Expected %d arguments, but %d were provided.\n", error, cond_expr->_base.loc, type->fieldv.size, cond_expr->argv.size);
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", error, cond_expr->select->loc, type->name.name);
        }
      }

      FblcsType* result_type = NULL;
      assert(cond_expr->argv.size > 0);
      for (size_t i = 0; i < cond_expr->argv.size; ++i) {
        FblcsType* arg_type = CheckExpr(prog, vars, cond_expr->argv.xs[i], error);
        CheckTypesMatch(cond_expr->argv.xs[i]->loc, result_type, arg_type, error);
        result_type = (result_type == NULL) ? arg_type : result_type;
      }
      return result_type;
    }

    default: {
      UNREACHABLE("invalid fblcs expression tag");
      return NULL;
    }
  }
}

// CheckActn --
//   Verify the given action is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   prog - The program environment.
//   vars - The variables currently in scope.
//   ports - The ports currently in scope.
//   actn - The action to perform name resolution on.
//   error - An out parameter to indicate error.
//
// Results:
//   The type of the action or NULL if the expression is not well formed and
//   well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to stderr describing the problem with the action and error is set
//   to true.
static FblcsType* CheckActn(FblcsProgram* prog, Vars* vars, Ports* ports, FblcsActn* actn, bool* error)
{
  switch (actn->tag) {
    case FBLCS_EVAL_ACTN: {
      FblcsEvalActn* eval_actn = (FblcsEvalActn*)actn;
      return CheckExpr(prog, vars, eval_actn->arg, error);
    }

    case FBLCS_GET_ACTN: {
      FblcsGetActn* get_actn = (FblcsGetActn*)actn;

      for (size_t i = 0; ports != NULL; ++i) {
        if (FblcsNamesEqual(ports->name, get_actn->port.name.name)) {
          if (ports->polarity == FBLCS_GET_POLARITY) {
            get_actn->port.id = i;
            return ports->type;
          } else {
            ReportError("Port '%s' should have get polarity, but has put polarity.\n", error, get_actn->port.name.loc, get_actn->port.name.name);
            return NULL;
          }
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", error, get_actn->port.name.loc, get_actn->port.name.name);
      return NULL;
    }

    case FBLCS_PUT_ACTN: {
      FblcsPutActn* put_actn = (FblcsPutActn*)actn;
      FblcsType* arg_type = CheckExpr(prog, vars, put_actn->arg, error);

      for (size_t i = 0; ports != NULL; ++i) {
        if (FblcsNamesEqual(ports->name, put_actn->port.name.name)) {
          if (ports->polarity == FBLCS_PUT_POLARITY) {
            put_actn->port.id = i;
            CheckTypesMatch(put_actn->arg->loc, ports->type, arg_type, error);
            return ports->type;
          } else {
            ReportError("Port '%s' should have put polarity, but has get polarity.\n", error, put_actn->port.name.loc, put_actn->port.name.name);
            return NULL;
          }
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", error, put_actn->port.name.loc, put_actn->port.name.name);
      return NULL;
    }

    case FBLCS_CALL_ACTN: {
      FblcsCallActn* call_actn = (FblcsCallActn*)actn;

      Ports* port_types[call_actn->portv.size];
      for (size_t i = 0; i < call_actn->portv.size; ++i) {
        port_types[i] = NULL;
        Ports* curr = ports;
        for (size_t id = 0; curr != NULL; ++id) {
          if (FblcsNamesEqual(curr->name, call_actn->portv.xs[i].name.name)) {
            call_actn->portv.xs[i].id = id;
            port_types[i] = curr;
            break;
          }
          curr = curr->next;
        }
        if (port_types[i] == NULL) {
          ReportError("Port '%s' not defined.\n", error, call_actn->portv.xs[i].name.loc, call_actn->portv.xs[i].name.name);
        }
      }

      FblcsType* arg_types[call_actn->argv.size];
      for (size_t i = 0; i < call_actn->argv.size; ++i) {
        arg_types[i] = CheckExpr(prog, vars, call_actn->argv.xs[i], error);
      }

      FblcsProc* proc = FblcsLookupProc(prog, call_actn->proc.name);
      if (proc == NULL) {
        ReportError("%s does not refer to a proc.\n", error, call_actn->proc.loc, call_actn->proc.name);
        return NULL;
      }

      if (proc->portv.size == call_actn->portv.size) {
        for (size_t i = 0; i < proc->portv.size; ++i) {
          if (port_types[i] != NULL) {
            if (port_types[i]->polarity != proc->portv.xs[i].polarity) {
                ReportError("Port '%s' has wrong polarity. Expected '%s', but found '%s'.\n", error,
                    call_actn->portv.xs[i].name.loc, call_actn->portv.xs[i].name.name,
                    proc->portv.xs[i].polarity == FBLCS_PUT_POLARITY ? "put" : "get",
                    port_types[i]->polarity == FBLCS_PUT_POLARITY ? "put" : "get");
            }
            FblcsType* expected = FblcsLookupType(prog, proc->portv.xs[i].type.name);
            CheckTypesMatch(call_actn->portv.xs[i].name.loc, expected, port_types[i]->type, error);
          }
        }
      } else {
        ReportError("Expected %d port arguments to %s, but %d were provided.\n", error, call_actn->proc.loc, proc->portv.size, call_actn->proc.name, call_actn->portv.size);
      }

      if (proc->argv.size == call_actn->argv.size) {
        for (size_t i = 0; i < call_actn->argv.size; ++i) {
          FblcsType* expected = FblcsLookupType(prog, proc->argv.xs[i].type.name);
          CheckTypesMatch(call_actn->argv.xs[i]->loc, expected, arg_types[i], error);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", error, call_actn->proc.loc, proc->argv.size, call_actn->proc.name, call_actn->argv.size);
      }
      return FblcsLookupType(prog, proc->return_type.name);
    }

    case FBLCS_LINK_ACTN: {
      FblcsLinkActn* link_actn = (FblcsLinkActn*)actn;
      FblcsType* type = CheckType(prog, &link_actn->type, error);

      for (Ports* curr = ports; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, link_actn->get.name)) {
          ReportError("Redefinition of port '%s'\n", error, link_actn->get.loc, link_actn->get.name);
        } else if (FblcsNamesEqual(curr->name, link_actn->put.name)) {
          ReportError("Redefinition of port '%s'\n", error, link_actn->put.loc, link_actn->put.name);
        }
      }

      if (FblcsNamesEqual(link_actn->get.name, link_actn->put.name)) {
        ReportError("Redefinition of port '%s'\n", error, link_actn->put.loc, link_actn->put.name);
      }

      Ports getport = {
        .type = type,
        .polarity = FBLCS_GET_POLARITY,
        .name = link_actn->get.name,
        .next = ports
      };
      Ports putport = {
        .type = type,
        .polarity = FBLCS_PUT_POLARITY,
        .name = link_actn->put.name,
        .next = &getport
      };

      return CheckActn(prog, vars, &putport, link_actn->body, error);
    }

    case FBLCS_EXEC_ACTN: {
      FblcsExecActn* exec_actn = (FblcsExecActn*)actn;

      Vars vars_data[exec_actn->execv.size];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execv.size; ++i) {
        FblcsExec* exec = exec_actn->execv.xs + i;
        FblcsType* var_type = CheckType(prog, &exec->type, error);
        FblcsType* def_type = CheckActn(prog, vars, ports, exec->actn, error);
        CheckTypesMatch(exec->actn->loc, var_type, def_type, error);
        vars_data[i].type = var_type;
        vars_data[i].name = exec->name.name;
        vars_data[i].next = nvars;
        nvars = vars_data + i;
      }
      return CheckActn(prog, nvars, ports, exec_actn->body, error);
    }

    case FBLCS_COND_ACTN: {
      FblcsCondActn* cond_actn = (FblcsCondActn*)actn;
      FblcsType* type = CheckExpr(prog, vars, cond_actn->select, error);
      if (type != NULL) {
        if (type->kind == FBLCS_UNION_KIND) {
          if (type->fieldv.size != cond_actn->argv.size) {
            ReportError("Expected %d arguments, but %d were provided.\n", error, cond_actn->_base.loc, type->fieldv.size, cond_actn->argv.size);
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", error, cond_actn->select->loc, type->name.name);
        }
      }

      FblcsType* result_type = NULL;
      assert(cond_actn->argv.size > 0);
      for (size_t i = 0; i < cond_actn->argv.size; ++i) {
        FblcsType* arg_type = CheckActn(prog, vars, ports, cond_actn->argv.xs[i], error);
        CheckTypesMatch(cond_actn->argv.xs[i]->loc, result_type, arg_type, error);
        result_type = (result_type == NULL) ? arg_type : result_type;
      }
      return result_type;
    }

    default: {
      UNREACHABLE("invalid fblcs actn tag");
      return NULL;
    }
  }
}

// FblcsCheckProgram -- see fblcs.h for documentation.
bool FblcsCheckProgram(FblcsProgram* prog)
{
  bool error = false;

  // Check type declarations.
  for (size_t type_id = 0; type_id < prog->typev.size; ++type_id) {
    FblcsType* type = prog->typev.xs[type_id];
    assert(type->kind == FBLCS_STRUCT_KIND || type->fieldv.size > 0);

    if (FblcsLookupType(prog, type->name.name) != type) {
      ReportError("Redefinition of %s\n", &error, type->name.loc, type->name.name);
    }

    for (FblcFieldId field_id = 0; field_id < type->fieldv.size; ++field_id) {
      FblcsArg* field = type->fieldv.xs + field_id;

      // Check whether a field has already been declared with the same name.
      for (FblcFieldId i = 0; i < field_id; ++i) {
        if (FblcsNamesEqual(field->name.name, type->fieldv.xs[i].name.name)) {
          ReportError("Redefinition of field %s\n", &error, field->name.loc, field->name.name);
          break;
        }
      }

      CheckType(prog, &field->type, &error);
    }
  }

  // Check func declarations
  for (size_t func_id = 0; func_id < prog->funcv.size; ++func_id) {
    FblcsFunc* func = prog->funcv.xs[func_id];

    if (FblcsLookupType(prog, func->name.name) != NULL ||
        FblcsLookupFunc(prog, func->name.name) != func) {
      ReportError("Redefinition of %s\n", &error, func->name.loc, func->name.name);
    }

    Vars nvars[func->argv.size];
    Vars* vars = NULL;
    for (size_t i = 0; i < func->argv.size; ++i) {
      FblcsArg* var = func->argv.xs + i;
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, var->name.name)) {
          ReportError("Redefinition of argument '%s'\n", &error, var->name.loc, var->name.name);
          break;
        }
      }

      nvars[i].name = var->name.name;
      nvars[i].type = CheckType(prog, &var->type, &error);
      nvars[i].next = vars;
      vars = nvars + i;
    }

    FblcsType* return_type = CheckType(prog, &func->return_type, &error);
    FblcsType* body_type = CheckExpr(prog, vars, func->body, &error);
    CheckTypesMatch(func->body->loc, return_type, body_type, &error);
  }

  // Check proc declarations
  for (size_t proc_id = 0; proc_id < prog->procv.size; ++proc_id) {
    FblcsProc* proc = prog->procv.xs[proc_id];

    if (FblcsLookupType(prog, proc->name.name) != NULL ||
        FblcsLookupFunc(prog, proc->name.name) != NULL ||
        FblcsLookupProc(prog, proc->name.name) != proc) {
      ReportError("Redefinition of %s\n", &error, proc->name.loc, proc->name.name);
    }

    Ports nports[proc->portv.size];
    Ports* ports = NULL;
    for (size_t i = 0; i < proc->portv.size; ++i) {
      FblcsPort* port = proc->portv.xs + i;
      for (Ports* curr = ports; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, port->name.name)) {
          ReportError("Redefinition of port '%s'\n", &error, port->name.loc, port->name.name);
        }
      }

      nports[i].name = port->name.name;
      nports[i].type = CheckType(prog, &port->type, &error);
      nports[i].polarity = port->polarity;
      nports[i].next = ports;
      ports = nports + i;
    }

    Vars nvars[proc->argv.size];
    Vars* vars = NULL;
    for (size_t i = 0; i < proc->argv.size; ++i) {
      FblcsArg* var = proc->argv.xs + i;
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, var->name.name)) {
          ReportError("Redefinition of argument '%s'\n", &error, var->name.loc, var->name.name);
        }
      }

      nvars[i].name = var->name.name;
      nvars[i].type = CheckType(prog, &var->type, &error);
      nvars[i].next = vars;
      vars = nvars + i;
    }

    FblcsType* return_type = CheckType(prog, &proc->return_type, &error);
    FblcsType* body_type = CheckActn(prog, vars, ports, proc->body, &error);
    CheckTypesMatch(proc->body->loc, return_type, body_type, &error);
  }

  return !error;
}
