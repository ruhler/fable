// Checker.c --
//
//   This file implements routines for checking an  program is well formed
//   and well typed.

#include <assert.h>     // for assert

#include "fblcs.h"

// The following Vars structure describes a mapping from variable names to
// their types and ids.
//
// We always allocate the Vars mapping on the stack. This means it is not safe
// to return or otherwise capture a Vars* outside of the frame where it is
// allocated.

typedef struct Vars {
  FblcTypeId type;
  struct Vars* next;
} Vars;

typedef struct Ports {
  Name name;
  FblcPolarity polarity;
  FblcTypeId type;
  struct Ports* next;
} Ports;

static void ArgCountError(Loc* loc, size_t expected, size_t actual);
static void TypeMismatchError(SProgram* sprog, Loc* loc, FblcTypeId expected, FblcTypeId actual);

static FblcTypeId LookupType(SProgram* sprog, Name name);
static Vars* AddVar(Vars* vars, FblcTypeId type, Vars* next);
static Ports* AddPort(
    Ports* vars, Name name, FblcTypeId type, FblcPolarity polarity, Ports* next);
static bool CheckArgs(
    SProgram* sprog, Vars* vars, int fieldc, FblcTypeId* fieldv,
    int argc, FblcExpr** argv, Loc* myloc, Loc** loc, SVar** svars);
static FblcTypeId CheckExpr(SProgram* sprog, Vars* vars, FblcExpr* expr, Loc** loc, SVar** svars);
static FblcTypeId CheckActn(SProgram* sprog, Vars* vars, Ports* ports, FblcActn* actn, Loc** loc, SVar** svars, SVar** sportv);
static bool CheckFields(SProgram* sprog, int fieldc, FblcTypeId* fieldv, SVar* fields, const char* kind);
static bool CheckPorts(SProgram* sprog, int portc, FblcPort* portv, SVar* ports);

// ArgCountError --
//   Reports an error of an argument count mismatch.
//
// Inputs:
//   loc - The error location. Conventionally this is the location of the
//         expression where the application of arguments occurs.
//   expected - The number of expected arguments.
//   actual - The number of provided arguments.
//
// Results:
//   None.
//
// Side effects:
//   Reports an error to stderr.
static void ArgCountError(Loc* loc, size_t expected, size_t actual)
{
  ReportError("Expected %d arguments, but %d were provided.\n", loc, expected, actual);
}

// TypeMismatchError --
//   Reports an error where a wrong type is encounterd.
//
// Inputs:
//   sprog - The program environment.
//   loc - The error location. Conventionally this is the location of the
//         expression or action with the wrong type.
//   expected - The id of the expected type.
//   actual - The id of the actual type.
//
// Results:
//   None.
//
// Side effects:
//   Reports an error to stderr.
static void TypeMismatchError(SProgram* sprog, Loc* loc, FblcTypeId expected, FblcTypeId actual)
{
  ReportError("Expected type %s, but found type %s.\n",
      loc, sprog->symbols[expected]->name.name, sprog->symbols[actual]->name.name);
}

// LookupType --
//   Look up the declaration id of the type with the given name in the given
//   environment.
// 
// Inputs:
//   sprog - The environment to look up the type in.
//   name - The name of the type to look up.
//
// Result:
//   The declaration id for the type with the given name, or NULL_ID if
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
  return NULL_ID;
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

// CheckArgs --
//
//   Check that the arguments to a struct or function are well typed, of the
//   proper count, and have the correct types.
//
// Inputs:
//   sprog - The program environment.
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
    SProgram* sprog, Vars* vars, int fieldc, FblcTypeId* fieldv,
    int argc, FblcExpr** argv, Loc* myloc, Loc** loc, SVar** svars)
{
  if (fieldc != argc) {
    ArgCountError(myloc, fieldc, argc);
    return false;
  }

  for (int i = 0; i < fieldc; i++) {
    Loc* argloc = *loc;
    FblcTypeId arg_type_id = CheckExpr(sprog, vars, argv[i], loc, svars);
    if (arg_type_id == NULL_ID) {
      return false;
    }
    if (arg_type_id != fieldv[i]) {
      TypeMismatchError(sprog, argloc, fieldv[i], arg_type_id);
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
//   sprog - The program environment.
//   vars - The names and types of the variables in scope.
//   expr - The expression to verify.
//   loc - An array of locations starting with the location of the current
//         expression and continuing with the locations of every subsequent
//         expression in the order they are encountered in the program.
//   svars - An array of svars starting with the next variable definition and
//           continuing with the svar of every subseqent variable definition
//           in the order it appears in the program.
//
// Result:
//   The type of the expression, or NULL_ID if the expression is not well formed
//   and well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to standard error describing the problem.
//   loc is advanced to the location after all locations in this expression.
//   svars is advanced to the svar after all svars in this expression.

static FblcTypeId CheckExpr(SProgram* sprog, Vars* vars, FblcExpr* expr, Loc** loc, SVar** svars)
{
  Loc* myloc = (*loc)++;
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcVarExpr* var_expr = (FblcVarExpr*)expr;
      assert(var_expr->var != NULL_ID);
      Vars* curr = vars;
      for (size_t i = 0; i < var_expr->var; ++i) {
        assert(curr != NULL && "already resolved variable should be in scope");
        curr = curr->next;
      }
      assert(curr != NULL && "already resolved variable should be in scope");
      return curr->type;
    }

    case FBLC_APP_EXPR: {
      FblcAppExpr* app_expr = (FblcAppExpr*)expr;
      FblcDecl* decl = sprog->program->declv[app_expr->func];
      SDecl* sdecl = sprog->symbols[app_expr->func];
      switch (decl->tag) {
        case FBLC_STRUCT_DECL: {
          FblcTypeDecl* type = (FblcTypeDecl*)decl;
          if (!CheckArgs(sprog, vars, type->fieldc, type->fieldv,
                app_expr->argc, app_expr->argv, myloc, loc, svars)) {
            return NULL_ID;
          }
          return app_expr->func;
        }

        case FBLC_UNION_DECL: {
          ReportError("Cannot do application on union type %s.\n", myloc, sdecl->name.name);
          return NULL_ID;
        }

        case FBLC_FUNC_DECL: {
          FblcFuncDecl* func = (FblcFuncDecl*)decl;
          if (!CheckArgs(sprog, vars, func->argc, func->argv,
                app_expr->argc, app_expr->argv, myloc, loc, svars)) {
            return NULL_ID;
          }
          return func->return_type;
        }

        case FBLC_PROC_DECL: {
          ReportError("Cannot do application on a process %s.\n", myloc, sdecl->name.name);
          return NULL_ID;
        }

        default:
          assert(false && "Invalid decl tag");
          return NULL_ID;
      }
    }

    case FBLC_ACCESS_EXPR: {
      FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
      FblcTypeId type_id = CheckExpr(sprog, vars, access_expr->arg, loc, svars);
      if (type_id == NULL_ID) {
        return NULL_ID;
      }
      FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];
      return type->fieldv[access_expr->field];
    }

    case FBLC_UNION_EXPR: {
      FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
      FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[union_expr->type];
      STypeDecl* stype = (STypeDecl*)sprog->symbols[union_expr->type];
      if (type->tag != FBLC_UNION_DECL) {
        ReportError("Type %s is not a union type.\n", myloc, stype->name.name);
        return NULL_ID;
      }

      Loc* bodyloc = *loc;
      FblcTypeId arg_type_id = CheckExpr(sprog, vars, union_expr->arg, loc, svars);
      if (arg_type_id == NULL_ID) {
        return NULL_ID;
      }

      FblcTypeId field_type_id = type->fieldv[union_expr->field];
      if (arg_type_id != field_type_id) {
        TypeMismatchError(sprog, bodyloc, field_type_id, arg_type_id);
        return NULL_ID;
      }
      return union_expr->type;
    }

    case FBLC_LET_EXPR: {
      FblcLetExpr* let_expr = (FblcLetExpr*)expr;
      SVar* svar = (*svars)++;
      FblcTypeId declared_type_id = LookupType(sprog, svar->type.name);
      if (declared_type_id == NULL_ID) {
        ReportError("Type '%s' not declared.\n", myloc, svar->type.name);
        return NULL_ID;
      }

      Loc* defloc = *loc;
      FblcTypeId actual_type_id = CheckExpr(sprog, vars, let_expr->def, loc, svars);
      if (actual_type_id == NULL_ID) {
        return NULL_ID;
      }

      if (declared_type_id != actual_type_id) {
        TypeMismatchError(sprog, defloc, declared_type_id, actual_type_id);
        return NULL_ID;
      }

      Vars nvars;
      AddVar(&nvars, actual_type_id, vars);
      return CheckExpr(sprog, &nvars, let_expr->body, loc, svars);
    }

    case FBLC_COND_EXPR: {
      FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
      Loc* condloc = *loc;
      FblcTypeId type_id = CheckExpr(sprog, vars, cond_expr->select, loc, svars);
      if (type_id == NULL_ID) {
        return NULL_ID;
      }
      FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];

      if (type->tag != FBLC_UNION_DECL) {
        SDecl* stype = sprog->symbols[type_id];
        ReportError("The condition has type %s, which is not a union type.\n",
            condloc, stype->name.name);
        return NULL_ID;
      }

      if (type->fieldc != cond_expr->argc) {
        ArgCountError(myloc, type->fieldc, cond_expr->argc);
        return NULL_ID;
      }

      FblcTypeId result_type_id = NULL_ID;
      for (int i = 0; i < cond_expr->argc; i++) {
        Loc* argloc = *loc;
        FblcTypeId arg_type_id = CheckExpr(sprog, vars, cond_expr->argv[i], loc, svars);
        if (arg_type_id == NULL_ID) {
          return NULL_ID;
        }

        if (i != 0 && result_type_id != arg_type_id) {
          TypeMismatchError(sprog, argloc, result_type_id, arg_type_id);
          return NULL_ID;
        }
        result_type_id = arg_type_id;
      }
      assert(result_type_id != NULL_ID);
      return result_type_id;
    }

    default: {
      assert(false && "Unknown expression type.");
      return NULL_ID;
    }
  }
}

// CheckActn --
//
//   Verify the given action is well formed and well typed, assuming the
//   rest of the environment is well formed and well typed.
//
// Inputs:
//   sprog - The program environment.
//   vars - The names and types of the variables in scope.
//   ports - The names, types, and polarities of the ports in scope.
//   actn - The action to verify.
//
// Result:
//   The type of the action or NULL_ID if the expression is not well formed and
//   well typed.
//
// Side effects:
//   If the expression is not well formed and well typed, an error message is
//   printed to standard error describing the problem.

static FblcTypeId CheckActn(SProgram* sprog, Vars* vars, Ports* ports, FblcActn* actn, Loc** loc, SVar** svars, SVar** sportv)
{
  Loc* myloc = (*loc)++;
  switch (actn->tag) {
    case FBLC_EVAL_ACTN: {
      FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
      return CheckExpr(sprog, vars, eval_actn->arg, loc, svars);
    }

    case FBLC_GET_ACTN: {
      FblcGetActn* get_actn = (FblcGetActn*)actn;
      for (size_t i = 0; i < get_actn->port && ports != NULL; ++i) {
        ports = ports->next;
      }
      if (ports == NULL || ports->polarity != FBLC_GET_POLARITY) {
        ReportError("Get port not valid.\n", myloc);
        return NULL_ID;
      }
      return ports->type;
    }

    case FBLC_PUT_ACTN: {
      FblcPutActn* put_actn = (FblcPutActn*)actn;
      for (size_t i = 0; i < put_actn->port && ports != NULL; ++i) {
        ports = ports->next;
      }
      if (ports == NULL || ports->polarity != FBLC_PUT_POLARITY) {
        ReportError("Put port not valid.\n", myloc);
        return NULL_ID;
      }

      Loc* argloc = *loc;
      FblcTypeId arg_type_id = CheckExpr(sprog, vars, put_actn->arg, loc, svars);
      if (arg_type_id == NULL_ID) {
        return NULL_ID;
      }
      if (ports->type != arg_type_id) {
        TypeMismatchError(sprog, argloc, ports->type, arg_type_id);
        return NULL_ID;
      }
      return arg_type_id;
    }

    case FBLC_CALL_ACTN: {
      FblcCallActn* call_actn = (FblcCallActn*)actn;
      if (sprog->program->declv[call_actn->proc]->tag != FBLC_PROC_DECL) {
        SDecl* sdecl = sprog->symbols[call_actn->proc];
        ReportError("'%s' is not a proc.\n", myloc, sdecl->name.name);
        return NULL_ID;
      }
      SProcDecl* sproc = (SProcDecl*)sprog->symbols[call_actn->proc];

      FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv[call_actn->proc];
      if (call_actn->portc != proc->portc) {
        SProcDecl* sproc = (SProcDecl*)sprog->symbols[call_actn->proc];
        ReportError("Wrong number of port arguments to %s. Expected %d, "
            "but got %d.\n", myloc, sproc->name.name,
            proc->portc, call_actn->portc);
        return NULL_ID;
      }

      for (int i = 0; i < proc->portc; i++) {
        Ports* curr = ports;
        for (size_t port_id = 0; port_id < call_actn->portv[i] && curr != NULL; ++port_id) {
          curr = curr->next;
        }
        if (curr == NULL) {
          // TODO: Have locations for each port argument in sdecl.
          ReportError("Port arg %i to call does not refer to a valid port.\n", myloc, i);
          return NULL_ID;
        }
        if (curr->polarity != proc->portv[i].polarity) {
          // TODO: Have locations for each port argument in sdecl.
          ReportError("Port arg %i to call has wrong polarity. Expected '%s', but found '%s'.\n", myloc, i, proc->portv[i].polarity == FBLC_PUT_POLARITY ? "put" : "get", curr->polarity == FBLC_PUT_POLARITY ? "put" : "get");
          return NULL_ID;
        }

        if (curr->type != proc->portv[i].type) {
          // TODO: Have locations for each port argument in sdecl.
          SDecl* port_type = sprog->symbols[proc->portv[i].type];
          ReportError("Port arg %i to call has wrong type. Expected port type %s, but found %s.\n", myloc, i, sproc->sportv[i].type.name, port_type->name.name);
          return NULL_ID;
        }
      }

      if (!CheckArgs(sprog, vars, proc->argc, proc->argv, call_actn->argc,
            call_actn->argv, myloc, loc, svars)) {
        return NULL_ID;
      }
      return proc->return_type;
    }

    case FBLC_LINK_ACTN: {
      // TODO: Test that we verify the link type resolves?
      FblcLinkActn* link_actn = (FblcLinkActn*)actn;
      SVar* sgetport = (*sportv)++;
      SVar* sputport = (*sportv)++;
      link_actn->type = LookupType(sprog, sgetport->type.name);
      if (link_actn->type == NULL_ID) {
        ReportError("Type '%s' not declared.\n", myloc, sgetport->type.name);
        return NULL_ID;
      }

      Ports getport;
      Ports putport;
      AddPort(&getport, sgetport->name.name, link_actn->type, FBLC_GET_POLARITY, ports);
      AddPort(&putport, sputport->name.name, link_actn->type, FBLC_PUT_POLARITY, &getport);
      return CheckActn(sprog, vars, &putport, link_actn->body, loc, svars, sportv);
    }

    case FBLC_EXEC_ACTN: {
      FblcExecActn* exec_actn = (FblcExecActn*)actn;
      Vars vars_data[exec_actn->execc];
      Vars* nvars = vars;
      for (int i = 0; i < exec_actn->execc; i++) {
        SVar* var = (*svars)++;
        FblcActn* exec = exec_actn->execv[i];
        Loc* actnloc = *loc;
        FblcTypeId actn_type = CheckActn(sprog, vars, ports, exec, loc, svars, sportv);
        if (actn_type == NULL_ID) {
          return NULL_ID;
        }

        FblcTypeId var_type = LookupType(sprog, var->type.name);
        if (var_type == NULL_ID) {
          return NULL_ID;
        }
        if (var_type != actn_type) {
          TypeMismatchError(sprog, actnloc, var_type, actn_type);
          return NULL_ID;
        }
        nvars = AddVar(vars_data+i, var_type, nvars);
      }
      return CheckActn(sprog, nvars, ports, exec_actn->body, loc, svars, sportv);
    }

    case FBLC_COND_ACTN: {
      FblcCondActn* cond_actn = (FblcCondActn*)actn;
      Loc* condloc = *loc;
      FblcTypeId type_id = CheckExpr(sprog, vars, cond_actn->select, loc, svars);
      if (type_id == NULL_ID) {
        return NULL_ID;
      }
      FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];

      if (type->tag != FBLC_UNION_DECL) {
        SDecl* stype = sprog->symbols[type_id];
        ReportError("The condition has type %s, which is not a union type.\n",
            condloc, stype->name.name);
        return NULL_ID;
      }

      if (type->fieldc != cond_actn->argc) {
        ArgCountError(myloc, type->fieldc, cond_actn->argc);
        return NULL_ID;
      }

      // TODO: Verify that no two branches of the condition refer to the same
      // port. Do we still want to do this?
      FblcTypeId result_type_id = NULL_ID;
      for (int i = 0; i < cond_actn->argc; i++) {
        Loc* argloc = *loc;
        FblcTypeId arg_type_id = CheckActn(sprog, vars, ports, cond_actn->argv[i], loc, svars, sportv);
        if (arg_type_id == NULL_ID) {
          return NULL_ID;
        }

        if (i > 0 && result_type_id != arg_type_id) {
          TypeMismatchError(sprog, argloc, result_type_id, arg_type_id);
          return NULL_ID;
        }
        result_type_id = arg_type_id;
      }
      assert(result_type_id != NULL_ID);
      return result_type_id;
    }
  }
  assert(false && "UNREACHABLE");
  return NULL_ID;
}

// CheckFields --
//
//   Verify the given fields have valid types and unique names. This is used
//   to check fields in a type declaration and arguments in a function or
//   process declaration.
//
// Inputs:
//   sprog - The program environment.
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

static bool CheckFields(SProgram* sprog, int fieldc, FblcTypeId* fieldv, SVar* fields, const char* kind)
{
  // Verify the type for each field exists.
  for (int i = 0; i < fieldc; i++) {
    fieldv[i] = LookupType(sprog, fields[i].type.name);
    if (fieldv[i] == NULL_ID) {
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
//   sprog - The program environment.
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
static bool CheckPorts(SProgram* sprog, int portc, FblcPort* portv, SVar* ports)
{
  // Verify the type for each port exists.
  for (int i = 0; i < portc; i++) {
    portv[i].type = LookupType(sprog, ports[i].type.name);
    if (portv[i].type == NULL_ID) {
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

// CheckProgram --
//
//   Check that the given program environment describes a well formed and well
//   typed program.
//
// Inputs:
//   sprog - The program environment to check.
//
// Results:
//   The value true if the program environment is well formed and well typed,
//   false otherwise.
//
// Side effects:
//   If the program environment is not well formed, an error message is
//   printed to standard error describing the problem with the program
//   environment.
bool CheckProgram(SProgram* sprog)
{
  for (size_t i = 0; i < sprog->program->declc; ++i) {
    FblcDecl* decl = sprog->program->declv[i];
    switch (decl->tag) {
      case FBLC_STRUCT_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        STypeDecl* stype = (STypeDecl*)sprog->symbols[i];
        if (!CheckFields(sprog, type->fieldc, type->fieldv, stype->fields, "field")) {
          return false;
        }
        break;
      }

      case FBLC_UNION_DECL: {
        FblcTypeDecl* type = (FblcTypeDecl*)decl;
        if (type->fieldc == 0) {
          ReportError("A union type must have at least one field.\n", sprog->symbols[i]->name.loc);
          return false;
        }
        STypeDecl* stype = (STypeDecl*)sprog->symbols[i];
        if (!CheckFields(sprog, type->fieldc, type->fieldv, stype->fields, "field")) {
          return false;
        }
        break;
      }

      case FBLC_FUNC_DECL: {
        FblcFuncDecl* func = (FblcFuncDecl*)decl;
        SFuncDecl* sfunc = (SFuncDecl*)sprog->symbols[i];
        // Check the arguments.
        if (!CheckFields(sprog, func->argc, func->argv, sfunc->svarv, "arg")) {
          return false;
        }

        // Check the body.
        Vars nvars[func->argc];
        Vars* vars = NULL;
        SVar* svars = sfunc->svarv;
        for (int i = 0; i < func->argc; i++) {
          // TODO: Add test that we verify the argument types are valid?
          FblcTypeId arg_type_id = LookupType(sprog, (svars++)->type.name);
          if (arg_type_id == NULL_ID) {
            return false;
          }
          vars = AddVar(nvars+i, arg_type_id, vars);
        }
        Loc* loc = sfunc->locv;
        FblcTypeId body_type_id = CheckExpr(sprog, vars, func->body, &loc, &svars);
        if (body_type_id == NULL_ID) {
          return false;
        }
        if (func->return_type != body_type_id) {
          TypeMismatchError(sprog, sfunc->locv, func->return_type, body_type_id);
          return false;
        }
        break;
      }

      case FBLC_PROC_DECL: {
        FblcProcDecl* proc = (FblcProcDecl*)decl;
        SProcDecl* sproc = (SProcDecl*)sprog->symbols[i];
        // Check the ports.
        if (!CheckPorts(sprog, proc->portc, proc->portv, sproc->sportv)) {
          return false;
        }

        // Check the arguments.
        if (!CheckFields(sprog, proc->argc, proc->argv, sproc->svarv, "arg")) {
          return false;
        }

        // Check the body.
        Vars vars_data[proc->argc];
        Ports ports_data[proc->portc];
        Vars* nvar = vars_data;
        Ports* nport = ports_data;

        Vars* vars = NULL;
        SVar* svars = sproc->svarv;
        for (int i = 0; i < proc->argc; i++) {
          // TODO: Add test that we check we managed to resolve the arg types?
          FblcTypeId arg_type_id = LookupType(sprog, (svars++)->type.name);
          if (arg_type_id == NULL_ID) {
            return false;
          }
          vars = AddVar(nvar++, arg_type_id, vars);
        }

        Ports* ports = NULL;
        SVar* sportv = sproc->sportv;
        for (int i = 0; i < proc->portc; i++) {
          // TODO: Add tests that we properly resolved the port types?
          SVar* sport = sportv++;
          FblcTypeId port_type_id = LookupType(sprog, sport->type.name);
          if (port_type_id == NULL_ID) {
            return false;
          }
          switch (proc->portv[i].polarity) {
            case FBLC_GET_POLARITY:
              ports = AddPort(nport++, sport->name.name, port_type_id, FBLC_GET_POLARITY, ports);
              break;

            case FBLC_PUT_POLARITY:
              ports = AddPort(nport++, sport->name.name, port_type_id, FBLC_PUT_POLARITY, ports);
              break;
          }
        }

        Loc* loc = sproc->locv;
        FblcTypeId body_type_id = CheckActn(sprog, vars, ports, proc->body, &loc, &svars, &sportv);
        if (body_type_id == NULL_ID) {
          return false;
        }
        if (proc->return_type != body_type_id) {
          TypeMismatchError(sprog, sproc->locv, proc->return_type, body_type_id);
          return false;
        }
        break;
      }

      default:
        assert(false && "Invalid decl type");
        return false;
    }

    // Verify the declaration does not have the same name as one we have
    // already seen.
    for (size_t j = 0; j < i; ++j) {
      if (NamesEqual(sprog->symbols[i]->name.name, sprog->symbols[j]->name.name)) {
        ReportError("Multiple declarations for %s.\n", sprog->symbols[i]->name.loc, sprog->symbols[i]->name.name);
        return false;
      }
    }
  }
  return true;
}
