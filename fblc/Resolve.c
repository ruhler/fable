// Resolve.c --
//   This file provides routines for performing resolution of references to
//   variables, ports, declarations, and fields of an fblc program.

#include <assert.h>     // for assert

#include "fblct.h"

typedef struct Vars {
  FblcTypeId type;
  Name name;
  struct Vars* next;
} Vars;

static Vars* AddVar(Vars* vars, FblcTypeId type, Name name, Vars* next);
static FblcTypeId LookupType(SProgram* sprog, Name name);
static FblcTypeId ResolveExpr(SProgram* sprog, SName* names, Vars* vars, FblcExpr* expr, SVar** svars);
static FblcTypeId ResolveActn(SProgram* sprog, SName* names, Vars* vars, Vars* ports, FblcActn* actn, SVar** svars, SVar** sportv);

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
//   Look up the declaration id of the type with the given name in the given
//   sprog.
// 
// Inputs:
//   sprog - The program to look up the type in.
//   name - The name of the type to look up.
//
// Result:
//   The declaration id for the type with the given name, or UNRESOLVED_ID if
//   there is no type with the given name in the given environment.
//
// Side effects:
//   None.
static FblcTypeId LookupType(SProgram* sprog, Name name)
{
  for (size_t i = 0; i < sprog->program->declc; ++i) {
    FblcDecl* decl = sprog->program->declv[i];
    SDecl* sdecl = sprog->symbols[i];
    if ((decl->tag == FBLC_STRUCT_DECL || decl->tag == FBLC_UNION_DECL)
        && NamesEqual(sdecl->name.name, name)) {
      return i;
    }
  }
  return UNRESOLVED_ID;
}

static FblcTypeId ResolveExpr(SProgram* sprog, SName* names, Vars* vars, FblcExpr* expr, SVar** svars)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      SName* name = names + var_expr->var;
      for (size_t i = 0; vars != NULL; ++i) {
        if (NamesEqual(vars->name, name->name)) {
          var_expr->var = i;
          return vars->type;
        }
        vars = vars->next;
      }

      ReportError("Variable %s not in scope.", name->loc, name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      SName* name = names + app_expr->func;
      app_expr->func = UNRESOLVED_ID;
      for (size_t i = 0; i < sprog->program->declc; ++i) {
        if (NamesEqual(name->name, sprog->symbols[i]->name.name)) {
          app_expr->func = i;
          break;
        }
      }
      if (app_expr->func == UNRESOLVED_ID) {
        ReportError("Declaration for '%s' not found.\n", name->loc, name->name);
        return UNRESOLVED_ID;
      }

      for (size_t i = 0 ; i < app_expr->argc; ++i) {
        if (ResolveExpr(sprog, names, vars, app_expr->argv[i], svars) == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
      }

      switch (sprog->program->declv[app_expr->func]->tag) {
        case FBLC_STRUCT_DECL: {
          return app_expr->func;
        }

        case FBLC_UNION_DECL: {
          ReportError("Cannot do application on union type %s.\n", name->loc, name->name);
          return UNRESOLVED_ID;
        }

        case FBLC_FUNC_DECL: {
          FblcFuncDecl* func = (FblcFuncDecl*)sprog->program->declv[app_expr->func];
          return func->return_type;
        }

        case FBLC_PROC_DECL: {
          ReportError("Cannot do application on a process %s.\n", name->loc, name->name);
          return UNRESOLVED_ID;
        }

        default:
          assert(false && "Invalid decl tag");
          return UNRESOLVED_ID;
      }
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcTypeId type_id = ResolveExpr(sprog, names, vars, access_expr->object, svars);
      if (type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      SName* name = names + access_expr->field;

      FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];
      STypeDecl* stype = (STypeDecl*)sprog->symbols[type_id];
      for (size_t i = 0; i < type->fieldc; ++i) {
        if (NamesEqual(stype->fields[i].name.name, name->name)) {
          access_expr->field = i;
          return LookupType(sprog, stype->fields[i].type.name);
        }
      }
      ReportError("'%s' is not a field of the type '%s'.\n",
          name->loc, name->name, stype->name.name);
      return UNRESOLVED_ID;
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      SName* type_name = names + union_expr->type;
      SName* field_name = names + union_expr->field;
      union_expr->type = LookupType(sprog, type_name->name);
      if (union_expr->type == UNRESOLVED_ID) {
        ReportError("Type %s not found.\n", type_name->loc, type_name->name);
        return UNRESOLVED_ID;
      }
      FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[union_expr->type];
      STypeDecl* stype = (STypeDecl*)sprog->symbols[union_expr->type];

      if (ResolveExpr(sprog, names, vars, union_expr->body, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      union_expr->field = UNRESOLVED_ID;
      for (size_t i = 0; i < type->fieldc; ++i) {
        if (NamesEqual(stype->fields[i].name.name, field_name->name)) {
          union_expr->field = i;
          return union_expr->type;
        }
      }

      ReportError("Type '%s' has no field '%s'.\n", field_name->loc, type_name->name, field_name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      SVar* svar = (*svars)++;

      FblcTypeId var_type_id = ResolveExpr(sprog, names, vars, let_expr->def, svars);
      if (var_type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (NamesEqual(curr->name, svar->name.name)) {
          ReportError("Redefinition of variable '%s'\n", svar->name.loc, svar->name.name);
          return UNRESOLVED_ID;
        }
      }
      Vars nvars;
      AddVar(&nvars, var_type_id, svar->name.name, vars);
      return ResolveExpr(sprog, names, &nvars, let_expr->body, svars);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      if (ResolveExpr(sprog, names, vars, cond_expr->select, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      FblcTypeId result_type_id = UNRESOLVED_ID;
      for (size_t i = 0; i < cond_expr->argc; ++i) {
        result_type_id = ResolveExpr(sprog, names, vars, cond_expr->argv[i], svars);
        if (result_type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
      }
      return result_type_id;
    }

    default: {
      assert(false && "Unknown expression type.");
      return UNRESOLVED_ID;
    }
  }
}

static FblcTypeId ResolveActn(SProgram* sprog, SName* names, Vars* vars, Vars* ports, FblcActn* actn, SVar** svars, SVar** sportv)
{
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return ResolveExpr(sprog, names, vars, eval_actn->expr, svars);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      SName* name = names + get_actn->port;

      for (size_t i = 0; ports != NULL; ++i) {
        if (NamesEqual(ports->name, name->name)) {
          get_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      ReportError("'%s' is not a valid port.\n", name->loc, name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      SName* name = names + put_actn->port;

      if (ResolveExpr(sprog, names, vars, put_actn->arg, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      for (size_t i = 0; ports != NULL; ++i) {
        if (NamesEqual(ports->name, name->name)) {
          put_actn->port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      ReportError("'%s' is not a valid put port.\n", name->loc, name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      SName* name = names + call_actn->proc;

      call_actn->proc = UNRESOLVED_ID;
      for (size_t i = 0; i < sprog->program->declc; ++i) {
        FblcDecl* decl = sprog->program->declv[i];
        SDecl* sdecl = sprog->symbols[i];
        if (decl->tag == FBLC_PROC_DECL && NamesEqual(sdecl->name.name, name->name)) {
          call_actn->proc = i;
          break;
        }
      }
      if (call_actn->proc == UNRESOLVED_ID) {
        ReportError("'%s' is not a proc.\n", name->loc, name->name);
        return UNRESOLVED_ID;
      }

      FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv[call_actn->proc];
      if (proc->portc != call_actn->portc) {
        ReportError("Wrong number of port arguments to '%s'. Expected %i but found %i.\n", name->loc, name->name, proc->portc, call_actn->portc);
        return UNRESOLVED_ID;
      }

      for (size_t i = 0; i < proc->portc; i++) {
        SName* port_name = names + call_actn->portv[i];
        call_actn->portv[i] = UNRESOLVED_ID;
        Vars* curr = ports;
        for (size_t port_id = 0; curr != NULL; ++port_id) {
          if (NamesEqual(curr->name, port_name->name)) {
            call_actn->portv[i] = port_id;
            break;
          }
          curr = curr->next;
        }
        if (call_actn->portv[i] == UNRESOLVED_ID) {
          ReportError("'%s' is not a valid port.\n", port_name->loc, port_name->name);
          return UNRESOLVED_ID;
        }
      }

      for (size_t i = 0 ; i < call_actn->argc; ++i) {
        if (ResolveExpr(sprog, names, vars, call_actn->argv[i], svars) == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
      }

      return proc->return_type;
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      SVar* sget = (*sportv)++;
      SVar* sput = (*sportv)++;
      link_actn->type = LookupType(sprog, sget->type.name);
      if (link_actn->type == UNRESOLVED_ID) {
        ReportError("Type '%s' not declared.\n", sget->type.loc, sget->type.name);
        return UNRESOLVED_ID;
      }

      Vars getport;
      Vars putport;
      AddVar(&getport, link_actn->type, sget->name.name, ports);
      AddVar(&putport, link_actn->type, sput->name.name, &getport);
      return ResolveActn(sprog, names, vars, &putport, link_actn->body, svars, sportv);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      Vars vars_data[exec_actn->execc];
      Vars* nvars = vars;
      for (int i = 0; i < exec_actn->execc; i++) {
        SVar* svar = (*svars)++;
        FblcActn* exec = exec_actn->execv[i];
        FblcTypeId type_id = ResolveActn(sprog, names, vars, ports, exec, svars, sportv);
        if (type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
        nvars = AddVar(vars_data+i, type_id, svar->name.name, nvars);
      }
      return ResolveActn(sprog, names, nvars, ports, exec_actn->body, svars, sportv);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      if (ResolveExpr(sprog, names, vars, cond_actn->select, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      FblcTypeId result_type_id = UNRESOLVED_ID;
      for (size_t i = 0; i < cond_actn->argc; ++i) {
        result_type_id = ResolveActn(sprog, names, vars, ports, cond_actn->argv[i], svars, sportv);
        if (result_type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
      }
      return result_type_id;
    }
  }
  assert(false && "UNREACHABLE");
  return UNRESOLVED_ID;
}

// ResolveProgram -- see fblct.h for documentation.
bool ResolveProgram(SProgram* sprog, SName* names)
{
  // Resolve names in declarations first, before resolving names in the bodies
  // of functions or processes. We must resolve the function and process
  // return types before we can do name resolution in bodies.
  for (size_t i = 0; i < sprog->program->declc; ++i) {
    FblcDecl* decl = sprog->program->declv[i];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL:
      case FBLC_UNION_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        STypeDecl* stype = (STypeDecl*)sprog->symbols[i];
        for (size_t i = 0; i < type->fieldc; ++i) {
          type->fieldv[i] = LookupType(sprog, stype->fields[i].type.name);
          if (type->fieldv[i] == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", stype->fields[i].type.loc, stype->fields[i].type.name);
            return false;
          }
        }
        break;
      }

      case FBLC_FUNC_DECL: {
        FblcFuncDecl* func = (FblcFuncDecl*)decl;
        SFuncDecl* sfunc = (SFuncDecl*)sprog->symbols[i];

        for (size_t i = 0; i < func->argc; ++i) {
          func->argv[i] = LookupType(sprog, sfunc->svarv[i].type.name);
          if (func->argv[i] == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", sfunc->svarv[i].type.loc, sfunc->svarv[i].type.name);
            return false;
          }
        }

        SName* name = names + func->return_type;
        func->return_type = LookupType(sprog, name->name);
        if (func->return_type == UNRESOLVED_ID) {
          ReportError("Type '%s' not found.\n", name->loc, name->name);
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        FblcProcDecl* proc = (FblcProcDecl*)decl;
        SProcDecl* sproc = (SProcDecl*)sprog->symbols[i];

        for (size_t i = 0; i < proc->portc; ++i) {
          proc->portv[i].type = LookupType(sprog, sproc->sportv[i].type.name);
          if (proc->portv[i].type == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", sproc->sportv[i].type.loc, sproc->sportv[i].type.name);
            return false;
          }
        }

        for (size_t i = 0; i < proc->argc; ++i) {
          proc->argv[i] = LookupType(sprog, sproc->svarv[i].type.name);
          if (proc->argv[i] == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", sproc->svarv[i].type.loc, sproc->svarv[i].type.name);
            return false;
          }
        }

        SName* name = names + proc->return_type;
        proc->return_type = LookupType(sprog, name->name);
        if (proc->return_type == UNRESOLVED_ID) {
          ReportError("Type '%s' not found.\n", name->loc, name->name);
          return false;
        }
        break;
      }

      default:
        assert(false && "Invalid decl type");
        return false;
    }
  }

  // Now resolve function and process bodies
  for (size_t i = 0; i < sprog->program->declc; ++i) {
    FblcDecl* decl = sprog->program->declv[i];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL: break;
      case FBLC_UNION_DECL: break;

      case FBLC_FUNC_DECL: {
        FblcFuncDecl* func = (FblcFuncDecl*)decl;
        SFuncDecl* sfunc = (SFuncDecl*)sprog->symbols[i];

        Vars nvars[func->argc];
        Vars* vars = NULL;
        SVar* svars = sfunc->svarv;
        for (size_t i = 0; i < func->argc; ++i) {
          vars = AddVar(nvars+i, func->argv[i], (svars++)->name.name, vars);
        }

        if (ResolveExpr(sprog, names, vars, func->body, &svars) == UNRESOLVED_ID) {
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        FblcProcDecl* proc = (FblcProcDecl*)decl;
        SProcDecl* sproc = (SProcDecl*)sprog->symbols[i];

        Vars nports[proc->portc];
        Vars* ports = NULL;
        SVar* sports = sproc->sportv;
        for (size_t i = 0; i < proc->portc; ++i) {
          ports = AddVar(nports+i, proc->portv[i].type, (sports++)->name.name, ports);
        }

        Vars nvars[proc->argc];
        Vars* vars = NULL;
        SVar* svars = sproc->svarv;
        for (size_t i = 0; i < proc->argc; ++i) {
          vars = AddVar(nvars+i, proc->argv[i], (svars++)->name.name, vars);
        }

        if (ResolveActn(sprog, names, vars, ports, proc->body, &svars, &sports) == UNRESOLVED_ID) {
          return false;
        }
        break;
      }

      default:
        assert(false && "Invalid decl type");
        return false;
    }
  }
  return true;
}
