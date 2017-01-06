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
static FblcTypeId LookupType(Env* env, Name name);
static FblcTypeId ResolveExpr(Env* env, LocName* names, Vars* vars, Expr* expr, SVar** svars);
static FblcTypeId ResolveActn(Env* env, LocName* names, Vars* vars, Vars* ports, Actn* actn, SVar** svars, SVar** sportv);

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
//   environment.
// 
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the type to look up.
//
// Result:
//   The declaration id for the type with the given name, or UNRESOLVED_ID if
//   there is no type with the given name in the given environment.
//
// Side effects:
//   None.
static FblcTypeId LookupType(Env* env, Name name)
{
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    SDecl* sdecl = env->sdeclv[i];
    if ((decl->tag == FBLC_STRUCT_DECL || decl->tag == FBLC_UNION_DECL)
        && NamesEqual(sdecl->name.name, name)) {
      return i;
    }
  }
  return UNRESOLVED_ID;
}

static FblcTypeId ResolveExpr(Env* env, LocName* names, Vars* vars, Expr* expr, SVar** svars)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      VarExpr* var_expr = (VarExpr*)expr;
      LocName* name = names + var_expr->x.var;
      for (size_t i = 0; vars != NULL; ++i) {
        if (NamesEqual(vars->name, name->name)) {
          var_expr->x.var = i;
          return vars->type;
        }
        vars = vars->next;
      }

      ReportError("Variable %s not in scope.", name->loc, name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_APP_EXPR: {
      AppExpr* app_expr = (AppExpr*)expr;
      LocName* name = names + app_expr->x.func;
      app_expr->x.func = UNRESOLVED_ID;
      for (size_t i = 0; i < env->declc; ++i) {
        if (NamesEqual(name->name, env->sdeclv[i]->name.name)) {
          app_expr->x.func = i;
          break;
        }
      }
      if (app_expr->x.func == UNRESOLVED_ID) {
        ReportError("Declaration for '%s' not found.\n", name->loc, name->name);
        return UNRESOLVED_ID;
      }

      for (size_t i = 0 ; i < app_expr->x.argc; ++i) {
        if (ResolveExpr(env, names, vars, (Expr*)app_expr->x.argv[i], svars) == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
      }

      switch (env->declv[app_expr->x.func]->tag) {
        case FBLC_STRUCT_DECL: {
          return app_expr->x.func;
        }

        case FBLC_UNION_DECL: {
          ReportError("Cannot do application on union type %s.\n", name->loc, name->name);
          return UNRESOLVED_ID;
        }

        case FBLC_FUNC_DECL: {
          FuncDecl* func = (FuncDecl*)env->declv[app_expr->x.func];
          return func->return_type_id;
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
      AccessExpr* access_expr = (AccessExpr*)expr;
      FblcTypeId type_id = ResolveExpr(env, names, vars, (Expr*)access_expr->x.object, svars);
      if (type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      LocName* name = names + access_expr->x.field;

      FblcTypeDecl* type = (FblcTypeDecl*)env->declv[type_id];
      STypeDecl* stype = (STypeDecl*)env->sdeclv[type_id];
      for (size_t i = 0; i < type->fieldc; ++i) {
        if (NamesEqual(stype->fields[i].name.name, name->name)) {
          access_expr->x.field = i;
          return LookupType(env, stype->fields[i].type.name);
        }
      }
      ReportError("'%s' is not a field of the type '%s'.\n",
          name->loc, name->name, stype->name.name);
      return UNRESOLVED_ID;
    }

    case FBLC_UNION_EXPR: {
      UnionExpr* union_expr = (UnionExpr*)expr;
      LocName* type_name = names + union_expr->x.type;
      LocName* field_name = names + union_expr->x.field;
      union_expr->x.type = LookupType(env, type_name->name);
      if (union_expr->x.type == UNRESOLVED_ID) {
        ReportError("Type %s not found.\n", type_name->loc, type_name->name);
        return UNRESOLVED_ID;
      }
      FblcTypeDecl* type = (FblcTypeDecl*)env->declv[union_expr->x.type];
      STypeDecl* stype = (STypeDecl*)env->sdeclv[union_expr->x.type];

      if (ResolveExpr(env, names, vars, (Expr*)union_expr->x.body, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      union_expr->x.field = UNRESOLVED_ID;
      for (size_t i = 0; i < type->fieldc; ++i) {
        if (NamesEqual(stype->fields[i].name.name, field_name->name)) {
          union_expr->x.field = i;
          return union_expr->x.type;
        }
      }

      ReportError("Type '%s' has no field '%s'.\n", field_name->loc, type_name->name, field_name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_LET_EXPR: {
      LetExpr* let_expr = (LetExpr*)expr;
      SVar* svar = (*svars)++;

      FblcTypeId var_type_id = ResolveExpr(env, names, vars, (Expr*)let_expr->x.def, svars);
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
      return ResolveExpr(env, names, &nvars, (Expr*)let_expr->x.body, svars);
    }

    case FBLC_COND_EXPR: {
      CondExpr* cond_expr = (CondExpr*)expr;
      if (ResolveExpr(env, names, vars, (Expr*)cond_expr->x.select, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      FblcTypeId result_type_id = UNRESOLVED_ID;
      for (size_t i = 0; i < cond_expr->x.argc; ++i) {
        result_type_id = ResolveExpr(env, names, vars, (Expr*)cond_expr->x.argv[i], svars);
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

static FblcTypeId ResolveActn(Env* env, LocName* names, Vars* vars, Vars* ports, Actn* actn, SVar** svars, SVar** sportv)
{
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      EvalActn* eval_actn = (EvalActn*)actn;
      return ResolveExpr(env, names, vars, (Expr*)eval_actn->x.expr, svars);
    }

    case FBLC_GET_ACTN: {
      GetActn* get_actn = (GetActn*)actn;
      LocName* name = names + get_actn->x.port;

      for (size_t i = 0; ports != NULL; ++i) {
        if (NamesEqual(ports->name, name->name)) {
          get_actn->x.port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      ReportError("'%s' is not a valid port.\n", name->loc, name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_PUT_ACTN: {
      PutActn* put_actn = (PutActn*)actn;
      LocName* name = names + put_actn->x.port;

      if (ResolveExpr(env, names, vars, (Expr*)put_actn->x.arg, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      for (size_t i = 0; ports != NULL; ++i) {
        if (NamesEqual(ports->name, name->name)) {
          put_actn->x.port = i;
          return ports->type;
        }
        ports = ports->next;
      }
      ReportError("'%s' is not a valid put port.\n", name->loc, name->name);
      return UNRESOLVED_ID;
    }

    case FBLC_CALL_ACTN: {
      CallActn* call_actn = (CallActn*)actn;
      LocName* name = names + call_actn->x.proc;

      call_actn->x.proc = UNRESOLVED_ID;
      for (size_t i = 0; i < env->declc; ++i) {
        Decl* decl = env->declv[i];
        SDecl* sdecl = env->sdeclv[i];
        if (decl->tag == FBLC_PROC_DECL && NamesEqual(sdecl->name.name, name->name)) {
          call_actn->x.proc = i;
          break;
        }
      }
      if (call_actn->x.proc == UNRESOLVED_ID) {
        ReportError("'%s' is not a proc.\n", name->loc, name->name);
        return UNRESOLVED_ID;
      }

      ProcDecl* proc = (ProcDecl*)env->declv[call_actn->x.proc];
      if (proc->portc != call_actn->x.portc) {
        ReportError("Wrong number of port arguments to '%s'. Expected %i but found %i.\n", name->loc, name->name, proc->portc, call_actn->x.portc);
        return UNRESOLVED_ID;
      }

      for (size_t i = 0; i < proc->portc; i++) {
        LocName* port_name = names + call_actn->x.portv[i];
        call_actn->x.portv[i] = UNRESOLVED_ID;
        Vars* curr = ports;
        for (size_t port_id = 0; curr != NULL; ++port_id) {
          if (NamesEqual(curr->name, port_name->name)) {
            call_actn->x.portv[i] = port_id;
            break;
          }
          curr = curr->next;
        }
        if (call_actn->x.portv[i] == UNRESOLVED_ID) {
          ReportError("'%s' is not a valid port.\n", port_name->loc, port_name->name);
          return UNRESOLVED_ID;
        }
      }

      for (size_t i = 0 ; i < call_actn->x.argc; ++i) {
        if (ResolveExpr(env, names, vars, (Expr*)call_actn->x.argv[i], svars) == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
      }

      return proc->return_type_id;
    }

    case FBLC_LINK_ACTN: {
      LinkActn* link_actn = (LinkActn*)actn;
      SVar* sget = (*sportv)++;
      SVar* sput = (*sportv)++;
      link_actn->x.type = LookupType(env, sget->type.name);
      if (link_actn->x.type == UNRESOLVED_ID) {
        ReportError("Type '%s' not declared.\n", sget->type.loc, sget->type.name);
        return UNRESOLVED_ID;
      }

      Vars getport;
      Vars putport;
      AddVar(&getport, link_actn->x.type, sget->name.name, ports);
      AddVar(&putport, link_actn->x.type, sput->name.name, &getport);
      return ResolveActn(env, names, vars, &putport, (Actn*)link_actn->x.body, svars, sportv);
    }

    case FBLC_EXEC_ACTN: {
      ExecActn* exec_actn = (ExecActn*)actn;
      Vars vars_data[exec_actn->x.execc];
      Vars* nvars = vars;
      for (int i = 0; i < exec_actn->x.execc; i++) {
        SVar* svar = (*svars)++;
        Actn* exec = (Actn*)exec_actn->x.execv[i];
        FblcTypeId type_id = ResolveActn(env, names, vars, ports, exec, svars, sportv);
        if (type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
        nvars = AddVar(vars_data+i, type_id, svar->name.name, nvars);
      }
      return ResolveActn(env, names, nvars, ports, (Actn*)exec_actn->x.body, svars, sportv);
    }

    case FBLC_COND_ACTN: {
      CondActn* cond_actn = (CondActn*)actn;
      if (ResolveExpr(env, names, vars, (Expr*)cond_actn->x.select, svars) == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      FblcTypeId result_type_id = UNRESOLVED_ID;
      for (size_t i = 0; i < cond_actn->x.argc; ++i) {
        result_type_id = ResolveActn(env, names, vars, ports, (Actn*)cond_actn->x.argv[i], svars, sportv);
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
bool ResolveProgram(Env* env, LocName* names)
{
  // Resolve names in declarations first, before resolving names in the bodies
  // of functions or processes. We must resolve the function and process
  // return types before we can do name resolution in bodies.
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL:
      case FBLC_UNION_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        STypeDecl* stype = (STypeDecl*)env->sdeclv[i];
        for (size_t i = 0; i < type->fieldc; ++i) {
          type->fieldv[i] = LookupType(env, stype->fields[i].type.name);
          if (type->fieldv[i] == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", stype->fields[i].type.loc, stype->fields[i].type.name);
            return false;
          }
        }
        break;
      }

      case FBLC_FUNC_DECL: {
        FuncDecl* func = (FuncDecl*)decl;
        SFuncDecl* sfunc = (SFuncDecl*)env->sdeclv[i];

        for (size_t i = 0; i < func->argc; ++i) {
          func->argv[i] = LookupType(env, sfunc->svarv[i].type.name);
          if (func->argv[i] == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", sfunc->svarv[i].type.loc, sfunc->svarv[i].type.name);
            return false;
          }
        }

        LocName* name = names + func->return_type_id;
        func->return_type_id = LookupType(env, name->name);
        if (func->return_type_id == UNRESOLVED_ID) {
          ReportError("Type '%s' not found.\n", name->loc, name->name);
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        ProcDecl* proc = (ProcDecl*)decl;
        SProcDecl* sproc = (SProcDecl*)env->sdeclv[i];

        for (size_t i = 0; i < proc->portc; ++i) {
          proc->portv[i].type = LookupType(env, sproc->sportv[i].type.name);
          if (proc->portv[i].type == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", sproc->sportv[i].type.loc, sproc->sportv[i].type.name);
            return false;
          }
        }

        for (size_t i = 0; i < proc->argc; ++i) {
          proc->argv[i] = LookupType(env, sproc->svarv[i].type.name);
          if (proc->argv[i] == UNRESOLVED_ID) {
            ReportError("Type '%s' not found.\n", sproc->svarv[i].type.loc, sproc->svarv[i].type.name);
            return false;
          }
        }

        LocName* name = names + proc->return_type_id;
        proc->return_type_id = LookupType(env, name->name);
        if (proc->return_type_id == UNRESOLVED_ID) {
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
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL: break;
      case FBLC_UNION_DECL: break;

      case FBLC_FUNC_DECL: {
        FuncDecl* func = (FuncDecl*)decl;
        SFuncDecl* sfunc = (SFuncDecl*)env->sdeclv[i];

        Vars nvars[func->argc];
        Vars* vars = NULL;
        SVar* svars = sfunc->svarv;
        for (size_t i = 0; i < func->argc; ++i) {
          vars = AddVar(nvars+i, func->argv[i], (svars++)->name.name, vars);
        }

        if (ResolveExpr(env, names, vars, func->body, &svars) == UNRESOLVED_ID) {
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        ProcDecl* proc = (ProcDecl*)decl;
        SProcDecl* sproc = (SProcDecl*)env->sdeclv[i];

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

        if (ResolveActn(env, names, vars, ports, proc->body, &svars, &sports) == UNRESOLVED_ID) {
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
