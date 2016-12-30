// Checker.c --
//
//   This file implements routines for checking an  program is well formed
//   and well typed.

#include <assert.h>     // for assert

#include "fblct.h"

// The following Vars structure describes a mapping from variable names to
// their types and ids.
//
// We always allocate the Vars mapping on the stack. This means it is not safe
// to return or otherwise capture a Vars* outside of the frame where it is
// allocated.

typedef struct Vars {
  Name name;
  FblcTypeId type;
  struct Vars* next;
} Vars;

typedef struct Ports {
  Name name;
  FblcPolarity polarity;
  FblcTypeId type;
  struct Ports* next;
} Ports;

static Loc* ExprLoc(Expr* expr);
static Loc* ActnLoc(Actn* actn);

static FblcTypeId LookupType(Env* env, Name name);
static Vars* AddVar(Vars* vars, Name name, FblcTypeId type, Vars* next);
static FblcTypeId ResolveVar(Vars* vars, LocName* name, FblcVarId* var_id);
static Ports* AddPort(
    Ports* vars, Name name, FblcTypeId type, FblcPolarity polarity, Ports* next);
static FblcTypeId ResolvePort(Ports* vars, LocName* name, FblcPolarity polarity, FblcPortId* port_id);
static bool CheckArgs(
    Env* env, Vars* vars, int fieldc, Field* fieldv,
    int argc, Expr** argv, LocName* func);
static FblcTypeId CheckExpr(Env* env, Vars* vars, Expr* expr);
static FblcTypeId CheckActn(Env* env, Vars* vars, Ports* ports, Actn* actn);
static bool CheckFields(Env* env, int fieldc, FblcTypeId* fieldv, Field* fields, const char* kind);
static bool CheckPorts(Env* env, int portc, FblcPort* portv, Port* ports);
static bool CheckType(Env* env, TypeDecl* type);
static bool CheckFunc(Env* env, FuncDecl* func);
static bool CheckProc(Env* env, ProcDecl* proc);

// ExprLoc --
//   Return the location of an expression.
static Loc* ExprLoc(Expr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      VarExpr* var_expr = (VarExpr*)expr;
      return var_expr->name.loc;
    }

    case FBLC_APP_EXPR: {
      AppExpr* app_expr = (AppExpr*)expr;
      return app_expr->func.loc;
    }

    case FBLC_ACCESS_EXPR: {
      AccessExpr* access_expr = (AccessExpr*)expr;
      return ExprLoc((Expr*)access_expr->x.object);
    }

    case FBLC_UNION_EXPR: {
      UnionExpr* union_expr = (UnionExpr*)expr;
      return union_expr->type.loc;
    }

    case FBLC_LET_EXPR: {
      LetExpr* let_expr = (LetExpr*)expr;
      return let_expr->type.loc;
    }

    case FBLC_COND_EXPR: {
      CondExpr* cond_expr = (CondExpr*)expr;
      return ExprLoc((Expr*)cond_expr->x.select);
    }

    default:
      assert(false && "Invalid expr tag.");
      return NULL;
  }
}

// ActnLoc --
//   Return the location of an action.
static Loc* ActnLoc(Actn* actn)
{
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      EvalActn* eval_actn = (EvalActn*)actn;
      return ExprLoc((Expr*)eval_actn->x.expr);
    }

    case FBLC_GET_ACTN: {
      GetActn* get_actn = (GetActn*)actn;
      return get_actn->port.loc;
    }

    case FBLC_PUT_ACTN: {
      PutActn* put_actn = (PutActn*)actn;
      return put_actn->port.loc;
    }

    case FBLC_CALL_ACTN: {
      CallActn* call_actn = (CallActn*)actn;
      return call_actn->proc.loc;
    }

    case FBLC_LINK_ACTN: {
      LinkActn* link_actn = (LinkActn*)actn;
      return link_actn->type.loc;
    }

    case FBLC_EXEC_ACTN: {
      ExecActn* exec_actn = (ExecActn*)actn;
      return exec_actn->vars[0].type.loc;
    }

    case FBLC_COND_ACTN: {
      CondActn* cond_actn = (CondActn*)actn;
      return ExprLoc((Expr*)cond_actn->x.select);
    }

    default:
      assert(false && "Invalid actn tag.");
      return NULL;
  }
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
    if ((decl->tag == FBLC_STRUCT_DECL || decl->tag == FBLC_UNION_DECL)
        && NamesEqual(DeclName(decl)->name, name)) {
      return i;
    }
  }
  return UNRESOLVED_ID;
}

// AddVar --
//
//   Add a variable to the given scope.
//
// Inputs:
//   vars - Memory to use for the new scope.
//   name - The name of the variable to add.
//   type - The type of the variable.
//   next - The scope to add it to.
//
// Results:
//   The 'vars' scope with the added variable.
//
// Side effects:
//   Sets vars to a scope including the given variable and next scope.

static Vars* AddVar(Vars* vars, Name name, FblcTypeId type, Vars* next)
{
  vars->name = name;
  vars->type = type;
  vars->next = next;
  return vars;
}

// ResolveVar --
//   Look up the type of a variable in scope.
//
// Inputs:
//   vars - The scope to look up the variable in.
//   name - The name of the variable.
//   var_id - Output var id parameter.
//
// Results:
//   The type of the variable in scope, or UNRESOLVED_ID if there is no variable with
//   that name in scope.
//
// Side effects:
//   Sets var_id to the id of the resolved variable
static FblcTypeId ResolveVar(Vars* vars, LocName* name, FblcVarId* var_id)
{
  for (size_t i = 0; vars != NULL; ++i) {
    if (NamesEqual(vars->name, name->name)) {
      *var_id = i;
      return vars->type;
    }
    vars = vars->next;
  }
  return UNRESOLVED_ID;
}

// AddPort --
//
//   Add a port to the given scope.
//
// Inputs:
//   ports - Memory to use for the new scope.
//   name - The name of the port to add.
//   type - The type of the port.
//   polarity - The polarity of the port to add.
//   next - The scope to add it to.
//
// Results:
//   The 'ports' scope with the added get port.
//
// Side effects:
//   Sets ports to a scope including the given port and next scope.

static Ports* AddPort(Ports* ports, Name name, FblcTypeId type, FblcPolarity polarity, Ports* next)
{
  ports->name = name;
  ports->type = type;
  ports->polarity = polarity;
  ports->next = next;
  return ports;
}

// ResolvePort --
//
//   Look up the type of a port in scope.
//
// Inputs:
//   ports - The scope to look up the port in.
//   name - The name of the port.
//   polarity - The polarity of the port.
//   port_id - Port id out parameter.
//
// Results:
//   The type of the port in scope, or UNRESOLVED_ID if there is no port with
//   that name and polarity in scope.
//
// Side effects:
//   Sets port_id to the id of the resolved port.
static FblcTypeId ResolvePort(Ports* ports, LocName* name, FblcPolarity polarity, FblcPortId* port_id)
{
  for (size_t i = 0; ports != NULL; ++i) {
    if (NamesEqual(ports->name, name->name)) {
      if (ports->polarity == polarity) {
        *port_id = i;
        return ports->type;
      } else {
        return UNRESOLVED_ID;
      }
    }
    ports = ports->next;
  }
  return UNRESOLVED_ID;
}

// CheckArgs --
//
//   Check that the arguments to a struct or function are well typed, of the
//   proper count, and have the correct types.
//
// Inputs:
//   env - The program environment.
//   vars - The types of local variables in scope.
//   fieldc - The number of arguments expected by the struct or function.
//   fieldv - The fieldc fields from the struct or function declaration.
//   argc - The number of arguments passed to the struct or function.
//   argv - The argc args passed to the struct or function.
//   func - A descriptive name and location of the function being applied, for
//          use in error messages.
//
// Results:
//   The value true if the arguments have the right type, false otherwise.
//
// Side effects:
//   LocName ids within the expressions are resolved.
//   If the arguments do not have the right type, prints a message on standard
//   error describing what's wrong.

static bool CheckArgs(
    Env* env, Vars* vars, int fieldc, Field* fieldv,
    int argc, Expr** argv, LocName* func)
{
  if (fieldc != argc) {
    ReportError("Wrong number of arguments to %s. Expected %d, "
        "but got %d.\n", func->loc, func->name, fieldc, argc);
    return false;
  }

  for (int i = 0; i < fieldc; i++) {
    FblcTypeId arg_type_id = CheckExpr(env, vars, argv[i]);
    if (arg_type_id == UNRESOLVED_ID) {
      return false;
    }
    TypeDecl* arg_type = (TypeDecl*)env->declv[arg_type_id];
    if (!NamesEqual(fieldv[i].type.name, arg_type->name.name)) {
      ReportError("Expected type %s, but found %s.\n",
          ExprLoc(argv[i]), fieldv[i].type.name, arg_type->name.name);
      return false;
    }
  }
  return true;
}

// CheckExpr --
//
//   Verify the given expression is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   env - The program environment.
//   vars - The names and types of the variables in scope.
//   expr - The expression to verify.
//
// Result:
//   The type of the expression, or UNRESOLVED_ID if the expression is not well formed
//   and well typed.
//
// Side effects:
//   LocName ids within the expression are resolved.
//   If the expression is not well formed and well typed, an error message is
//   printed to standard error describing the problem.

static FblcTypeId CheckExpr(Env* env, Vars* vars, Expr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      VarExpr* var_expr = (VarExpr*)expr;
      FblcTypeId type = ResolveVar(vars, &var_expr->name, &var_expr->x.var);
      if (type == UNRESOLVED_ID) {
        ReportError("Variable '%s' not in scope.\n",
            var_expr->name.loc, var_expr->name.name);
        return UNRESOLVED_ID;
      }
      return type;
    }

    case FBLC_APP_EXPR: {
      AppExpr* app_expr = (AppExpr*)expr;
      Decl* decl = NULL;
      for (size_t i = 0; i < env->declc; ++i) {
        if (NamesEqual(app_expr->func.name, DeclName(env->declv[i])->name)) {
          app_expr->x.func = i;
          decl = env->declv[i];
        }
      }
      if (decl == NULL) {
        ReportError("Declaration for '%s' not found.\n", app_expr->func.loc, app_expr->func.name);
        return UNRESOLVED_ID;
      }

      switch (decl->tag) {
        case FBLC_STRUCT_DECL: {
          TypeDecl* type = (TypeDecl*)decl;
          if (!CheckArgs(env, vars, type->fieldc, type->fields,
                app_expr->x.argc, (Expr**)app_expr->x.argv, &(app_expr->func))) {
            return UNRESOLVED_ID;
          }
          return app_expr->x.func;
        }

        case FBLC_UNION_DECL: {
          ReportError("Cannot do application on union type %s.\n",
              app_expr->func.loc, app_expr->func.name);
          return UNRESOLVED_ID;
        }

        case FBLC_FUNC_DECL: {
          FuncDecl* func = (FuncDecl*)decl;
          if (!CheckArgs(env, vars, func->argc, func->args,
                app_expr->x.argc, (Expr**)app_expr->x.argv, &(app_expr->func))) {
            return UNRESOLVED_ID;
          }
          return LookupType(env, func->return_type.name);
        }

        case FBLC_PROC_DECL: {
          ReportError("Cannot do application on a process %s.\n",
              app_expr->func.loc, app_expr->func.name);
          return UNRESOLVED_ID;
        }

        default:
          assert(false && "Invalid decl tag");
          return UNRESOLVED_ID;
      }
    }

    case FBLC_ACCESS_EXPR: {
      AccessExpr* access_expr = (AccessExpr*)expr;
      FblcTypeId type_id = CheckExpr(env, vars, (Expr*)access_expr->x.object);
      if (type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }
      TypeDecl* type = (TypeDecl*)env->declv[type_id];

      for (int i = 0; i < type->fieldc; i++) {
        if (NamesEqual(type->fields[i].name.name, access_expr->field.name)) {
          access_expr->x.field = i;
          return LookupType(env, type->fields[i].type.name);
        }
      }
      ReportError("'%s' is not a field of the type '%s'.\n",
          access_expr->field.loc, access_expr->field.name, type->name.name);
      return UNRESOLVED_ID;
    }

    case FBLC_UNION_EXPR: {
      UnionExpr* union_expr = (UnionExpr*)expr;
      union_expr->x.type = LookupType(env, union_expr->type.name);
      if (union_expr->x.type == UNRESOLVED_ID) {
        ReportError("Type %s not found.\n", union_expr->type.loc, union_expr->type.name);
        return UNRESOLVED_ID;
      }
      TypeDecl* type = (TypeDecl*)env->declv[union_expr->x.type];

      if (type->tag != FBLC_UNION_DECL) {
        ReportError("Type %s is not a union type.\n",
            union_expr->type.loc, union_expr->type.name);
        return UNRESOLVED_ID;
      }

      FblcTypeId arg_type_id = CheckExpr(env, vars, (Expr*)union_expr->x.body);
      if (arg_type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }
      TypeDecl* arg_type = (TypeDecl*)env->declv[arg_type_id];

      for (int i = 0; i < type->fieldc; i++) {
        if (NamesEqual(type->fields[i].name.name, union_expr->field.name)) {
          if (!NamesEqual(type->fields[i].type.name, arg_type->name.name)) {
            ReportError("Expected type '%s', but found type '%s'.\n",
                ExprLoc((Expr*)union_expr->x.body),
                type->fields[i].type.name, arg_type);
            return UNRESOLVED_ID;
          }
          union_expr->x.field = i;
          return union_expr->x.type;
        }
      }
      ReportError("Type '%s' has no field '%s'.\n", union_expr->field.loc,
          union_expr->type.name, union_expr->field.name);
      return UNRESOLVED_ID;
    }

    case FBLC_LET_EXPR: {
      LetExpr* let_expr = (LetExpr*)expr;
      FblcTypeId declared_type_id = LookupType(env, let_expr->type.name);
      if (declared_type_id == UNRESOLVED_ID) {
        ReportError("Type '%s' not declared.\n",
            let_expr->type.loc, let_expr->type.name);
        return UNRESOLVED_ID;
      }

      FblcVarId dummy;
      if (ResolveVar(vars, &let_expr->name, &dummy) != UNRESOLVED_ID) {
        ReportError("Variable %s already defined.\n",
            let_expr->name.loc, let_expr->name.name);
        return UNRESOLVED_ID;
      }

      FblcTypeId actual_type_id = CheckExpr(env, vars, (Expr*)let_expr->x.def);
      if (actual_type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }

      if (declared_type_id != actual_type_id) {
        TypeDecl* actual_type = (TypeDecl*)env->declv[actual_type_id];
        ReportError("Expected type %s, but found expression of type %s.\n",
            ExprLoc((Expr*)let_expr->x.def), let_expr->type.name, actual_type->name.name);
        return UNRESOLVED_ID;
      }

      Vars nvars;
      AddVar(&nvars, let_expr->name.name, actual_type_id, vars);
      return CheckExpr(env, &nvars, (Expr*)let_expr->x.body);
    }

    case FBLC_COND_EXPR: {
      CondExpr* cond_expr = (CondExpr*)expr;
      FblcTypeId type_id = CheckExpr(env, vars, (Expr*)cond_expr->x.select);
      if (type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }
      TypeDecl* type = (TypeDecl*)env->declv[type_id];

      if (type->tag != FBLC_UNION_DECL) {
        ReportError("The condition has type %s, "
            "which is not a union type.\n", ExprLoc(expr), type->name.name);
        return UNRESOLVED_ID;
      }

      if (type->fieldc != cond_expr->x.argc) {
        ReportError("Wrong number of arguments to condition. Expected %d, "
            "but found %d.\n", ExprLoc(expr), type->fieldc, cond_expr->x.argc);
        return UNRESOLVED_ID;
      }

      FblcTypeId result_type_id = UNRESOLVED_ID;
      for (int i = 0; i < cond_expr->x.argc; i++) {
        FblcTypeId arg_type_id = CheckExpr(env, vars, (Expr*)cond_expr->x.argv[i]);
        if (arg_type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }

        if (i != 0 && result_type_id != arg_type_id) {
          TypeDecl* arg_type = (TypeDecl*)env->declv[arg_type_id];
          TypeDecl* result_type = (TypeDecl*)env->declv[result_type_id];
          ReportError("Expected expression of type %s, "
              "but found expression of type %s.\n",
              ExprLoc((Expr*)cond_expr->x.argv[i]), result_type->name.name, arg_type->name.name);
          return UNRESOLVED_ID;
        }
        result_type_id = arg_type_id;
      }
      assert(result_type_id != UNRESOLVED_ID);
      return result_type_id;
    }

    default: {
      assert(false && "Unknown expression type.");
      return UNRESOLVED_ID;
    }
  }
}

// CheckActn --
//
//   Verify the given action is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   env - The program environment.
//   vars - The names and types of the variables in scope.
//   ports - The names, types, and polarities of the ports in scope.
//   actn - The action to verify.
//
// Result:
//   The type of the action or UNRESOLVED_ID if the expression is not well formed and
//   well typed.
//
// Side effects:
//   LocName ids within the action are resolved.
//   If the expression is not well formed and well typed, an error message is
//   printed to standard error describing the problem.

static FblcTypeId CheckActn(Env* env, Vars* vars, Ports* ports, Actn* actn)
{
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      EvalActn* eval_actn = (EvalActn*)actn;
      return CheckExpr(env, vars, (Expr*)eval_actn->x.expr);
    }

    case FBLC_GET_ACTN: {
      GetActn* get_actn = (GetActn*)actn;
      FblcTypeId type_id = ResolvePort(ports, &get_actn->port, FBLC_GET_POLARITY, &get_actn->x.port);
      if (type_id == UNRESOLVED_ID) {
        ReportError("'%s' is not a valid get port.\n", get_actn->port.loc, get_actn->port.name);
        return UNRESOLVED_ID;
      }
      return type_id;
    }

    case FBLC_PUT_ACTN: {
      PutActn* put_actn = (PutActn*)actn;
      FblcTypeId port_type_id = ResolvePort(ports, &put_actn->port, FBLC_PUT_POLARITY, &put_actn->x.port);
      if (port_type_id == UNRESOLVED_ID) {
        ReportError("'%s' is not a valid put port.\n", put_actn->port.loc, put_actn->port.name);
        return UNRESOLVED_ID;
      }

      FblcTypeId arg_type_id = CheckExpr(env, vars, (Expr*)put_actn->x.arg);
      if (arg_type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }
      if (port_type_id != arg_type_id) {
        TypeDecl* port_type = (TypeDecl*)env->declv[port_type_id];
        TypeDecl* arg_type = (TypeDecl*)env->declv[arg_type_id];
        ReportError("Expected type %s, but found %s.\n",
            ExprLoc((Expr*)put_actn->x.arg), port_type->name.name, arg_type->name.name);
        return UNRESOLVED_ID;
      }
      return arg_type_id;
    }

    case FBLC_CALL_ACTN: {
      CallActn* call_actn = (CallActn*)actn;
      ProcDecl* proc = NULL;
      for (size_t i = 0; i < env->declc; ++i) {
        Decl* decl = env->declv[i];
        if (decl->tag == FBLC_PROC_DECL && NamesEqual(DeclName(decl)->name, call_actn->proc.name)) {
          call_actn->x.proc = i;
          proc = (ProcDecl*)decl;
        }
      }
      if (proc == NULL) {
        ReportError("'%s' is not a proc.\n", call_actn->proc.loc, call_actn->proc.name);
        return UNRESOLVED_ID;
      }

      if (call_actn->x.portc  != proc->portc) {
        ReportError("Wrong number of port arguments to %s. Expected %d, "
            "but got %d.\n", call_actn->proc.loc, call_actn->proc.name,
            proc->portc, call_actn->x.portc);
        return UNRESOLVED_ID;
      }

      for (int i = 0; i < proc->portc; i++) {
        bool isput = (proc->portv[i].polarity == FBLC_PUT_POLARITY);
        FblcTypeId port_type_id = UNRESOLVED_ID;
        if (isput) {
          port_type_id = ResolvePort(ports, call_actn->ports + i, FBLC_PUT_POLARITY, call_actn->x.portv + i);
        } else {
          port_type_id = ResolvePort(ports, call_actn->ports + i, FBLC_GET_POLARITY, call_actn->x.portv + i);
        }
        if (port_type_id == UNRESOLVED_ID) {
          ReportError("'%s' is not a valid %s port.\n",
              call_actn->ports[i].loc,
              call_actn->ports[i].name,
              isput ? "put" : "get");
          return UNRESOLVED_ID;
        }

        TypeDecl* port_type = (TypeDecl*)env->declv[port_type_id];
        if (!NamesEqual(proc->ports[i].type.name, port_type->name.name)) {
          ReportError("Expected port type %s, but found %s.\n",
              call_actn->ports[i].loc, proc->ports[i].type.name, port_type->name);
          return UNRESOLVED_ID;
        }
      }

      if (!CheckArgs(env, vars, proc->argc, proc->args, call_actn->x.argc,
            (Expr**)call_actn->x.argv, &(call_actn->proc))) {
        return UNRESOLVED_ID;
      }
      return LookupType(env, proc->return_type.name);
    }

    case FBLC_LINK_ACTN: {
      // TODO: Test that we verify the link type resolves?
      LinkActn* link_actn = (LinkActn*)actn;
      link_actn->x.type = LookupType(env, link_actn->type.name);
      if (link_actn->x.type == UNRESOLVED_ID) {
        ReportError("Type '%s' not declared.\n",
            link_actn->type.loc, link_actn->type.name);
        return UNRESOLVED_ID;
      }

      Ports getport;
      Ports putport;
      AddPort(&getport, link_actn->getname.name, link_actn->x.type, FBLC_GET_POLARITY, ports);
      AddPort(&putport, link_actn->putname.name, link_actn->x.type, FBLC_PUT_POLARITY, &getport);
      return CheckActn(env, vars, &putport, (Actn*)link_actn->x.body);
    }

    case FBLC_EXEC_ACTN: {
      ExecActn* exec_actn = (ExecActn*)actn;
      Vars vars_data[exec_actn->x.execc];
      Vars* nvars = vars;
      for (int i = 0; i < exec_actn->x.execc; i++) {
        Field* var = exec_actn->vars + i;
        Actn* exec = (Actn*)exec_actn->x.execv[i];
        FblcTypeId type_id = CheckActn(env, vars, ports, exec);
        if (type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }

        // TODO: Test that we verify actual_type is not null?
        FblcTypeId actual_type_id = LookupType(env, var->type.name);
        if (actual_type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }
        if (actual_type_id != type_id) {
          TypeDecl* type = (TypeDecl*)env->declv[type_id];
          ReportError("Expected type %s, but found %s.\n", var->type.loc, var->type.name, type->name.name);
          return UNRESOLVED_ID;
        }

        nvars = AddVar(vars_data+i, var->name.name, type_id, nvars);
      }
      return CheckActn(env, nvars, ports, (Actn*)exec_actn->x.body);
    }

    case FBLC_COND_ACTN: {
      CondActn* cond_actn = (CondActn*)actn;
      FblcTypeId type_id = CheckExpr(env, vars, (Expr*)cond_actn->x.select);
      if (type_id == UNRESOLVED_ID) {
        return UNRESOLVED_ID;
      }
      TypeDecl* type = (TypeDecl*)env->declv[type_id];

      if (type->tag != FBLC_UNION_DECL) {
        ReportError("The condition has type %s, "
            "which is not a union type.\n", ActnLoc(actn), type->name.name);
        return UNRESOLVED_ID;
      }

      if (type->fieldc != cond_actn->x.argc) {
        ReportError("Wrong number of arguments to condition. Expected %d, "
            "but found %d.\n", ActnLoc(actn), type->fieldc, cond_actn->x.argc);
        return UNRESOLVED_ID;
      }

      // TODO: Verify that no two branches of the condition refer to the same
      // port. Do we still want to do this?
      FblcTypeId result_type_id = UNRESOLVED_ID;
      for (int i = 0; i < cond_actn->x.argc; i++) {
        FblcTypeId arg_type_id = CheckActn(env, vars, ports, (Actn*)cond_actn->x.argv[i]);
        if (arg_type_id == UNRESOLVED_ID) {
          return UNRESOLVED_ID;
        }

        if (i > 0 && result_type_id != arg_type_id) {
          TypeDecl* result_type = (TypeDecl*)env->declv[result_type_id];
          TypeDecl* arg_type = (TypeDecl*)env->declv[arg_type_id];
          ReportError("Expected process of type %s, "
              "but found process of type %s.\n",
              ActnLoc((Actn*)cond_actn->x.argv[i]), result_type->name.name, arg_type->name.name);
          return UNRESOLVED_ID;
        }
        result_type_id = arg_type_id;
      }
      assert(result_type_id != UNRESOLVED_ID);
      return result_type_id;
    }
  }
  assert(false && "UNREACHABLE");
  return UNRESOLVED_ID;
}

// CheckFields --
//
//   Verify the given fields have valid types and unique names. This is used
//   to check fields in a type declaration and arguments in a function or
//   process declaration.
//
// Inputs:
//   env - The program environment.
//   fieldc - The number of fields to verify.
//   fieldv - The fields to verify.
//   kind - The type of field for error reporting purposes, e.g. "field" or
//          "arg".
//
// Results:
//   LocName ids within the body of the declaration are resolved.
//   Returns true if the fields have valid types and unique names, false
//   otherwise.
//
// Side effects:
//   LocName ids within the fields are resolved.
//   If the fields don't have valid types or don't have unique names, a
//   message is printed to standard error describing the problem.

static bool CheckFields(Env* env, int fieldc, FblcTypeId* fieldv, Field* fields, const char* kind)
{
  // Verify the type for each field exists.
  for (int i = 0; i < fieldc; i++) {
    fieldv[i] = LookupType(env, fields[i].type.name);
    if (fieldv[i] == UNRESOLVED_ID) {
      ReportError("Type '%s' not found.\n", fields[i].type.loc, fields[i].type.name);
      return false;
    }
  }

  // Verify fields have unique names.
  for (int i = 0; i < fieldc; i++) {
    for (int j = i+1; j < fieldc; j++) {
      if (NamesEqual(fields[i].name.name, fields[j].name.name)) {
        ReportError("Multiple %ss named '%s'.\n",
            fields[j].name.loc, kind, fields[j].name.name);
        return false;
      }
    }
  }
  return true;
}

// CheckPorts --
//
//   Verify the given ports have valid types and unique names.
//
// Inputs:
//   env - The program environment.
//   portc - The number of ports to verify.
//   portv - The ports to verify.
//
// Results:
//   Returns true if the ports have valid types and unique names, false
//   otherwise.
//
// Side effects:
//   LocName ids within the ports are resolved.
//   If the ports don't have valid types or don't have unique names, a
//   message is printed to standard error describing the problem.

static bool CheckPorts(Env* env, int portc, FblcPort* portv, Port* ports)
{
  // Verify the type for each port exists.
  for (int i = 0; i < portc; i++) {
    portv[i].type = LookupType(env, ports[i].type.name);
    if (portv[i].type == UNRESOLVED_ID) {
      ReportError("Type '%s' not found.\n", ports[i].type.loc, ports[i].type.name);
      return false;
    }
  }

  // Verify ports have unique names.
  for (int i = 0; i < portc; i++) {
    for (int j = i+1; j < portc; j++) {
      if (NamesEqual(ports[i].name.name, ports[j].name.name)) {
        ReportError("Multiple ports named '%s'.\n",
            ports[j].name.loc, ports[j].name.name);
        return false;
      }
    }
  }
  return true;
}

// CheckType --
//
//   Verify the given type declaration is well formed in the given
//   environment.
//
// Inputs:
//   env - The program environment.
//   type - The type declaration to check.
//
// Results:
//   Returns true if the type declaration is well formed, false otherwise.
//
// Side effects:
//   LocName ids within the body of the declaration are resolved.
//   If the type declaration is not well formed, prints a message to standard
//   error describing the problem.

static bool CheckType(Env* env, TypeDecl* type)
{
  if (type->tag == FBLC_UNION_DECL && type->fieldc == 0) {
    ReportError("A union type must have at least one field.\n",
        type->name.loc);
    return false;
  }
  return CheckFields(env, type->fieldc, type->fieldv, type->fields, "field");
}

// CheckFunc --
//
//   Verify the given function declaration is well formed in the given
//   environment.
//
// Inputs:
//   env - The program environment.
//   func - The function declaration to check.
//
// Results:
//   Returns true if the function declaration is well formed, false otherwise.
//
// Side effects:
//   LocName ids within the body of declaration are resolved.
//   If the function declaration is not well formed, prints a message to
//   standard error describing the problem.

static bool CheckFunc(Env* env, FuncDecl* func)
{
  // Check the arguments.
  if (!CheckFields(env, func->argc, func->argv, func->args, "arg")) {
    return false;
  }

  // Check the return type.
  func->return_type_id = LookupType(env, func->return_type.name);
  if (func->return_type_id == UNRESOLVED_ID) {
    ReportError("Type '%s' not found.\n", func->return_type.loc, func->return_type.name);
    return false;
  }

  // Check the body.
  Vars nvars[func->argc];
  Vars* vars = NULL;
  for (int i = 0; i < func->argc; i++) {
    // TODO: Add test that we verify the argument types are valid?
    FblcTypeId arg_type_id = LookupType(env, func->args[i].type.name);
    if (arg_type_id == UNRESOLVED_ID) {
      return false;
    }
    vars = AddVar(nvars+i, func->args[i].name.name, arg_type_id, vars);
  }
  FblcTypeId body_type_id = CheckExpr(env, vars, func->body);
  if (body_type_id == UNRESOLVED_ID) {
    return false;
  }
  if (func->return_type_id != body_type_id) {
    TypeDecl* body_type = (TypeDecl*)env->declv[body_type_id];
    ReportError("Type mismatch. Expected %s, but found %s.\n",
        ExprLoc(func->body), func->return_type.name, body_type);
    return false;
  }
  return true;
}

// CheckProc --
//
//   Verify the given process declaration is well formed in the given
//   environment.
//
// Inputs:
//   env - The program environment.
//   proc - The process declaration to check.
//
// Results:
//   Returns true if the process declaration is well formed, false otherwise.
//
// Side effects:
//   LocName ids within the body of declaration are resolved.
//   If the process declaration is not well formed, prints a message to
//   standard error describing the problem.

static bool CheckProc(Env* env, ProcDecl* proc)
{
  // Check the ports.
  if (!CheckPorts(env, proc->portc, proc->portv, proc->ports)) {
    return false;
  }

  // Check the arguments.
  if (!CheckFields(env, proc->argc, proc->argv, proc->args, "arg")) {
    return false;
  }

  // Check the return type.
  proc->return_type_id = LookupType(env, proc->return_type.name);
  if (proc->return_type_id == UNRESOLVED_ID) {
    ReportError("Type '%s' not found.\n", proc->return_type.loc, proc->return_type.name);
    return false;
  }

  // Check the body.
  Vars vars_data[proc->argc];
  Ports ports_data[proc->portc];
  Vars* nvar = vars_data;
  Ports* nport = ports_data;

  Vars* vars = NULL;
  for (int i = 0; i < proc->argc; i++) {
    // TODO: Add test that we check we managed to resolve the arg types?
    FblcTypeId arg_type_id = LookupType(env, proc->args[i].type.name);
    if (arg_type_id == UNRESOLVED_ID) {
      return false;
    }
    vars = AddVar(nvar++, proc->args[i].name.name, arg_type_id, vars);
  }

  Ports* ports = NULL;
  for (int i = 0; i < proc->portc; i++) {
    // TODO: Add tests that we properly resolved the port types?
    FblcTypeId port_type_id = LookupType(env, proc->ports[i].type.name);
    if (port_type_id == UNRESOLVED_ID) {
      return false;
    }
    switch (proc->portv[i].polarity) {
      case FBLC_GET_POLARITY:
        ports = AddPort(nport++, proc->ports[i].name.name, port_type_id, FBLC_GET_POLARITY, ports);
        break;

      case FBLC_PUT_POLARITY:
        ports = AddPort(nport++, proc->ports[i].name.name, port_type_id, FBLC_PUT_POLARITY, ports);
        break;
    }
  }

  FblcTypeId body_type_id = CheckActn(env, vars, ports, proc->body);
  if (body_type_id == UNRESOLVED_ID) {
    return false;
  }
  if (proc->return_type_id != body_type_id) {
    TypeDecl* body_type = (TypeDecl*)env->declv[body_type_id];
    ReportError("Type mismatch. Expected %s, but found %s.\n",
        ActnLoc(proc->body), proc->return_type.name, body_type->name.name);
    return false;
  }
  return true;
}

// CheckProgram --
//
//   Check that the given program environment describes a well formed and well
//   typed program.
//
// Inputs:
//   env - The program environment to check.
//
// Results:
//   The value true if the program environment is well formed and well typed,
//   false otherwise.
//
// Side effects:
//   LocName ids within the body of declarations are resolved.
//   If the program environment is not well formed, an error message is
//   printed to standard error describing the problem with the program
//   environment.

bool CheckProgram(Env* env)
{
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL:
      case FBLC_UNION_DECL:
        if (!CheckType(env, (TypeDecl*)decl)) {
          return false;
        }
        break;

      case FBLC_FUNC_DECL:
        if (!CheckFunc(env, (FuncDecl*)decl)) {
          return false;
        }
        break;

      case FBLC_PROC_DECL:
        if (!CheckProc(env, (ProcDecl*)decl)) {
          return false;
        }
        break;

      default:
        assert(false && "Invalid decl type");
        return false;
    }

    // Verify the declaration does not have the same name as one we have
    // already seen.
    for (size_t j = 0; j < i; ++j) {
      if (NamesEqual(DeclName(env->declv[i])->name, DeclName(env->declv[j])->name)) {
        ReportError("Multiple declarations for %s.\n",
            DeclName(env->declv[i])->loc, DeclName(env->declv[i])->name);
        return false;
      }
    }
  }
  return true;
}
