// Checker.c --
//
//   This file implements routines for checking an  program is well formed
//   and well typed.

#include "Internal.h"

// The following Vars structure describes a mapping from variable names to
// their types.
//
// We always allocate the Vars mapping on the stack. This means it is not safe
// to return or otherwise capture a Vars* outside of the frame where it is
// allocated.

typedef struct Vars {
  Name name;
  Name type;
  struct Vars* next;
} Vars;

static Vars* AddVar(Vars* vars, Name name, Name type, Vars* next);
static Name LookupVar(Vars* vars, Name name);
static bool CheckArgs(
    Env* env, Vars* vars, int fieldc, Field* fieldv,
    int argc, Expr** argv, LocName* func);
static Name CheckExpr(
    Env* env, Vars* vars, Expr* expr);
static Name CheckActn(Env* env, Vars* vars, Vars* gets,
    Vars* puts, Actn* actn);
static bool CheckFields(
    Env* env, int fieldc, Field* fieldv, const char* kind);
static bool CheckPorts(Env* env, int portc, Port* portv);
static bool CheckType(Env* env, TypeDecl* type);
static bool CheckFunc(Env* env, FuncDecl* func);
static bool CheckProc(Env* env, ProcDecl* proc);

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

static Vars* AddVar(Vars* vars, Name name, Name type, Vars* next)
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

static Name LookupVar(Vars* vars, Name name)
{
  while (vars != NULL) {
    if (NamesEqual(vars->name, name)) {
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
    Env* env, Vars* vars, int fieldc, Field* fieldv,
    int argc, Expr** argv, LocName* func)
{
  if (fieldc != argc) {
    ReportError("Wrong number of arguments to %s. Expected %d, "
        "but got %d.\n", func->loc, func->name, fieldc, argc);
    return false;
  }

  for (int i = 0; i < fieldc; i++) {
    Name arg_type = CheckExpr(env, vars, argv[i]);
    if (arg_type == NULL) {
      return false;
    }
    if (!NamesEqual(fieldv[i].type.name, arg_type)) {
      ReportError("Expected type %s, but found %s.\n",
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

static Name CheckExpr(Env* env, Vars* vars, Expr* expr)
{
  switch (expr->tag) {
    case VAR_EXPR: {
      VarExpr* var_expr = (VarExpr*)expr;
      Name type = LookupVar(vars, var_expr->name.name);
      if (type == NULL) {
        ReportError("Variable '%s' not in scope.\n",
            var_expr->name.loc, var_expr->name.name);
        return NULL;
      }
      return type;
    }

    case APP_EXPR: {
      AppExpr* app_expr = (AppExpr*)expr;
      TypeDecl* type = LookupType(env, app_expr->func.name);
      if (type != NULL) {
        if (type->tag != STRUCT_DECL) {
          ReportError("Cannot do application on non-struct type %s.\n",
              app_expr->func.loc, app_expr->func.name);
          return NULL;
        }
        if (!CheckArgs(env, vars, type->fieldc, type->fieldv,
              app_expr->argc, app_expr->argv, &(app_expr->func))) {
          return NULL;
        }
        return type->name.name;
      }

      FuncDecl* func = LookupFunc(env, app_expr->func.name);
      if (func != NULL) {
        if (!CheckArgs(env, vars, func->argc, func->argv,
              app_expr->argc, app_expr->argv, &(app_expr->func))) {
          return NULL;
        }
        return func->return_type.name;
      }

      ReportError("'%s' is not a type or function.\n",
          app_expr->loc, app_expr->func.name);
      return NULL;
    }

    case ACCESS_EXPR: {
      AccessExpr* access_expr = (AccessExpr*)expr;
      Name typename = CheckExpr(env, vars, access_expr->object);
      if (typename == NULL) {
        return NULL;
      }

      TypeDecl* type = LookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");
      for (int i = 0; i < type->fieldc; i++) {
        if (NamesEqual(type->fieldv[i].name.name,
              access_expr->field.name)) {
          return type->fieldv[i].type.name;
        }
      }
      ReportError("'%s' is not a field of the type '%s'.\n",
          access_expr->field.loc, access_expr->field.name, typename);
      return NULL;
    }

    case UNION_EXPR: {
      UnionExpr* union_expr = (UnionExpr*)expr;
      TypeDecl* type = LookupType(env, union_expr->type.name);
      if (type == NULL) {
        ReportError("Type %s not found.\n",
            union_expr->type.loc, union_expr->type.name);
        return NULL;
      }

      if (type->tag != UNION_DECL) {
        ReportError("Type %s is not a union type.\n",
            union_expr->loc, union_expr->type.name);
        return NULL;
      }

      Name arg_type = CheckExpr(env, vars, union_expr->value);
      if (arg_type == NULL) {
        return NULL;
      }

      for (int i = 0; i < type->fieldc; i++) {
        if (NamesEqual(type->fieldv[i].name.name,
              union_expr->field.name)) {
          if (!NamesEqual(type->fieldv[i].type.name, arg_type)) {
            ReportError("Expected type '%s', but found type '%s'.\n",
                union_expr->value->loc,
                type->fieldv[i].type.name, arg_type);
            return NULL;
          }
          return type->name.name;
        }
      }
      ReportError("Type '%s' has no field '%s'.\n", union_expr->field.loc,
          union_expr->type.name, union_expr->field.name);
      return NULL;
    }

    case LET_EXPR: {
      LetExpr* let_expr = (LetExpr*)expr;
      if (LookupType(env, let_expr->type.name) == NULL) {
        ReportError("Type '%s' not declared.\n",
            let_expr->type.loc, let_expr->type.name);
        return NULL;
      }

      if (LookupVar(vars, let_expr->name.name) != NULL) {
        ReportError("Variable %s already defined.\n",
            let_expr->name.loc, let_expr->name.name);
        return NULL;
      }

      Name type = CheckExpr(env, vars, let_expr->def);
      if (type == NULL) {
        return NULL;
      }

      if (!NamesEqual(let_expr->type.name, type)) {
        ReportError("Expected type %s, but found expression of type %s.\n",
            let_expr->def->loc, let_expr->type.name, type);
        return NULL;
      }

      Vars nvars;
      AddVar(&nvars, let_expr->name.name, type, vars);
      return CheckExpr(env, &nvars, let_expr->body);
    }

    case COND_EXPR: {
      CondExpr* cond_expr = (CondExpr*)expr;
      Name typename = CheckExpr(env, vars, cond_expr->select);
      if (typename == NULL) {
        return NULL;
      }

      TypeDecl* type = LookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");

      if (type->tag != UNION_DECL) {
        ReportError("The condition has type %s, "
            "which is not a union type.\n", cond_expr->loc, typename);
        return NULL;
      }

      if (type->fieldc != cond_expr->argc) {
        ReportError("Wrong number of arguments to condition. Expected %d, "
            "but found %d.\n", cond_expr->loc, type->fieldc, cond_expr->argc);
        return NULL;
      }

      Name result_type = NULL;
      for (int i = 0; i < cond_expr->argc; i++) {
        Name arg_type = CheckExpr(env, vars, cond_expr->argv[i]);
        if (arg_type == NULL) {
          return NULL;
        }

        if (result_type != NULL && !NamesEqual(result_type, arg_type)) {
          ReportError("Expected expression of type %s, "
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

static Name CheckActn(Env* env, Vars* vars, Vars* gets, Vars* puts, Actn* actn)
{
  switch (actn->tag) {
    case EVAL_ACTN: {
      EvalActn* eval_actn = (EvalActn*)actn;
      return CheckExpr(env, vars, eval_actn->expr);
    }

    case GET_ACTN: {
      GetActn* get_actn = (GetActn*)actn;
      Name type = LookupVar(gets, get_actn->port.name);
      if (type == NULL) {
        ReportError("'%s' is not a valid get port.\n", get_actn->loc,
            get_actn->port.name);
        return NULL;
      }
      return type;
    }

    case PUT_ACTN: {
      PutActn* put_actn = (PutActn*)actn;
      Name port_type = LookupVar(puts, put_actn->port.name);
      if (port_type == NULL) {
        ReportError("'%s' is not a valid put port.\n", put_actn->loc,
            put_actn->port.name);
        return NULL;
      }

      Name arg_type = CheckExpr(env, vars, put_actn->expr);
      if (arg_type == NULL) {
        return NULL;
      }
      if (!NamesEqual(port_type, arg_type)) {
        ReportError("Expected type %s, but found %s.\n",
            put_actn->expr->loc, port_type, arg_type);
        return NULL;
      }
      return arg_type;
    }

    case CALL_ACTN: {
      CallActn* call_actn = (CallActn*)actn;
      ProcDecl* proc = LookupProc(env, call_actn->proc.name);
      if (proc == NULL) {
        ReportError("'%s' is not a proc.\n",
            call_actn->loc, call_actn->proc.name);
        return NULL;
      }

      if (call_actn->portc  != proc->portc) {
        ReportError("Wrong number of port arguments to %s. Expected %d, "
            "but got %d.\n", call_actn->loc, call_actn->proc.name,
            proc->portc, call_actn->portc);
        return NULL;
      }

      for (int i = 0; i < proc->portc; i++) {
        bool isput = (proc->portv[i].polarity == POLARITY_PUT);
        Vars* ports = isput ? puts : gets;
        Name port_type = LookupVar(ports, call_actn->ports[i].name);
        if (port_type == NULL) {
          ReportError("'%s' is not a valid %s port.\n",
              call_actn->ports[i].loc,
              call_actn->ports[i].name,
              isput ? "put" : "get");
          return NULL;
        }

        if (!NamesEqual(proc->portv[i].type.name, port_type)) {
          ReportError("Expected port type %s, but found %s.\n",
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

    case LINK_ACTN: {
      LinkActn* link_actn = (LinkActn*)actn;
      Name type = link_actn->type.name;
      Vars ngets;
      Vars nputs;
      AddVar(&ngets, link_actn->getname.name, type, gets);
      AddVar(&nputs, link_actn->putname.name, type, puts);
      return CheckActn(env, vars, &ngets, &nputs, link_actn->body);
    }

    case EXEC_ACTN: {
      ExecActn* exec_actn = (ExecActn*)actn;
      Vars nvars[exec_actn->execc];
      for (int i = 0; i < exec_actn->execc; i++) {
        Exec* exec = &(exec_actn->execv[i]);
        Name type = CheckActn(env, vars, gets, puts, exec->actn);
        if (type == NULL) {
          return NULL;
        }

        if (!NamesEqual(exec->var.type.name, type)) {
          ReportError("Expected type %s, but found %s.\n",
              exec->actn->loc, exec->var.type.name, type);
          return NULL;
        }
        vars = AddVar(nvars+i, exec->var.name.name, exec->var.type.name, vars);
      }
      return CheckActn(env, vars, gets, puts, exec_actn->body);
    }

    case COND_ACTN: {
      CondActn* cond_actn = (CondActn*)actn;
      Name typename = CheckExpr(env, vars, cond_actn->select);
      if (typename == NULL) {
        return NULL;
      }

      TypeDecl* type = LookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");

      if (type->tag != UNION_DECL) {
        ReportError("The condition has type %s, "
            "which is not a union type.\n", cond_actn->loc, typename);
        return NULL;
      }

      if (type->fieldc != cond_actn->argc) {
        ReportError("Wrong number of arguments to condition. Expected %d, "
            "but found %d.\n", cond_actn->loc, type->fieldc, cond_actn->argc);
        return NULL;
      }

      // TODO: Verify that no two branches of the condition refer to the same
      // port.
      Name result_type = NULL;
      for (int i = 0; i < cond_actn->argc; i++) {
        Name arg_type = CheckActn(
            env, vars, gets, puts, cond_actn->args[i]);
        if (arg_type == NULL) {
          return NULL;
        }

        if (result_type != NULL && !NamesEqual(result_type, arg_type)) {
          ReportError("Expected process of type %s, "
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

static bool CheckFields(Env* env, int fieldc, Field* fieldv, const char* kind)
{
  // Verify the type for each field exists.
  for (int i = 0; i < fieldc; i++) {
    if (LookupType(env, fieldv[i].type.name) == NULL) {
      ReportError("Type '%s' not found.\n",
          fieldv[i].type.loc, fieldv[i].type.name);
      return false;
    }
  }

  // Verify fields have unique names.
  for (int i = 0; i < fieldc; i++) {
    for (int j = i+1; j < fieldc; j++) {
      if (NamesEqual(fieldv[i].name.name, fieldv[j].name.name)) {
        ReportError("Multiple %ss named '%s'.\n",
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

static bool CheckPorts(Env* env, int portc, Port* portv)
{
  // Verify the type for each port exists.
  for (int i = 0; i < portc; i++) {
    if (LookupType(env, portv[i].type.name) == NULL) {
      ReportError("Type '%s' not found.\n",
          portv[i].type.loc, portv[i].type.name);
      return false;
    }
  }

  // Verify ports have unique names.
  for (int i = 0; i < portc; i++) {
    for (int j = i+1; j < portc; j++) {
      if (NamesEqual(portv[i].name.name, portv[j].name.name)) {
        ReportError("Multiple ports named '%s'.\n",
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

static bool CheckType(Env* env, TypeDecl* type)
{
  if (type->tag == UNION_DECL && type->fieldc == 0) {
    ReportError("A union type must have at least one field.\n",
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

static bool CheckFunc(Env* env, FuncDecl* func)
{
  // Check the arguments.
  if (!CheckFields(env, func->argc, func->argv, "arg")) {
    return false;
  }

  // Check the return type.
  if (LookupType(env, func->return_type.name) == NULL) {
    ReportError("Type '%s' not found.\n",
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
  Name body_type = CheckExpr(env, vars, func->body);
  if (body_type == NULL) {
    return false;
  }
  if (!NamesEqual(func->return_type.name, body_type)) {
    ReportError("Type mismatch. Expected %s, but found %s.\n",
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

static bool CheckProc(Env* env, ProcDecl* proc)
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
  if (LookupType(env, proc->return_type.name) == NULL) {
    ReportError("Type '%s' not found.\n",
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
      case POLARITY_GET:
        gets = AddVar(
            nvar++, proc->portv[i].name.name, proc->portv[i].type.name, gets);
        break;

      case POLARITY_PUT:
        puts = AddVar(
            nvar++, proc->portv[i].name.name, proc->portv[i].type.name, puts);
        break;
    }
  }

  Name body_type = CheckActn(env, vars, gets, puts, proc->body);
  if (body_type == NULL) {
    return false;
  }
  if (!NamesEqual(proc->return_type.name, body_type)) {
    ReportError("Type mismatch. Expected %s, but found %s.\n",
        proc->body->loc, proc->return_type.name, body_type);
    return false;
  }
  return true;
}

// CheckProgram --
//
//   Check that the given program environment describes a well formed and well
//   typed  program.
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

bool CheckProgram(Env* env)
{
  for (size_t i = 0; i < env->declc; ++i) {
    Decl* decl = env->declv[i];
    switch (decl->tag) {
      case STRUCT_DECL:
      case UNION_DECL:
        if (!CheckType(env, (TypeDecl*)decl)) {
          return false;
        }
        break;

      case FUNC_DECL:
        if (!CheckFunc(env, (FuncDecl*)decl)) {
          return false;
        }
        break;

      case PROC_DECL:
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
      if (NamesEqual(env->declv[i]->name.name, env->declv[j]->name.name)) {
        ReportError("Multiple declarations for %s.\n",
            env->declv[i]->name.loc, env->declv[i]->name.name);
        return false;
      }
    }
  }
  return true;
}
