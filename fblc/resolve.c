// resolve.c --
//   This file provides routines for performing resolution of references to
//   variables, ports, declarations, and fields of an fblc program.

#include <assert.h>     // for assert

#include "fblcs.h"

#define INVALID_CASE() assert(false)

typedef struct Vars {
  FblcTypeId type;
  Name name;
  struct Vars* next;
} Vars;

static Vars* AddVar(Vars* vars, FblcTypeId type, Name name, Vars* next);
static FblcTypeId LookupType(SProgram* sprog, SName* type);
static FblcFieldId LookupField(SProgram* sprog, FblcTypeId type_id, FblcLocId loc_id);
static FblcTypeId FieldType(SProgram* sprog, FblcTypeId type_id, FblcFieldId field_id);
static FblcTypeId ResolveExpr(SProgram* sprog, Vars* vars, FblcLocId* loc_id, FblcExpr* expr);
static FblcTypeId ResolveActn(SProgram* sprog, Vars* vars, Vars* ports, FblcLocId* loc_id, FblcActn* actn);

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
static Vars* AddVar(Vars* vars, FblcTypeId type, Name name, Vars* next)
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
//   of the program, or NULL_ID if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcTypeId LookupType(SProgram* sprog, SName* type)
{
  FblcTypeId id = SLookupDecl(sprog, type->name);
  if (id == NULL_ID) {
    ReportError("Type %s not declared.\n", type->loc, type->name);
    return NULL_ID;
  }
  return id;
}

// LookupField --
//   Look up the field id for the field referred to from the given location.
//
// Inputs:
//   sprog - The program environment.
//   type_id - The id of the type to look up the field in.
//   loc_id - The location of an id node in the program that names a field.
//
// Results:
//   The field id corresponding to the field referred to at that given
//   location of the program, or NULL_ID if there is an error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.

static FblcFieldId LookupField(SProgram* sprog, FblcTypeId type_id, FblcLocId loc_id)
{
  SName* field = LocIdName(sprog->symbols, loc_id);
  FblcFieldId id = SLookupField(sprog, type_id, field->name);
  if (id == NULL_ID) {
    ReportError("'%s' is not a field of the type '%s'.\n", field->loc, field->name, DeclName(sprog, type_id));
    return NULL_ID;
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
//   declaration, or NULL_ID if there is an error.
//   The behavior is undefined if type_id is not the id of a type declaration
//   in the program.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
static FblcTypeId FieldType(SProgram* sprog, FblcTypeId type_id, FblcFieldId field_id)
{
  assert(type_id < sprog->program->declc);
  FblcDecl* decl = sprog->program->declv[type_id];
  assert(decl->tag == FBLC_STRUCT_DECL || decl->tag == FBLC_UNION_DECL);
  SName* type = LocIdType(sprog->symbols, DeclLocId(sprog, type_id) + 1 + field_id);
  return LookupType(sprog, type);
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
//   Returns NULL_ID on error.
//
// Side effects:
//   Prints an error message to stderr in the case of an error.
FblcTypeId ReturnType(SProgram* sprog, FblcDeclId decl_id)
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
      SName* type = LocIdName(sprog->symbols, DeclLocId(sprog, decl_id) + 1 + func->argc);
      return LookupType(sprog, type);
    }

    case FBLC_PROC_DECL: {
      FblcProcDecl* proc = (FblcProcDecl*)decl;
      SName* type = LocIdName(sprog->symbols, DeclLocId(sprog, decl_id) + 1 + proc->portc + proc->argc);
      return LookupType(sprog, type);
    }

    default: {
      INVALID_CASE();
      return NULL_ID;
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
//   The id of the type of the expression, or NULL_ID if name resolution
//   failed.
//
// Side effects:
//   IDs in the expression are resolved.
//   Advances loc_id past the expression.
//   Prints error messages to stderr in case of error.
static FblcTypeId ResolveExpr(SProgram* sprog, Vars* vars, FblcLocId* loc_id, FblcExpr* expr)
{
  (*loc_id)++;
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      SName* var = LocIdName(sprog->symbols, (*loc_id)++);
      for (size_t i = 0; vars != NULL; ++i) {
        if (NamesEqual(vars->name, var->name)) {
          var_expr->var = i;
          return vars->type;
        }
        vars = vars->next;
      }
      ReportError("variable '%s' not defined.", var->loc, var->name);
      return NULL_ID;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      SName* func = LocIdName(sprog->symbols, (*loc_id)++);
      app_expr->func = SLookupDecl(sprog, func->name);
      if (app_expr->func == NULL_ID) {
        ReportError("'%s' not defined.\n", func->loc, func->name);
        return NULL_ID;
      }

      for (size_t i = 0; i < app_expr->argc; ++i) {
        if (ResolveExpr(sprog, vars, loc_id, app_expr->argv[i]) == NULL_ID) {
          return NULL_ID;
        }
      }
      return ReturnType(sprog, app_expr->func);
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcLocId field_loc = (*loc_id)++;

      FblcTypeId type_id = ResolveExpr(sprog, vars, loc_id, access_expr->arg);
      if (type_id == NULL_ID) {
        return NULL_ID;
      }

      access_expr->field = LookupField(sprog, type_id, field_loc);
      if (access_expr->field == NULL_ID) {
        return NULL_ID;
      }
      return FieldType(sprog, type_id, access_expr->field);
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      SName* type = LocIdName(sprog->symbols, (*loc_id)++);
      union_expr->type = LookupType(sprog, type);
      if (union_expr->type == NULL_ID) {
        return NULL_ID;
      }

      union_expr->field = LookupField(sprog, union_expr->type, (*loc_id)++);
      if (union_expr->field == NULL_ID) {
        return NULL_ID;
      }

      if (ResolveExpr(sprog, vars, loc_id, union_expr->arg) == NULL_ID) {
        return NULL_ID;
      }
      return union_expr->type;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      SName* var = LocIdName(sprog->symbols, *loc_id);
      SName* type = LocIdType(sprog->symbols, (*loc_id)++);
      let_expr->type = LookupType(sprog, type);
      if (let_expr->type == NULL_ID) {
        return NULL_ID;
      }

      if (ResolveExpr(sprog, vars, loc_id, let_expr->def) == NULL_ID) {
        return NULL_ID;
      }

      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (NamesEqual(curr->name, var->name)) {
          ReportError("Redefinition of variable '%s'\n", var->loc, var->name);
          return NULL_ID;
        }
      }

      Vars nvars;
      AddVar(&nvars, let_expr->type, var->name, vars);
      return ResolveExpr(sprog, &nvars, loc_id, let_expr->body);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      if (ResolveExpr(sprog, vars, loc_id, cond_expr->select) == NULL_ID) {
        return NULL_ID;
      }

      FblcTypeId result_type_id = NULL_ID;
      assert(cond_expr->argc > 0);
      for (size_t i = 0; i < cond_expr->argc; ++i) {
        result_type_id = ResolveExpr(sprog, vars, loc_id, cond_expr->argv[i]);
        if (result_type_id == NULL_ID) {
          return NULL_ID;
        }
      }
      return result_type_id;
    }

    default: {
      INVALID_CASE();
      return NULL_ID;
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
//   The id of the type of the action, or NULL_ID if name resolution failed.
//
// Side effects:
//   IDs in the action are resolved.
//   Advances loc_id past the action.
//   Prints error messages to stderr in case of error.
static FblcTypeId ResolveActn(SProgram* sprog, Vars* vars, Vars* ports, FblcLocId* loc_id, FblcActn* actn)
{
  (*loc_id)++;
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return ResolveExpr(sprog, vars, loc_id, eval_actn->arg);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      SName* port = LocIdName(sprog->symbols, (*loc_id)++);
      for (size_t i = 0; ports != NULL; ++i) {
        if (NamesEqual(ports->name, port->name)) {
          get_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", port->loc, port->name);
      return NULL_ID;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      SName* port = LocIdName(sprog->symbols, (*loc_id)++);

      if (ResolveExpr(sprog, vars, loc_id, put_actn->arg) == NULL_ID) {
        return NULL_ID;
      }

      for (size_t i = 0; ports != NULL; ++i) {
        if (NamesEqual(ports->name, port->name)) {
          put_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", port->loc, port->name);
      return NULL_ID;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      SName* proc = LocIdName(sprog->symbols, (*loc_id)++);
      call_actn->proc = SLookupDecl(sprog, proc->name);
      if (call_actn->proc == NULL_ID) {
        ReportError("'%s' not defined.\n", proc->loc, proc->name);
        return NULL_ID;
      }

      for (size_t i = 0; i < call_actn->portc; ++i) {
        SName* port = LocIdName(sprog->symbols, (*loc_id)++);
        call_actn->portv[i] = NULL_ID;
        Vars* curr = ports;
        for (size_t port_id = 0; curr != NULL; ++port_id) {
          if (NamesEqual(curr->name, port->name)) {
            call_actn->portv[i] = port_id;
            break;
          }
          curr = curr->next;
        }
        if (call_actn->portv[i] == NULL_ID) {
          ReportError("port '%s' not defined.\n", port->loc, port->name);
          return NULL_ID;
        }
      }

      for (size_t i = 0 ; i < call_actn->argc; ++i) {
        if (ResolveExpr(sprog, vars, loc_id, call_actn->argv[i]) == NULL_ID) {
          return NULL_ID;
        }
      }

      return ReturnType(sprog, call_actn->proc);
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      SName* get = LocIdLinkGet(sprog->symbols, *loc_id);
      SName* put = LocIdLinkPut(sprog->symbols, *loc_id);
      SName* type = LocIdType(sprog->symbols, (*loc_id)++);
      link_actn->type = LookupType(sprog, type);
      if (link_actn->type == NULL_ID) {
        return NULL_ID;
      }

      for (Vars* curr = ports; curr != NULL; curr = curr->next) {
        if (NamesEqual(curr->name, get->name)) {
          ReportError("Redefinition of port '%s'\n", get->loc, get->name);
          return NULL_ID;
        }
        if (NamesEqual(curr->name, put->name)) {
          ReportError("Redefinition of port '%s'\n", put->loc, put->name);
          return NULL_ID;
        }
      }

      Vars getport;
      Vars putport;
      AddVar(&getport, link_actn->type, get->name, ports);
      AddVar(&putport, link_actn->type, put->name, &getport);
      return ResolveActn(sprog, vars, &putport, loc_id, link_actn->body);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      Vars vars_data[exec_actn->execc];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execc; ++i) {
        FblcExec* exec = exec_actn->execv + i;
        SName* var = LocIdName(sprog->symbols, *loc_id);
        SName* type = LocIdType(sprog->symbols, (*loc_id)++);
        exec->type = LookupType(sprog, type);
        if (ResolveActn(sprog, vars, ports, loc_id, exec->actn) == NULL_ID) {
          return NULL_ID;
        }
        nvars = AddVar(vars_data+i, exec->type, var->name, nvars);
      }
      return ResolveActn(sprog, nvars, ports, loc_id, exec_actn->body);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      if (ResolveExpr(sprog, vars, loc_id, cond_actn->select) == NULL_ID) {
        return NULL_ID;
      }

      FblcTypeId result_type_id = NULL_ID;
      assert(cond_actn->argc > 0);
      for (size_t i = 0; i < cond_actn->argc; ++i) {
        result_type_id = ResolveActn(sprog, vars, ports, loc_id, cond_actn->argv[i]);
        if (result_type_id == NULL_ID) {
          return NULL_ID;
        }
      }
      return result_type_id;
    }

    default: {
      INVALID_CASE();
      return NULL_ID;
    }
  }
}

// ResolveProgram -- see fblcs.h for documentation.
bool ResolveProgram(SProgram* sprog)
{
  FblcLocId loc_id = 0;
  for (FblcDeclId decl_id = 0; decl_id < sprog->program->declc; ++decl_id) {
    SName* name = LocIdName(sprog->symbols, loc_id++);
    if (SLookupDecl(sprog, name->name) != decl_id) {
      ReportError("Redefinition of %s\n", name->loc, name->name);
      return false;
    }

    FblcDecl* decl = sprog->program->declv[decl_id];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL:
      case FBLC_UNION_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        for (FblcFieldId field_id = 0; field_id < type->fieldc; ++field_id) {
          SName* field = LocIdName(sprog->symbols, loc_id++);
          for (FblcFieldId i = 0; i < field_id; ++i) {
            SName* prior_field = LocIdName(sprog->symbols, loc_id - i - 2);
            if (NamesEqual(field->name, prior_field->name)) {
              ReportError("Redefinition of field %s\n", field->loc, field->name);
              return false;
            }
          }

          type->fieldv[field_id] = FieldType(sprog, decl_id, field_id);
          if (type->fieldv[field_id] == NULL_ID) {
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
          SName* var = LocIdName(sprog->symbols, loc_id);
          SName* type = LocIdType(sprog->symbols, loc_id++);
          func->argv[i] = LookupType(sprog, type);
          if (func->argv[i] == NULL_ID) {
            return false;
          }
          for (Vars* curr = vars; curr != NULL; curr = curr->next) {
            if (NamesEqual(curr->name, var->name)) {
              ReportError("Redefinition of argument '%s'\n", var->loc, var->name);
              return NULL_ID;
            }
          }
          vars = AddVar(nvars+i, func->argv[i], var->name, vars);
        }

        SName* return_type = LocIdName(sprog->symbols, loc_id++);
        func->return_type = LookupType(sprog, return_type);
        if (func->return_type == NULL_ID) {
          return false;
        }

        if (ResolveExpr(sprog, vars, &loc_id, func->body) == NULL_ID) {
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        FblcProcDecl* proc = (FblcProcDecl*)decl;
        Vars nports[proc->portc];
        Vars* ports = NULL;
        for (size_t i = 0; i < proc->portc; ++i) {
          SName* port = LocIdName(sprog->symbols, loc_id);
          SName* type = LocIdType(sprog->symbols, loc_id++);
          proc->portv[i].type = LookupType(sprog, type);
          if (proc->portv[i].type == NULL_ID) {
            return false;
          }
          for (Vars* curr = ports; curr != NULL; curr = curr->next) {
            if (NamesEqual(curr->name, port->name)) {
              ReportError("Redefinition of port '%s'\n", port->loc, port->name);
              return NULL_ID;
            }
          }
          ports = AddVar(nports+i, proc->portv[i].type, port->name, ports);
        }

        Vars nvars[proc->argc];
        Vars* vars = NULL;
        for (size_t i = 0; i < proc->argc; ++i) {
          SName* var = LocIdName(sprog->symbols, loc_id);
          SName* type = LocIdType(sprog->symbols, loc_id++);
          proc->argv[i] = LookupType(sprog, type);
          if (proc->argv[i] == NULL_ID) {
            return false;
          }
          for (Vars* curr = vars; curr != NULL; curr = curr->next) {
            if (NamesEqual(curr->name, var->name)) {
              ReportError("Redefinition of argument '%s'\n", var->loc, var->name);
              return NULL_ID;
            }
          }
          vars = AddVar(nvars+i, proc->argv[i], var->name, vars);
        }

        SName* return_type = LocIdName(sprog->symbols, loc_id++);
        proc->return_type = LookupType(sprog, return_type);
        if (proc->return_type == NULL_ID) {
          return false;
        }

        if (ResolveActn(sprog, vars, ports, &loc_id, proc->body) == NULL_ID) {
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
