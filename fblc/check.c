// check.c --
//   This file implements routines for checking a program is well formed and
//   well typed.

#include <assert.h>     // for assert

#include "fblcs.h"

// Vars --
//   A mapping from variable id to type.
//
//   We typically allocate the Vars mapping on the stack, so it is not safe to
//   return or otherwise capture a Vars* outside of the frame where it is
//   allocated.
typedef struct Vars {
  FblcTypeId type;
  struct Vars* next;
} Vars;

// Ports --
//   A mapping from port id to port type and polarity.
//
//   We typically allocate the Ports mapping on the stack, so it is not safe
//   to return or otherwise capture a Ports* outside of the frame where it is
//   allocated.
typedef struct Ports {
  FblcTypeId type;
  FblcPolarity polarity;
  struct Ports* next;
} Ports;

static FblcsLoc* LocIdLoc(FblcsSymbols* symbols, FblcLocId loc_id);
static FblcsNameL* LocIdName(FblcsSymbols* symbols, FblcLocId loc_id);
static Vars* AddVar(Vars* vars, FblcTypeId type, Vars* next);
static Ports* AddPort(Ports* vars, FblcTypeId type, FblcPolarity polarity, Ports* next);
static FblcTypeId CheckExpr(FblcsProgram* sprog, Vars* vars, FblcExpr* expr, FblcLocId* loc_id, bool* error);
static FblcTypeId CheckActn(FblcsProgram* sprog, Vars* vars, Ports* ports, FblcActn* actn, FblcLocId* loc_id, bool* error);

// LocIdLoc -- 
//   Return the location corresponding to a given loc_id.
//
// TODO: Remove this function. We should know the symbol type from context and
// be able to look up the location from the symbol directly.
static FblcsLoc* LocIdLoc(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == FBLCS_LOC_SYMBOL) {
    FblcsLocSymbol* loc_symbol = (FblcsLocSymbol*)symbol;
    return &loc_symbol->loc;
  }
  return LocIdName(symbols, loc_id)->loc;
}

// LocIdLoc -- 
//   Return the name corresponding to a given loc_id.
//
// TODO: Remove this function. We should know the symbol type from context and
// be able to look up the name from the symbol directly.
static FblcsNameL* LocIdName(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  switch (symbol->tag) {
    case FBLCS_LOC_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case FBLCS_ID_SYMBOL: {
      FblcsIdSymbol* id_symbol = (FblcsIdSymbol*)symbol;
      return &id_symbol->name;
    }

    case FBLCS_TYPED_ID_SYMBOL: {
      FblcsTypedIdSymbol* typed_id_symbol = (FblcsTypedIdSymbol*)symbol;
      return &typed_id_symbol->name;
    }

    case FBLCS_LINK_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case FBLCS_DECL_SYMBOL: {
      FblcsDeclSymbol* decl_symbol = (FblcsDeclSymbol*)symbol;
      return &decl_symbol->name;
    }
  }
  assert(false && "Invalid tag");
  return NULL;
}

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
static Vars* AddVar(Vars* vars, FblcTypeId type, Vars* next)
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

static Ports* AddPort(Ports* ports, FblcTypeId type, FblcPolarity polarity, Ports* next)
{
  ports->type = type;
  ports->polarity = polarity;
  ports->next = next;
  return ports;
}

// IsType --
//   Return whether a declaration id refers to a valid type declaration.
//
// Inputs:
//   sprog - The program environment.
//   decl_id - The declaration id to check.
//
// Results:
//   true if the declaration id refers to a valid type declaration, false
//   otherwise.
//
// Side effects:
//   None.
bool IsType(FblcsProgram* sprog, FblcDeclId decl_id)
{
  if (decl_id < sprog->program->declc) {
    FblcDecl* decl = sprog->program->declv[decl_id];
    if (decl->tag == FBLC_UNION_DECL || decl->tag == FBLC_STRUCT_DECL) {
      return true;
    }
  }
  return false;
}

// CheckIsType --
//   Check that the given declaration id refers to a type declaration.
//
// Inputs:
//   sprog - The program environment.
//   loc_id - The location of the reference to this declaration.
//   decl_id - The declaration id to check.
//   error - Out parameter set to false on error.
//
// Results:
//   None.
//
// Side effects:
//   If the declaration id does not refer to a valid type declaration, set
//   error to true and print an error message to stderr.
void CheckIsType(FblcsProgram* sprog, FblcLocId loc_id, FblcDeclId decl_id, bool* error)
{
  if (!IsType(sprog, decl_id)) {
    FblcsNameL* type = LocIdName(sprog->symbols, loc_id);
    FblcsReportError("%s does not refer to a type.\n", type->loc, type->name);
    *error = true;
  }
}

// CheckTypesMatch --
//   Check that the given types match.
//
// Inputs:
//   sprog - The program environment.
//   loc_id - The location of the reference to this declaration.
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
void CheckTypesMatch(FblcsProgram* sprog, FblcLocId loc_id, FblcTypeId expected, FblcTypeId actual, bool* error)
{
  if (!IsType(sprog, expected) || !IsType(sprog, actual)) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  if (expected != actual) {
    FblcsReportError("Expected type %s, but found type %s.\n",
        LocIdLoc(sprog->symbols, loc_id), FblcsDeclName(sprog, expected), FblcsDeclName(sprog, actual));
    *error = true;
  }
}

// CheckExpr --
//   Verify the given expression is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   sprog - The program environment.
//   vars - The types of the variables in scope.
//   expr - The expression to check.
//   loc_id - The location of the expression.
//   error - An out parameter to indicate error.
//
// Result:
//   The type of the expression, or FBLC_NULL_ID if the expression is not well
//   formed and well typed.
//
// Side effects:
//   If the expression is not well formed, an error message is printed to
//   stderr describing the problem with the expression and error is set to
//   true.
//   Advances loc_id just past the end of the expression, regardless of
//   whether the expression is well formed or not.
static FblcTypeId CheckExpr(FblcsProgram* sprog, Vars* vars, FblcExpr* expr, FblcLocId* loc_id, bool* error)
{
  FblcLocId expr_loc_id = (*loc_id)++;
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      FblcLocId var_loc_id = (*loc_id)++;
      for (size_t i = 0; vars != NULL && i < var_expr->var; ++i) {
        vars = vars->next;
      }
      if (vars == NULL) {
        FblcsIdSymbol* var = (FblcsIdSymbol*)sprog->symbols->symbolv[var_loc_id];
        assert(var->tag == FBLCS_ID_SYMBOL);
        FblcsReportError("Variable '%s' not defined.\n", var->name.loc, var->name.name);
        *error = true;
        return FBLC_NULL_ID;
      }
      return vars->type;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      FblcLocId func_loc_id = (*loc_id)++;
      size_t argc = 0;
      FblcTypeId* argv = NULL;
      FblcTypeId return_type = FBLC_NULL_ID;
      if (app_expr->func < sprog->program->declc) {
        FblcDecl* decl = sprog->program->declv[app_expr->func];
        switch (decl->tag) {
          case FBLC_STRUCT_DECL: {
            FblcTypeDecl* type = (FblcTypeDecl*)decl;
            argc = type->fieldc;
            argv = type->fieldv;
            return_type = app_expr->func;
            break;
          }

          case FBLC_UNION_DECL: {
            FblcsIdSymbol* func = (FblcsIdSymbol*)sprog->symbols->symbolv[func_loc_id];
            assert(func->tag == FBLCS_ID_SYMBOL);
            FblcsReportError("Cannot do application on union type %s.\n", func->name.loc, func->name.name);
            *error = true;
            break;
          }

          case FBLC_FUNC_DECL: {
            FblcFuncDecl* func = (FblcFuncDecl*)decl;
            argc = func->argc;
            argv = func->argv;
            return_type = func->return_type;
            break;
          }

          case FBLC_PROC_DECL: {
            FblcsIdSymbol* func = (FblcsIdSymbol*)sprog->symbols->symbolv[func_loc_id];
            assert(func->tag == FBLCS_ID_SYMBOL);
            FblcsReportError("Cannot do application on a process %s.\n", func->name.loc, func->name.name);
            *error = true;
            break;
          }

          default:
            assert(false && "Invalid decl tag");
            break;
        }
      } else {
        FblcsIdSymbol* func = (FblcsIdSymbol*)sprog->symbols->symbolv[func_loc_id];
        assert(func->tag == FBLCS_ID_SYMBOL);
        FblcsReportError("%s does not refer to a function.\n", func->name.loc, func->name.name);
        *error = true;
      }

      if (argv != NULL && argc != app_expr->argc) {
        FblcsIdSymbol* func = (FblcsIdSymbol*)sprog->symbols->symbolv[func_loc_id];
        assert(func->tag == FBLCS_ID_SYMBOL);
        FblcsReportError("Expected %d arguments to %s, but %d were provided.\n", func->name.loc, argc, func->name.name, app_expr->argc);
        argv = NULL;
        *error = true;
      }

      for (size_t i = 0; i < app_expr->argc; ++i) {
        FblcLocId expr_loc_id = *loc_id;
        FblcTypeId arg_type = CheckExpr(sprog, vars, app_expr->argv[i], loc_id, error);
        if (argv != NULL) {
          CheckTypesMatch(sprog, expr_loc_id, argv[i], arg_type, error);
        }
      }
      return return_type;
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcTypeId type_id = CheckExpr(sprog, vars, access_expr->obj, loc_id, error);
      FblcLocId field_loc_id = (*loc_id)++;
      if (IsType(sprog, type_id)) {
        FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];
        if (access_expr->field < type->fieldc) {
          return type->fieldv[access_expr->field];
        } else {
          FblcsIdSymbol* field = (FblcsIdSymbol*)sprog->symbols->symbolv[field_loc_id];
          assert(field->tag == FBLCS_ID_SYMBOL);
          FblcsReportError("%s is not a field of type %s\n", field->name.loc, field->name.name, FblcsDeclName(sprog, type_id));
          *error = true;
        }
      }
      return FBLC_NULL_ID;
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      FblcLocId type_loc_id = (*loc_id)++;
      FblcLocId field_loc_id = (*loc_id)++;

      FblcTypeId field_type = FBLC_NULL_ID;
      if (union_expr->type < sprog->program->declc) {
        FblcTypeDecl* type_decl = (FblcTypeDecl*)sprog->program->declv[union_expr->type];
        if (type_decl->tag == FBLC_UNION_DECL) {
          if (union_expr->field < type_decl->fieldc) {
            field_type = type_decl->fieldv[union_expr->field];
          } else {
            FblcsIdSymbol* field = (FblcsIdSymbol*)sprog->symbols->symbolv[field_loc_id];
            assert(field->tag == FBLCS_ID_SYMBOL);
            FblcsIdSymbol* type = (FblcsIdSymbol*)sprog->symbols->symbolv[type_loc_id];
            assert(type->tag == FBLCS_ID_SYMBOL);
            FblcsReportError("%s is not a valid field of the type %s", field->name.loc, field->name.name, type->name.name);
            *error = true;
          }
        } else {
          FblcsIdSymbol* type = (FblcsIdSymbol*)sprog->symbols->symbolv[type_loc_id];
          assert(type->tag == FBLCS_ID_SYMBOL);
          FblcsReportError("%s does not refer to a union type.\n", type->name.loc, type->name.name);
          *error = true;
        }
      } else {
        FblcsIdSymbol* type = (FblcsIdSymbol*)sprog->symbols->symbolv[type_loc_id];
        assert(type->tag == FBLCS_ID_SYMBOL);
        FblcsReportError("%s does not refer to a type.\n", type->name.loc, type->name.name);
        *error = true;
      }

      FblcLocId expr_loc_id = *loc_id;
      FblcTypeId expr_type = CheckExpr(sprog, vars, union_expr->arg, loc_id, error);
      CheckTypesMatch(sprog, expr_loc_id, field_type, expr_type, error);
      return union_expr->type;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      CheckIsType(sprog, (*loc_id)++, let_expr->type, error);
      FblcLocId def_loc_id = *loc_id;
      FblcTypeId def_type = CheckExpr(sprog, vars, let_expr->def, loc_id, error);
      CheckTypesMatch(sprog, def_loc_id, let_expr->type, def_type, error);

      Vars nvars;
      AddVar(&nvars, let_expr->type, vars);
      return CheckExpr(sprog, &nvars, let_expr->body, loc_id, error);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      FblcLocId select_loc_id = *loc_id;
      FblcTypeId select_type = CheckExpr(sprog, vars, cond_expr->select, loc_id, error);
      if (select_type < sprog->program->declc) {
        FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[select_type];
        if (type->tag == FBLC_UNION_DECL) {
          if (type->fieldc != cond_expr->argc) {
            FblcsLocSymbol* expr_loc = (FblcsLocSymbol*)sprog->symbols->symbolv[expr_loc_id];
            assert(expr_loc->tag == FBLCS_LOC_SYMBOL);
            FblcsReportError("Expected %d arguments, but %d were provided.\n", &expr_loc->loc, type->fieldc, cond_expr->argc);
            *error = true;
          }
        } else {
          FblcsLocSymbol* select_loc = (FblcsLocSymbol*)sprog->symbols->symbolv[select_loc_id];
          assert(select_loc->tag == FBLCS_LOC_SYMBOL);
          FblcsReportError("The condition has type %s, which is not a union type.\n", &select_loc->loc, FblcsDeclName(sprog, select_type));
          *error = true;
        }
      }

      assert(cond_expr->argc > 0);
      FblcTypeId result_type = FBLC_NULL_ID;
      for (size_t i = 0; i < cond_expr->argc; ++i) {
        FblcLocId arg_loc_id = *loc_id;
        FblcTypeId arg_type = CheckExpr(sprog, vars, cond_expr->argv[i], loc_id, error);
        if (i == 0) {
          result_type = arg_type;
        } else {
          CheckTypesMatch(sprog, arg_loc_id, result_type, arg_type, error);
        }
      }
      return result_type;
    }

    default: {
      assert(false && "Unknown expression type.");
      return FBLC_NULL_ID;
    }
  }
}

// CheckActn --
//   Verify the given action is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   sprog - The program environment.
//   vars - The names and types of the variables in scope.
//   ports - The names, types, and polarities of the ports in scope.
//   actn - The action to verify.
//   loc_id - The location of the action.
//   error - An out parameter to indicate error.
//
// Result:
//   The type of the action or FBLC_NULL_ID if the expression is not well formed and
//   well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to stderr describing the problem with the action and error is set
//   to true.
//   Advances loc_id just past the end of the action, regardless of whether
//   the action is well formed or not.
static FblcTypeId CheckActn(FblcsProgram* sprog, Vars* vars, Ports* ports, FblcActn* actn, FblcLocId* loc_id, bool* error)
{
  FblcLocId actn_loc_id = (*loc_id)++;
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return CheckExpr(sprog, vars, eval_actn->arg, loc_id, error);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      FblcsNameL* port = LocIdName(sprog->symbols, (*loc_id)++);
      for (size_t i = 0; ports != NULL && i < get_actn->port; ++i) {
        ports = ports->next;
      }

      FblcTypeId port_type = FBLC_NULL_ID;
      if (ports == NULL) {
        FblcsReportError("Port '%s' not defined.\n", port->loc, port->name);
        *error = true;
      } else if (ports->polarity != FBLC_GET_POLARITY) {
        FblcsReportError("Port '%s' should have get polarity, but has put polarity.\n", port->loc, port->name);
        *error = true;
      } else {
        port_type = ports->type;
      }
      return port_type;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      FblcsNameL* port = LocIdName(sprog->symbols, (*loc_id)++);
      for (size_t i = 0; ports != NULL && i < put_actn->port; ++i) {
        ports = ports->next;
      }

      FblcTypeId port_type = FBLC_NULL_ID;
      if (ports == NULL) {
        FblcsReportError("Port '%s' not defined.\n", port->loc, port->name);
        *error = true;
      } else if (ports->polarity != FBLC_PUT_POLARITY) {
        FblcsReportError("Port '%s' should have put polarity, but has get polarity.\n", port->loc, port->name);
        *error = true;
      } else {
        port_type = ports->type;
      }

      FblcLocId arg_loc = *loc_id;
      FblcTypeId arg_type = CheckExpr(sprog, vars, put_actn->arg, loc_id, error);
      CheckTypesMatch(sprog, arg_loc, port_type, arg_type, error);
      return port_type;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      FblcsNameL* proc = LocIdName(sprog->symbols, (*loc_id)++);
      FblcProcDecl* proc_decl = NULL;
      if (call_actn->proc < sprog->program->declc) {
        FblcDecl* decl = sprog->program->declv[call_actn->proc];
        if (decl->tag == FBLC_PROC_DECL) {
          proc_decl = (FblcProcDecl*)decl;
        } else {
          FblcsReportError("%s does not refer to a process.\n", proc->loc, proc->name);
          *error = true;
        }
      } else {
        FblcsReportError("%s not defined.\n", proc->loc, proc->name);
        *error = true;
      }

      bool check_ports = proc_decl != NULL;
      if (proc_decl != NULL && proc_decl->portc != call_actn->portc) {
        FblcsReportError("Expected %d port arguments to %s, but %d were provided.\n", proc->loc, proc_decl->portc, proc->name, call_actn->portc);
        *error = true;
        check_ports = false;
      }

      for (size_t i = 0; i < call_actn->portc; ++i) {
        FblcLocId port_loc_id = (*loc_id)++;
        Ports* curr = ports;
        for (size_t port_id = 0; curr != NULL && port_id < call_actn->portv[i]; ++port_id) {
          curr = curr->next;
        }
        if (curr == NULL) {
          FblcsNameL* port = LocIdName(sprog->symbols, port_loc_id);
          FblcsReportError("Port '%s' not defined.\n", port->loc, port->name);
          *error = true;
        } else {
          if (check_ports) {
            if (curr->polarity != proc_decl->portv[i].polarity) {
              FblcsNameL* port = LocIdName(sprog->symbols, port_loc_id);
              FblcsReportError("Port '%s' has wrong polarity. Expected '%s', but found '%s'.\n",
                  port->loc, port->name,
                  proc_decl->portv[i].polarity == FBLC_PUT_POLARITY ? "put" : "get",
                  curr->polarity == FBLC_PUT_POLARITY ? "put" : "get");
              *error = true;
            }
            CheckTypesMatch(sprog, port_loc_id, proc_decl->portv[i].type, curr->type, error);
          }
        }
      }

      bool check_args = proc_decl != NULL;
      if (proc_decl != NULL && proc_decl->argc != call_actn->argc) {
        FblcsReportError("Expected %d arguments to %s, but %d were provided.\n", proc->loc, proc_decl->argc, proc->name, call_actn->argc);
        *error = true;
        check_args = false;
      }

      for (size_t i = 0; i < call_actn->argc; ++i) {
        FblcLocId expr_loc_id = *loc_id;
        FblcTypeId arg_type = CheckExpr(sprog, vars, call_actn->argv[i], loc_id, error);
        if (check_args) {
          assert(proc_decl != NULL);
          CheckTypesMatch(sprog, expr_loc_id, proc_decl->argv[i], arg_type, error);
        }
      }

      return proc_decl == NULL ? FBLC_NULL_ID : proc_decl->return_type;
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      CheckIsType(sprog, (*loc_id)++, link_actn->type, error);

      Ports getport;
      Ports putport;
      AddPort(&getport, link_actn->type, FBLC_GET_POLARITY, ports);
      AddPort(&putport, link_actn->type, FBLC_PUT_POLARITY, &getport);
      return CheckActn(sprog, vars, &putport, link_actn->body, loc_id, error);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      Vars vars_data[exec_actn->execc];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execc; ++i) {
        FblcExec* exec = exec_actn->execv + i;
        CheckIsType(sprog, (*loc_id)++, exec->type, error);
        FblcLocId def_loc_id = *loc_id;
        FblcTypeId def_type = CheckActn(sprog, vars, ports, exec->actn, loc_id, error);
        CheckTypesMatch(sprog, def_loc_id, exec->type, def_type, error);
        nvars = AddVar(vars_data+i, exec->type, nvars);
      }
      return CheckActn(sprog, nvars, ports, exec_actn->body, loc_id, error);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      FblcsLoc* select_loc = LocIdLoc(sprog->symbols, *loc_id);
      FblcTypeId select_type = CheckExpr(sprog, vars, cond_actn->select, loc_id, error);
      if (select_type < sprog->program->declc) {
        FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[select_type];
        if (type->tag == FBLC_UNION_DECL) {
          if (type->fieldc != cond_actn->argc) {
            FblcsReportError("Expected %d arguments, but %d were provided.\n", LocIdLoc(sprog->symbols, actn_loc_id), type->fieldc, cond_actn->argc);
            *error = true;
          }
        } else {
          FblcsReportError("The condition has type %s, which is not a union type.\n", select_loc, FblcsDeclName(sprog, select_type));
          *error = true;
        }
      }

      assert(cond_actn->argc > 0);
      FblcTypeId result_type = FBLC_NULL_ID;
      for (size_t i = 0; i < cond_actn->argc; ++i) {
        FblcLocId arg_loc_id = *loc_id;
        FblcTypeId arg_type = CheckActn(sprog, vars, ports, cond_actn->argv[i], loc_id, error);
        if (i == 0) {
          result_type = arg_type;
        } else {
          CheckTypesMatch(sprog, arg_loc_id, result_type, arg_type, error);
        }
      }
      return result_type;
    }
  }
  assert(false && "UNREACHABLE");
  return FBLC_NULL_ID;
}

// FblcsCheckProgram -- see documentation in fblcs.h
bool FblcsCheckProgram(FblcsProgram* sprog)
{
  bool error = false;
  FblcLocId loc_id = 0;
  for (FblcDeclId decl_id = 0; decl_id < sprog->program->declc; ++decl_id) {
    FblcDecl* decl = sprog->program->declv[decl_id];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        loc_id++;
        for (FblcFieldId field_id = 0; field_id < type->fieldc; ++field_id) {
          CheckIsType(sprog, loc_id++, type->fieldv[field_id], &error);
        }
        break;
      }

      case FBLC_UNION_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        loc_id++;
        assert(type->fieldc > 0);
        for (FblcFieldId field_id = 0; field_id < type->fieldc; ++field_id) {
          CheckIsType(sprog, loc_id++, type->fieldv[field_id], &error);
        }
        break;
      }

      case FBLC_FUNC_DECL: {
        FblcFuncDecl* func = (FblcFuncDecl*)decl;
        loc_id++;
        Vars nvars[func->argc];
        Vars* vars = NULL;
        for (size_t i = 0; i < func->argc; i++) {
          CheckIsType(sprog, loc_id++, func->argv[i], &error);
          vars = AddVar(nvars+i, func->argv[i], vars);
        }
        CheckIsType(sprog, loc_id++, func->return_type, &error);
        FblcLocId expr_loc_id = loc_id;
        FblcTypeId expr_type = CheckExpr(sprog, vars, func->body, &loc_id, &error);
        CheckTypesMatch(sprog, expr_loc_id, func->return_type, expr_type, &error);
        break;
      }

      case FBLC_PROC_DECL: {
        FblcProcDecl* proc = (FblcProcDecl*)decl;
        loc_id++;
        Ports nports[proc->portc];
        Ports* ports = NULL;
        for (size_t i = 0; i < proc->portc; ++i) {
          FblcPort* port = proc->portv + i;
          CheckIsType(sprog, loc_id++, port->type, &error);
          ports = AddPort(nports + i, port->type, port->polarity, ports);
        }
        Vars nvars[proc->argc];
        Vars* vars = NULL;
        for (size_t i = 0; i < proc->argc; i++) {
          CheckIsType(sprog, loc_id++, proc->argv[i], &error);
          vars = AddVar(nvars+i, proc->argv[i], vars);
        }
        CheckIsType(sprog, loc_id++, proc->return_type, &error);
        FblcLocId body_loc_id = loc_id;
        FblcTypeId body_type = CheckActn(sprog, vars, ports, proc->body, &loc_id, &error);
        CheckTypesMatch(sprog, body_loc_id, proc->return_type, body_type, &error);
        break;
      }

      default:
        assert(false && "Invalid decl type");
        error = true;
        break;
    }
  }
  return !error;
}
