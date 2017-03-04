// resolve.c --
//   This file provides routines for performing resolution of references to
//   variables, ports, declarations, and fields of an fblc program.

#include <assert.h>     // for assert

#include "fblcs.h"

#define INVALID_CASE() assert(false)

typedef struct Vars {
  FblcTypeId type;
  FblcsName name;
  struct Vars* next;
} Vars;

static Vars* AddVar(Vars* vars, FblcTypeId type, FblcsName name, Vars* next);
static FblcTypeId LookupType(FblcsProgram* sprog, FblcsNameL* type);
static FblcFieldId LookupField(FblcsProgram* sprog, FblcTypeId type_id, FblcsNameL* field);
static FblcTypeId FieldType(FblcsProgram* sprog, FblcTypeId type_id, FblcFieldId field_id);
static FblcTypeId ResolveExpr(FblcsProgram* sprog, Vars* vars, FblcLocId* loc_id, FblcExpr* expr);
static FblcTypeId ResolveActn(FblcsProgram* sprog, Vars* vars, Vars* ports, FblcLocId* loc_id, FblcActn* actn);

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
static Vars* AddVar(Vars* vars, FblcTypeId type, FblcsName name, Vars* next)
{
  vars->type = type;
  vars->name = name;
  vars->next = next;
  return vars;
}

// LookupType --
//   Look up the type id for the give type name.
//
// Inputs:
//   sprog - The program environment.
//   type - The name and location of the type to look up.
//
// Results:
//   The type id corresponding to the type referred to at that given location
//   of the program, or FBLC_NULL_ID if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcTypeId LookupType(FblcsProgram* sprog, FblcsNameL* type)
{
  FblcTypeId id = FblcsLookupDecl(sprog, type->name);
  if (id == FBLC_NULL_ID) {
    FblcsReportError("Type %s not declared.\n", type->loc, type->name);
    return FBLC_NULL_ID;
  }
  return id;
}

// LookupField --
//   Look up the field id for the field referred to from the given location.
//
// Inputs:
//   sprog - The program environment.
//   type_id - The id of the type to look up the field in.
//   field - The name and location of the field to look up.
//
// Results:
//   The field id corresponding to the field referred to at that given
//   location of the program, or FBLC_NULL_ID if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.

static FblcFieldId LookupField(FblcsProgram* sprog, FblcTypeId type_id, FblcsNameL* field)
{
  FblcFieldId id = FblcsLookupField(sprog, type_id, field->name);
  if (id == FBLC_NULL_ID) {
    FblcsReportError("'%s' is not a field of the type '%s'.\n", field->loc, field->name, DeclName(sprog, type_id));
    return FBLC_NULL_ID;
  }
  return id;
}

// FieldType --
//   Look up the type id for the given field of a type declaration.
//
// Inputs:
//   sprog - The program environment.
//   type_id - The id of the type declaration.
//   field_id - The id of the field.
//
// Results:
//   The type id for the type of the given field in the given type
//   declaration, or FBLC_NULL_ID if there is an error.
//   The behavior is undefined if type_id is not the id of a type declaration
//   in the program.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcTypeId FieldType(FblcsProgram* sprog, FblcTypeId type_id, FblcFieldId field_id)
{
  assert(type_id < sprog->program->declc);
  FblcDecl* decl = sprog->program->declv[type_id];
  assert(decl->tag == FBLC_STRUCT_DECL || decl->tag == FBLC_UNION_DECL);
  FblcLocId field_loc_id = sprog->symbols->declv[type_id] + 1 + field_id;
  FblcsTypedIdSymbol* field = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[field_loc_id];
  assert(field->tag == FBLCS_TYPED_ID_SYMBOL);
  return LookupType(sprog, &field->type);
}

// ReturnType --
//   Look up the id of the return type for a declaration.
//
// Inputs:
//   sprog - The program environment.
//   decl_id - The id of the declaration to get the return type for.
//
// Results:
//   The id of the return type for the given declaration. For struct and union
//   declarations, this returns the declaration itself. For func and proc
//   declarations, this returns the id of the return type of the func or proc.
//   Returns FBLC_NULL_ID on error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
FblcTypeId ReturnType(FblcsProgram* sprog, FblcDeclId decl_id)
{
  assert(decl_id < sprog->program->declc);
  FblcDecl* decl = sprog->program->declv[decl_id];
  switch (decl->tag) {
    case FBLC_STRUCT_DECL: {
      return decl_id;
    }

    case FBLC_UNION_DECL: {
      return decl_id;
    }

    case FBLC_FUNC_DECL: {
      FblcFuncDecl* func = (FblcFuncDecl*)decl;
      FblcLocId return_type_loc_id = sprog->symbols->declv[decl_id] + 1 + func->argc;
      FblcsIdSymbol* type = (FblcsIdSymbol*)sprog->symbols->symbolv[return_type_loc_id];
      assert(type->tag == FBLCS_ID_SYMBOL);
      return LookupType(sprog, &type->name);
    }

    case FBLC_PROC_DECL: {
      FblcProcDecl* proc = (FblcProcDecl*)decl;
      FblcLocId return_type_loc_id = sprog->symbols->declv[decl_id] + 1 + proc->portc + proc->argc;
      FblcsIdSymbol* type = (FblcsIdSymbol*)sprog->symbols->symbolv[return_type_loc_id];
      assert(type->tag == FBLCS_ID_SYMBOL);
      return LookupType(sprog, &type->name);
    }

    default: {
      INVALID_CASE();
      return FBLC_NULL_ID;
    }
  }
}

// ResolveExpr --
//   Perform id/name resolution for references to variables, ports,
//   declarations, and fields in the given expression.
//
// Inputs:
//   sprog - The program environment.
//   vars - The variables in scope at this point in the expression.
//   loc_id - The location of the expression in the program.
//   expr - The expression to perform name resolution on.
//
// Results:
//   The id of the type of the expression, or FBLC_NULL_ID if name resolution
//   failed.
//
// Side effects:
//   IDs in the expression are resolved.
//   Advances loc_id past the expression.
//   Prints error messages to stderr in case of error.
static FblcTypeId ResolveExpr(FblcsProgram* sprog, Vars* vars, FblcLocId* loc_id, FblcExpr* expr)
{
  (*loc_id)++;
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      FblcsIdSymbol* var = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(var->tag == FBLCS_ID_SYMBOL);
      for (size_t i = 0; vars != NULL; ++i) {
        if (FblcsNamesEqual(vars->name, var->name.name)) {
          var_expr->var = i;
          return vars->type;
        }
        vars = vars->next;
      }
      FblcsReportError("variable '%s' not defined.", var->name.loc, var->name.name);
      return FBLC_NULL_ID;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      FblcsIdSymbol* func = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(func->tag == FBLCS_ID_SYMBOL);
      app_expr->func = FblcsLookupDecl(sprog, func->name.name);
      if (app_expr->func == FBLC_NULL_ID) {
        FblcsReportError("'%s' not defined.\n", func->name.loc, func->name.name);
        return FBLC_NULL_ID;
      }

      for (size_t i = 0; i < app_expr->argc; ++i) {
        if (ResolveExpr(sprog, vars, loc_id, app_expr->argv[i]) == FBLC_NULL_ID) {
          return FBLC_NULL_ID;
        }
      }
      return ReturnType(sprog, app_expr->func);
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;

      FblcTypeId type_id = ResolveExpr(sprog, vars, loc_id, access_expr->obj);
      if (type_id == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      FblcsIdSymbol* field_name = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(field_name->tag == FBLCS_ID_SYMBOL);
      access_expr->field = LookupField(sprog, type_id, &field_name->name);
      if (access_expr->field == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }
      return FieldType(sprog, type_id, access_expr->field);
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      FblcsIdSymbol* type = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(type->tag == FBLCS_ID_SYMBOL);
      union_expr->type = LookupType(sprog, &type->name);
      if (union_expr->type == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }


      FblcsIdSymbol* field_name = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(field_name->tag == FBLCS_ID_SYMBOL);
      union_expr->field = LookupField(sprog, union_expr->type, &field_name->name);
      if (union_expr->field == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      if (ResolveExpr(sprog, vars, loc_id, union_expr->arg) == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }
      return union_expr->type;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      FblcsTypedIdSymbol* var = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(var->tag == FBLCS_TYPED_ID_SYMBOL);
      let_expr->type = LookupType(sprog, &var->type);
      if (let_expr->type == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      if (ResolveExpr(sprog, vars, loc_id, let_expr->def) == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, var->name.name)) {
          FblcsReportError("Redefinition of variable '%s'\n", var->name.loc, var->name.name);
          return FBLC_NULL_ID;
        }
      }

      Vars nvars;
      AddVar(&nvars, let_expr->type, var->name.name, vars);
      return ResolveExpr(sprog, &nvars, loc_id, let_expr->body);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      if (ResolveExpr(sprog, vars, loc_id, cond_expr->select) == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      FblcTypeId result_type_id = FBLC_NULL_ID;
      assert(cond_expr->argc > 0);
      for (size_t i = 0; i < cond_expr->argc; ++i) {
        result_type_id = ResolveExpr(sprog, vars, loc_id, cond_expr->argv[i]);
        if (result_type_id == FBLC_NULL_ID) {
          return FBLC_NULL_ID;
        }
      }
      return result_type_id;
    }

    default: {
      INVALID_CASE();
      return FBLC_NULL_ID;
    }
  }
}

// ResolveActn --
//   Perform id/name resolution for references to variables, ports,
//   declarations, and fields in the given action.
//
// Inputs:
//   sprog - The program environment.
//   vars - The variables in scope at this point in the expression.
//   ports - The ports in scope at this point in the expression.
//   loc_id - The location of the action in the program.
//   actn - The action to perform name resolution on.
//
// Results:
//   The id of the type of the action, or FBLC_NULL_ID if name resolution failed.
//
// Side effects:
//   IDs in the action are resolved.
//   Advances loc_id past the action.
//   Prints error messages to stderr in case of error.
static FblcTypeId ResolveActn(FblcsProgram* sprog, Vars* vars, Vars* ports, FblcLocId* loc_id, FblcActn* actn)
{
  (*loc_id)++;
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return ResolveExpr(sprog, vars, loc_id, eval_actn->arg);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      FblcsIdSymbol* port = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(port->tag == FBLCS_ID_SYMBOL);
      for (size_t i = 0; ports != NULL; ++i) {
        if (FblcsNamesEqual(ports->name, port->name.name)) {
          get_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      FblcsReportError("port '%s' not defined.\n", port->name.loc, port->name.name);
      return FBLC_NULL_ID;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      FblcsIdSymbol* port = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(port->tag == FBLCS_ID_SYMBOL);

      if (ResolveExpr(sprog, vars, loc_id, put_actn->arg) == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      for (size_t i = 0; ports != NULL; ++i) {
        if (FblcsNamesEqual(ports->name, port->name.name)) {
          put_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      FblcsReportError("port '%s' not defined.\n", port->name.loc, port->name.name);
      return FBLC_NULL_ID;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      FblcsIdSymbol* proc = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(proc->tag == FBLCS_ID_SYMBOL);
      call_actn->proc = FblcsLookupDecl(sprog, proc->name.name);
      if (call_actn->proc == FBLC_NULL_ID) {
        FblcsReportError("'%s' not defined.\n", proc->name.loc, proc->name.name);
        return FBLC_NULL_ID;
      }

      for (size_t i = 0; i < call_actn->portc; ++i) {
        FblcsIdSymbol* port = (FblcsIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
        assert(port->tag == FBLCS_ID_SYMBOL);
        call_actn->portv[i] = FBLC_NULL_ID;
        Vars* curr = ports;
        for (size_t port_id = 0; curr != NULL; ++port_id) {
          if (FblcsNamesEqual(curr->name, port->name.name)) {
            call_actn->portv[i] = port_id;
            break;
          }
          curr = curr->next;
        }
        if (call_actn->portv[i] == FBLC_NULL_ID) {
          FblcsReportError("port '%s' not defined.\n", port->name.loc, port->name.name);
          return FBLC_NULL_ID;
        }
      }

      for (size_t i = 0 ; i < call_actn->argc; ++i) {
        if (ResolveExpr(sprog, vars, loc_id, call_actn->argv[i]) == FBLC_NULL_ID) {
          return FBLC_NULL_ID;
        }
      }

      return ReturnType(sprog, call_actn->proc);
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      FblcsLinkSymbol* symbol = (FblcsLinkSymbol*)sprog->symbols->symbolv[(*loc_id)++];
      assert(symbol->tag == FBLCS_LINK_SYMBOL);
      link_actn->type = LookupType(sprog, &symbol->type);
      if (link_actn->type == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      for (Vars* curr = ports; curr != NULL; curr = curr->next) {
        if (FblcsNamesEqual(curr->name, symbol->get.name)) {
          FblcsReportError("Redefinition of port '%s'\n", symbol->get.loc, symbol->get.name);
          return FBLC_NULL_ID;
        }
        if (FblcsNamesEqual(curr->name, symbol->put.name)) {
          FblcsReportError("Redefinition of port '%s'\n", symbol->put.loc, symbol->put.name);
          return FBLC_NULL_ID;
        }
      }

      Vars getport;
      Vars putport;
      AddVar(&getport, link_actn->type, symbol->get.name, ports);
      AddVar(&putport, link_actn->type, symbol->put.name, &getport);
      return ResolveActn(sprog, vars, &putport, loc_id, link_actn->body);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      Vars vars_data[exec_actn->execc];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execc; ++i) {
        FblcExec* exec = exec_actn->execv + i;
        FblcsTypedIdSymbol* var = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[(*loc_id)++];
        assert(var->tag == FBLCS_TYPED_ID_SYMBOL);
        exec->type = LookupType(sprog, &var->type);
        if (ResolveActn(sprog, vars, ports, loc_id, exec->actn) == FBLC_NULL_ID) {
          return FBLC_NULL_ID;
        }
        nvars = AddVar(vars_data+i, exec->type, var->name.name, nvars);
      }
      return ResolveActn(sprog, nvars, ports, loc_id, exec_actn->body);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      if (ResolveExpr(sprog, vars, loc_id, cond_actn->select) == FBLC_NULL_ID) {
        return FBLC_NULL_ID;
      }

      FblcTypeId result_type_id = FBLC_NULL_ID;
      assert(cond_actn->argc > 0);
      for (size_t i = 0; i < cond_actn->argc; ++i) {
        result_type_id = ResolveActn(sprog, vars, ports, loc_id, cond_actn->argv[i]);
        if (result_type_id == FBLC_NULL_ID) {
          return FBLC_NULL_ID;
        }
      }
      return result_type_id;
    }

    default: {
      INVALID_CASE();
      return FBLC_NULL_ID;
    }
  }
}

// FblcsResolveProgram -- see fblcs.h for documentation.
bool FblcsResolveProgram(FblcsProgram* sprog)
{
  FblcLocId loc_id = 0;
  for (FblcDeclId decl_id = 0; decl_id < sprog->program->declc; ++decl_id) {
    FblcsDeclSymbol* other_decl = (FblcsDeclSymbol*)sprog->symbols->symbolv[loc_id++];
    assert(other_decl->tag == FBLCS_DECL_SYMBOL);
    if (FblcsLookupDecl(sprog, other_decl->name.name) != decl_id) {
      FblcsReportError("Redefinition of %s\n", other_decl->name.loc, other_decl->name.name);
      return false;
    }

    FblcDecl* decl = sprog->program->declv[decl_id];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL:
      case FBLC_UNION_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        for (FblcFieldId field_id = 0; field_id < type->fieldc; ++field_id) {
          FblcsTypedIdSymbol* field = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[loc_id++];
          assert(field->tag == FBLCS_TYPED_ID_SYMBOL);
          for (FblcFieldId i = 0; i < field_id; ++i) {
            FblcsTypedIdSymbol* prior_field = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[loc_id - i - 2];
            assert(prior_field->tag == FBLCS_TYPED_ID_SYMBOL);
            if (FblcsNamesEqual(field->name.name, prior_field->name.name)) {
              FblcsReportError("Redefinition of field %s\n", field->name.loc, field->name.name);
              return false;
            }
          }

          type->fieldv[field_id] = FieldType(sprog, decl_id, field_id);
          if (type->fieldv[field_id] == FBLC_NULL_ID) {
            return false;
          }
        }
        break;
      }

      case FBLC_FUNC_DECL: {
        FblcFuncDecl* func = (FblcFuncDecl*)decl;
        Vars nvars[func->argc];
        Vars* vars = NULL;
        for (size_t i = 0; i < func->argc; ++i) {
          FblcsTypedIdSymbol* var = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[loc_id++];
          assert(var->tag == FBLCS_TYPED_ID_SYMBOL);
          func->argv[i] = LookupType(sprog, &var->type);
          if (func->argv[i] == FBLC_NULL_ID) {
            return false;
          }
          for (Vars* curr = vars; curr != NULL; curr = curr->next) {
            if (FblcsNamesEqual(curr->name, var->name.name)) {
              FblcsReportError("Redefinition of argument '%s'\n", var->name.loc, var->name.name);
              return FBLC_NULL_ID;
            }
          }
          vars = AddVar(nvars+i, func->argv[i], var->name.name, vars);
        }

        FblcsIdSymbol* return_type = (FblcsIdSymbol*)sprog->symbols->symbolv[loc_id++];
        assert(return_type->tag == FBLCS_ID_SYMBOL);
        func->return_type = LookupType(sprog, &return_type->name);
        if (func->return_type == FBLC_NULL_ID) {
          return false;
        }

        if (ResolveExpr(sprog, vars, &loc_id, func->body) == FBLC_NULL_ID) {
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        FblcProcDecl* proc = (FblcProcDecl*)decl;
        Vars nports[proc->portc];
        Vars* ports = NULL;
        for (size_t i = 0; i < proc->portc; ++i) {
          FblcsTypedIdSymbol* port = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[loc_id++];
          assert(port->tag == FBLCS_TYPED_ID_SYMBOL);
          proc->portv[i].type = LookupType(sprog, &port->type);
          if (proc->portv[i].type == FBLC_NULL_ID) {
            return false;
          }
          for (Vars* curr = ports; curr != NULL; curr = curr->next) {
            if (FblcsNamesEqual(curr->name, port->name.name)) {
              FblcsReportError("Redefinition of port '%s'\n", port->name.loc, port->name.name);
              return FBLC_NULL_ID;
            }
          }
          ports = AddVar(nports+i, proc->portv[i].type, port->name.name, ports);
        }

        Vars nvars[proc->argc];
        Vars* vars = NULL;
        for (size_t i = 0; i < proc->argc; ++i) {
          FblcsTypedIdSymbol* var = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[loc_id++];
          assert(var->tag == FBLCS_TYPED_ID_SYMBOL);
          proc->argv[i] = LookupType(sprog, &var->type);
          if (proc->argv[i] == FBLC_NULL_ID) {
            return false;
          }
          for (Vars* curr = vars; curr != NULL; curr = curr->next) {
            if (FblcsNamesEqual(curr->name, var->name.name)) {
              FblcsReportError("Redefinition of argument '%s'\n", var->name.loc, var->name.name);
              return FBLC_NULL_ID;
            }
          }
          vars = AddVar(nvars+i, proc->argv[i], var->name.name, vars);
        }

        FblcsIdSymbol* return_type = (FblcsIdSymbol*)sprog->symbols->symbolv[loc_id++];
        assert(return_type->tag == FBLCS_ID_SYMBOL);
        proc->return_type = LookupType(sprog, &return_type->name);
        if (proc->return_type == FBLC_NULL_ID) {
          return false;
        }

        if (ResolveActn(sprog, vars, ports, &loc_id, proc->body) == FBLC_NULL_ID) {
          return false;
        }
        break;
      }

      default: {
        INVALID_CASE();
        return false;
      }
    }
  }
  return true;
}
