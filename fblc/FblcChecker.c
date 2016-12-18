// FblcChecker.c --
//
//   This file implements routines for checking an Fblc program is well formed
//   and well typed.

#include "FblcInternal.h"

// The following Vars structure describes a mapping from variable names to
// their types.
//
// We always allocate the Vars mapping on the stack. This means it is not safe
// to return or otherwise capture a Vars* outside of the frame where it is
// allocated.

typedef struct Vars {
  FblcName name;
  FblcName type;
  struct Vars* next;
} Vars;

static Vars* AddVar(Vars* vars, FblcName name, FblcName type, Vars* next);
static FblcName LookupVar(Vars* vars, FblcName name);
static bool CheckArgs(
    const FblcEnv* env, Vars* vars, int fieldc, FblcField* fieldv,
    int argc, FblcExpr* const* argv, const FblcLocName* func);
static FblcName CheckExpr(
    const FblcEnv* env, Vars* vars, const FblcExpr* expr);
static FblcName CheckActn(const FblcEnv* env, Vars* vars, Vars* gets,
    Vars* puts, FblcActn* actn);
static bool CheckFields(
    const FblcEnv* env, int fieldc, FblcField* fieldv, const char* kind);
static bool CheckPorts(const FblcEnv* env, int portc, FblcPort* portv);
static bool CheckType(const FblcEnv* env, FblcType* type);
static bool CheckFunc(const FblcEnv* env, FblcFunc* func);
static bool CheckProc(const FblcEnv* env, FblcProc* proc);

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

static Vars* AddVar(Vars* vars, FblcName name, FblcName type, Vars* next)
{
  vars->name = name;
  vars->type = type;
  vars->next = next;
  return vars;
}

// LookupVar --
//
//   Look up the type of a variable in scope.
//
// Inputs:
//   vars - The scope to look up the variable in.
//   name - The name of the variable.
//
// Results:
//   The type of the variable in scope, or NULL if there is no variable with
//   that name in scope.
//
// Side effects:
//   None.

static FblcName LookupVar(Vars* vars, FblcName name)
{
  while (vars != NULL) {
    if (FblcNamesEqual(vars->name, name)) {
      return vars->type;
    }
    vars = vars->next;
  }
  return NULL;
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
//   If the arguments do not have the right type, prints a message on standard
//   error describing what's wrong.

static bool CheckArgs(
    const FblcEnv* env, Vars* vars, int fieldc, FblcField* fieldv,
    int argc, FblcExpr* const* argv, const FblcLocName* func)
{
  if (fieldc != argc) {
    FblcReportError("Wrong number of arguments to %s. Expected %d, "
        "but got %d.\n", func->loc, func->name, fieldc, argc);
    return false;
  }

  for (int i = 0; i < fieldc; i++) {
    FblcName arg_type = CheckExpr(env, vars, argv[i]);
    if (arg_type == NULL) {
      return false;
    }
    if (!FblcNamesEqual(fieldv[i].type.name, arg_type)) {
      FblcReportError("Expected type %s, but found %s.\n",
          argv[i]->loc, fieldv[i].type.name, arg_type);
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
//   The type of the expression, or NULL if the expression is not well formed
//   and well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to standard error describing the problem.

static FblcName CheckExpr(
    const FblcEnv* env, Vars* vars, const FblcExpr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      FblcName type = LookupVar(vars, var_expr->name.name);
      if (type == NULL) {
        FblcReportError("Variable '%s' not in scope.\n",
            var_expr->name.loc, var_expr->name.name);
        return NULL;
      }
      return type;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      FblcType* type = FblcLookupType(env, app_expr->func.name);
      if (type != NULL) {
        if (type->kind != FBLC_KIND_STRUCT) {
          FblcReportError("Cannot do application on non-struct type %s.\n",
              app_expr->func.loc, app_expr->func.name);
          return NULL;
        }
        if (!CheckArgs(env, vars, type->fieldc, type->fieldv,
              app_expr->argc, app_expr->argv, &(app_expr->func))) {
          return NULL;
        }
        return type->name.name;
      }

      FblcFunc* func = FblcLookupFunc(env, app_expr->func.name);
      if (func != NULL) {
        if (!CheckArgs(env, vars, func->argc, func->argv,
              app_expr->argc, app_expr->argv, &(app_expr->func))) {
          return NULL;
        }
        return func->return_type.name;
      }

      FblcReportError("'%s' is not a type or function.\n",
          app_expr->loc, app_expr->func.name);
      return NULL;
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcName typename = CheckExpr(env, vars, access_expr->object);
      if (typename == NULL) {
        return NULL;
      }

      FblcType* type = FblcLookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");
      for (int i = 0; i < type->fieldc; i++) {
        if (FblcNamesEqual(type->fieldv[i].name.name,
              access_expr->field.name)) {
          return type->fieldv[i].type.name;
        }
      }
      FblcReportError("'%s' is not a field of the type '%s'.\n",
          access_expr->field.loc, access_expr->field.name, typename);
      return NULL;
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      FblcType* type = FblcLookupType(env, union_expr->type.name);
      if (type == NULL) {
        FblcReportError("Type %s not found.\n",
            union_expr->type.loc, union_expr->type.name);
        return NULL;
      }

      if (type->kind != FBLC_KIND_UNION) {
        FblcReportError("Type %s is not a union type.\n",
            union_expr->loc, union_expr->type.name);
        return NULL;
      }

      FblcName arg_type = CheckExpr(env, vars, union_expr->value);
      if (arg_type == NULL) {
        return NULL;
      }

      for (int i = 0; i < type->fieldc; i++) {
        if (FblcNamesEqual(type->fieldv[i].name.name,
              union_expr->field.name)) {
          if (!FblcNamesEqual(type->fieldv[i].type.name, arg_type)) {
            FblcReportError("Expected type '%s', but found type '%s'.\n",
                union_expr->value->loc,
                type->fieldv[i].type.name, arg_type);
            return NULL;
          }
          return type->name.name;
        }
      }
      FblcReportError("Type '%s' has no field '%s'.\n", union_expr->field.loc,
          union_expr->type.name, union_expr->field.name);
      return NULL;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      if (FblcLookupType(env, let_expr->type.name) == NULL) {
        FblcReportError("Type '%s' not declared.\n",
            let_expr->type.loc, let_expr->type.name);
        return NULL;
      }

      if (LookupVar(vars, let_expr->name.name) != NULL) {
        FblcReportError("Variable %s already defined.\n",
            let_expr->name.loc, let_expr->name.name);
        return NULL;
      }

      FblcName type = CheckExpr(env, vars, let_expr->def);
      if (type == NULL) {
        return NULL;
      }

      if (!FblcNamesEqual(let_expr->type.name, type)) {
        FblcReportError("Expected type %s, but found expression of type %s.\n",
            let_expr->def->loc, let_expr->type.name, type);
        return NULL;
      }

      Vars nvars;
      AddVar(&nvars, let_expr->name.name, type, vars);
      return CheckExpr(env, &nvars, let_expr->body);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      FblcName typename = CheckExpr(env, vars, cond_expr->select);
      if (typename == NULL) {
        return NULL;
      }

      FblcType* type = FblcLookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");

      if (type->kind != FBLC_KIND_UNION) {
        FblcReportError("The condition has type %s, "
            "which is not a union type.\n", cond_expr->loc, typename);
        return NULL;
      }

      if (type->fieldc != cond_expr->argc) {
        FblcReportError("Wrong number of arguments to condition. Expected %d, "
            "but found %d.\n", cond_expr->loc, type->fieldc, cond_expr->argc);
        return NULL;
      }

      FblcName result_type = NULL;
      for (int i = 0; i < cond_expr->argc; i++) {
        FblcName arg_type = CheckExpr(env, vars, cond_expr->argv[i]);
        if (arg_type == NULL) {
          return NULL;
        }

        if (result_type != NULL && !FblcNamesEqual(result_type, arg_type)) {
          FblcReportError("Expected expression of type %s, "
              "but found expression of type %s.\n",
              cond_expr->argv[i]->loc, result_type, arg_type);
          return NULL;
        }
        result_type = arg_type;
      }
      assert(result_type != NULL);
      return result_type;
    }

    default: {
      assert(false && "Unknown expression type.");
      return NULL;
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
//   gets - The names and types of the get ports in scope.
//   puts - The names and types of the put ports in scope.
//   actn - The action to verify.
//
// Result:
//   The type of the action or NULL if the expression is not well formed and
//   well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to standard error describing the problem.

static FblcName CheckActn(const FblcEnv* env, Vars* vars, Vars* gets,
    Vars* puts, FblcActn* actn)
{
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return CheckExpr(env, vars, eval_actn->expr);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      FblcName type = LookupVar(gets, get_actn->port.name);
      if (type == NULL) {
        FblcReportError("'%s' is not a valid get port.\n", get_actn->loc,
            get_actn->port.name);
        return NULL;
      }
      return type;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      FblcName port_type = LookupVar(puts, put_actn->port.name);
      if (port_type == NULL) {
        FblcReportError("'%s' is not a valid put port.\n", put_actn->loc,
            put_actn->port.name);
        return NULL;
      }

      FblcName arg_type = CheckExpr(env, vars, put_actn->expr);
      if (arg_type == NULL) {
        return NULL;
      }
      if (!FblcNamesEqual(port_type, arg_type)) {
        FblcReportError("Expected type %s, but found %s.\n",
            put_actn->expr->loc, port_type, arg_type);
        return NULL;
      }
      return arg_type;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      FblcProc* proc = FblcLookupProc(env, call_actn->proc.name);
      if (proc == NULL) {
        FblcReportError("'%s' is not a proc.\n",
            call_actn->loc, call_actn->proc.name);
        return NULL;
      }

      if (call_actn->portc  != proc->portc) {
        FblcReportError("Wrong number of port arguments to %s. Expected %d, "
            "but got %d.\n", call_actn->loc, call_actn->proc.name,
            proc->portc, call_actn->portc);
        return NULL;
      }

      for (int i = 0; i < proc->portc; i++) {
        bool isput = (proc->portv[i].polarity == FBLC_POLARITY_PUT);
        Vars* ports = isput ? puts : gets;
        FblcName port_type = LookupVar(ports, call_actn->ports[i].name);
        if (port_type == NULL) {
          FblcReportError("'%s' is not a valid %s port.\n",
              call_actn->ports[i].loc,
              call_actn->ports[i].name,
              isput ? "put" : "get");
          return NULL;
        }

        if (!FblcNamesEqual(proc->portv[i].type.name, port_type)) {
          FblcReportError("Expected port type %s, but found %s.\n",
              call_actn->ports[i].loc, proc->portv[i].type.name, port_type);
          return NULL;
        }
      }

      if (!CheckArgs(env, vars, proc->argc, proc->argv, call_actn->exprc,
            call_actn->exprs, &(call_actn->proc))) {
        return NULL;
      }
      return proc->return_type.name;
    }

    case FBLC_LINK_ACTN: {
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      FblcName type = link_actn->type.name;
      Vars ngets;
      Vars nputs;
      AddVar(&ngets, link_actn->getname.name, type, gets);
      AddVar(&nputs, link_actn->putname.name, type, puts);
      return CheckActn(env, vars, &ngets, &nputs, link_actn->body);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      Vars vars_data[exec_actn->execc];
      Vars* nvars = vars;
      for (int i = 0; i < exec_actn->execc; i++) {
        FblcExec* exec = &(exec_actn->execv[i]);
        FblcName type = CheckActn(env, vars, gets, puts, exec->actn);
        if (type == NULL) {
          return NULL;
        }

        if (!FblcNamesEqual(exec->var.type.name, type)) {
          FblcReportError("Expected type %s, but found %s.\n",
              exec->actn->loc, exec->var.type.name, type);
          return NULL;
        }
        nvars = AddVar(vars_data+i, exec->var.name.name, exec->var.type.name, nvars);
      }
      return CheckActn(env, nvars, gets, puts, exec_actn->body);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      FblcName typename = CheckExpr(env, vars, cond_actn->select);
      if (typename == NULL) {
        return NULL;
      }

      FblcType* type = FblcLookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");

      if (type->kind != FBLC_KIND_UNION) {
        FblcReportError("The condition has type %s, "
            "which is not a union type.\n", cond_actn->loc, typename);
        return NULL;
      }

      if (type->fieldc != cond_actn->argc) {
        FblcReportError("Wrong number of arguments to condition. Expected %d, "
            "but found %d.\n", cond_actn->loc, type->fieldc, cond_actn->argc);
        return NULL;
      }

      // TODO: Verify that no two branches of the condition refer to the same
      // port.
      FblcName result_type = NULL;
      for (int i = 0; i < cond_actn->argc; i++) {
        FblcName arg_type = CheckActn(
            env, vars, gets, puts, cond_actn->args[i]);
        if (arg_type == NULL) {
          return NULL;
        }

        if (result_type != NULL && !FblcNamesEqual(result_type, arg_type)) {
          FblcReportError("Expected process of type %s, "
              "but found process of type %s.\n",
              cond_actn->args[i]->loc, result_type, arg_type);
          return NULL;
        }
        result_type = arg_type;
      }
      assert(result_type != NULL);
      return result_type;
    }
  }
  assert(false && "UNREACHABLE");
  return NULL;
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
//   Returns true if the fields have valid types and unique names, false
//   otherwise.
//
// Side effects:
//   If the fields don't have valid types or don't have unique names, a
//   message is printed to standard error describing the problem.

static bool CheckFields(
    const FblcEnv* env, int fieldc, FblcField* fieldv, const char* kind)
{
  // Verify the type for each field exists.
  for (int i = 0; i < fieldc; i++) {
    if (FblcLookupType(env, fieldv[i].type.name) == NULL) {
      FblcReportError("Type '%s' not found.\n",
          fieldv[i].type.loc, fieldv[i].type.name);
      return false;
    }
  }

  // Verify fields have unique names.
  for (int i = 0; i < fieldc; i++) {
    for (int j = i+1; j < fieldc; j++) {
      if (FblcNamesEqual(fieldv[i].name.name, fieldv[j].name.name)) {
        FblcReportError("Multiple %ss named '%s'.\n",
            fieldv[j].name.loc, kind, fieldv[j].name.name);
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
//   If the ports don't have valid types or don't have unique names, a
//   message is printed to standard error describing the problem.

static bool CheckPorts(const FblcEnv* env, int portc, FblcPort* portv)
{
  // Verify the type for each port exists.
  for (int i = 0; i < portc; i++) {
    if (FblcLookupType(env, portv[i].type.name) == NULL) {
      FblcReportError("Type '%s' not found.\n",
          portv[i].type.loc, portv[i].type.name);
      return false;
    }
  }

  // Verify ports have unique names.
  for (int i = 0; i < portc; i++) {
    for (int j = i+1; j < portc; j++) {
      if (FblcNamesEqual(portv[i].name.name, portv[j].name.name)) {
        FblcReportError("Multiple ports named '%s'.\n",
            portv[j].name.loc, portv[j].name.name);
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
//   If the type declaration is not well formed, prints a message to standard
//   error describing the problem.

static bool CheckType(const FblcEnv* env, FblcType* type)
{
  if (type->kind == FBLC_KIND_UNION && type->fieldc == 0) {
    FblcReportError("A union type must have at least one field.\n",
        type->name.loc);
    return false;
  }
  return CheckFields(env, type->fieldc, type->fieldv, "field");
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
//   If the function declaration is not well formed, prints a message to
//   standard error describing the problem.

static bool CheckFunc(const FblcEnv* env, FblcFunc* func)
{
  // Check the arguments.
  if (!CheckFields(env, func->argc, func->argv, "arg")) {
    return false;
  }

  // Check the return type.
  if (FblcLookupType(env, func->return_type.name) == NULL) {
    FblcReportError("Type '%s' not found.\n",
        func->return_type.loc, func->return_type.name);
    return false;
  }

  // Check the body.
  Vars nvars[func->argc];
  Vars* vars = NULL;
  for (int i = 0; i < func->argc; i++) {
    vars = AddVar(
        nvars+i, func->argv[i].name.name, func->argv[i].type.name, vars);
  }
  FblcName body_type = CheckExpr(env, vars, func->body);
  if (body_type == NULL) {
    return false;
  }
  if (!FblcNamesEqual(func->return_type.name, body_type)) {
    FblcReportError("Type mismatch. Expected %s, but found %s.\n",
        func->body->loc, func->return_type.name, body_type);
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
//   If the process declaration is not well formed, prints a message to
//   standard error describing the problem.

static bool CheckProc(const FblcEnv* env, FblcProc* proc)
{
  // Check the ports.
  if (!CheckPorts(env, proc->portc, proc->portv)) {
    return false;
  }

  // Check the arguments.
  if (!CheckFields(env, proc->argc, proc->argv, "arg")) {
    return false;
  }

  // Check the return type.
  if (FblcLookupType(env, proc->return_type.name) == NULL) {
    FblcReportError("Type '%s' not found.\n",
        proc->return_type.loc, proc->return_type.name);
    return false;
  }

  // Check the body.
  Vars vars_data[proc->argc + proc->portc];
  Vars* nvar = vars_data;

  Vars* vars = NULL;
  for (int i = 0; i < proc->argc; i++) {
    vars = AddVar(
        nvar++, proc->argv[i].name.name, proc->argv[i].type.name, vars);
  }

  Vars* gets = NULL;
  Vars* puts = NULL;
  for (int i = 0; i < proc->portc; i++) {
    switch (proc->portv[i].polarity) {
      case FBLC_POLARITY_GET:
        gets = AddVar(
            nvar++, proc->portv[i].name.name, proc->portv[i].type.name, gets);
        break;

      case FBLC_POLARITY_PUT:
        puts = AddVar(
            nvar++, proc->portv[i].name.name, proc->portv[i].type.name, puts);
        break;
    }
  }

  FblcName body_type = CheckActn(env, vars, gets, puts, proc->body);
  if (body_type == NULL) {
    return false;
  }
  if (!FblcNamesEqual(proc->return_type.name, body_type)) {
    FblcReportError("Type mismatch. Expected %s, but found %s.\n",
        proc->body->loc, proc->return_type.name, body_type);
    return false;
  }
  return true;
}

// FblcCheckProgram --
//
//   Check that the given program environment describes a well formed and well
//   typed Fblc program.
//
// Inputs:
//   env - The program environment to check.
//
// Results:
//   The value true if the program environment is well formed and well typed,
//   false otherwise.
//
// Side effects:
//   If the program environment is not well formed, an error message is
//   printed to standard error describing the problem with the program
//   environment.

bool FblcCheckProgram(const FblcEnv* env)
{
  // Verify all type declarations are good.
  for (FblcTypeEnv* types = env->types; types != NULL; types = types->next) {
    if (!CheckType(env, types->decl)) {
      return false;
    }
  }

  // Verify all function declarations are good.
  for (FblcFuncEnv* funcs = env->funcs; funcs != NULL; funcs = funcs->next) {
    if (!CheckFunc(env, funcs->decl)) {
      return false;
    }
  }

  // Verify all process declarations are good.
  for (FblcProcEnv* procs = env->procs; procs != NULL; procs = procs->next) {
    if (!CheckProc(env, procs->decl)) {
      return false;
    }
  }
  return true;
}
