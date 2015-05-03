// FblcChecker.c --
//
//   This file implements routines for checking an Fblc program is well formed
//   and well typed.

#include "FblcChecker.h"

// The following Scope structure describes a scope mapping variable names to
// their types.

typedef struct Scope {
  FblcName name;
  FblcName type;
  struct Scope* next;
} Scope;

static Scope AddVar(FblcName name, FblcName type, Scope* next);
static FblcName LookupVar(Scope* scope, FblcName name);
static bool CheckArgs(
    const FblcEnv* env, Scope* scope, int num_fields, FblcField* fields,
    int num_args, FblcExpr** args, FblcLoc* loc, const char* func);
static FblcName CheckExpr(
    const FblcEnv* env, Scope* scope, const FblcExpr* expr);
static bool CheckFields(
    const FblcEnv* env, int num_fields, FblcField* fields, const char* kind);
static bool CheckType(const FblcEnv* env, FblcType* type);
static bool CheckFunc(const FblcEnv* env, FblcFunc* func);

// AddVar --
//
//   Add a variable to the given scope.
//
// Inputs:
//   name - The name of the variable to add.
//   type - The type of the variable.
//   next - The scope to add it to.
//
// Results:
//   A new scope with the added variable.
//
// Side effects:
//   None.

static Scope AddVar(FblcName name, FblcName type, Scope* next)
{
  Scope* scope = GC_MALLOC(sizeof(Scope));
  scope->name = name;
  scope->type = type;
  scope->next = next;
  return scope;
}

// LookupVar --
//
//   Look up the type of a variable in scope.
//
// Inputs:
//   scope - The scope to look up the variable in.
//   name - The name of the variable.
//
// Results:
//   The type of the variable in scope, or NULL if there is no variable with
//   that name in scope.
//
// Side effects:
//   None.

static FblcName LookupVar(Scope* scope, FblcName name)
{
  while (scope != NULL) {
    if (FblcNamesEqual(scope->name, name)) {
      return scope->type;
    }
    scope = scope->next;
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
//   scope - The types of local variables in scope.
//   num_fields - The number of arguments expected by the struct or function.
//   fields - The num_fields fields from the struct or function declaration.
//   num_args - The number of arguments passed to the struct or function.
//   args - The num_args args passed to the struct or function.
//   loc - The location of the application expression.
//   func - A descriptive name of the function being defined, for use in error
//          messages.
//
// Results:
//   The value true if the arguments have the right type, false otherwise.
//
// Side effects:
//   If the arguments do not have the right type, prints a message on standard
//   error describing what's wrong.

static bool CheckArgs(
    const FblcEnv* env, Scope* scope, int num_fields, FblcField* fields,
    int num_args, FblcExpr** args, FblcLoc* loc, const char* func)
{
  if (num_fields != num_args) {
    FblcReportError("Wrong number of arguments to %s. Expected %d, but got %d.",
        loc, func, num_fields, num_args);
    return false;
  }

  for (int i = 0; i < num_fields; i++) {
    FblcName arg_type = CheckExpr(env, scope, args[i]);
    if (arg_type == NULL) {
      return false;
    }
    if (!FblcNamesEqual(fields[i].type, arg_type)) {
      FblcReportError("Expected type %s, but found %s.",
          args[i].loc, fields[i].type, arg_type);
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
//   scope - The names and types of the variables in scope.
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
    const FblcEnv* env, Scope* scope, const FblcExpr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FblcName type = LookupVar(scope, expr->ex.var.name);
      if (type == NULL) {
        FblcReportError("Variable '%s' not in scope.", expr->loc,
            expr->ex.var.name);
        return NULL;
      }
      return type;
    }

    case FBLC_APP_EXPR: {
      FblcType type = FblcLookupType(env, expr->ex.app.func);
      if (type != NULL) {
        if (type->kind != FBLC_KIND_STRUCT) {
          FblcReportError("Cannot do application on non-struct type %s.",
              expr->loc, expr->ex.app.func);
          return NULL;
        }
        // TODO: Don't assume there are the right number of arguments.
        if (!CheckArgs(env, scope, type->num_fields, type->fields,
              type->num_fields, expr->args, expr->loc, expr->ex.app.func)) {
          return NULL;
        }
        return type->name;
      }

      FblcFunc func = FblcLookupFunc(env, expr->ex.app.func);
      if (func != NULL) {
        // TODO: Don't assume there are the right number of arguments.
        if (!CheckArgs(env, scope, func->num_args, func->args,
              func->num_args, expr->args, expr->loc, expr->ex.app.func)) {
          return NULL;
        }
        return func->return_type;
      }

      FblcReportError("'%s' is not a type or function.",
          expr->loc, expr->ex.app.func);
      return NULL;
    }

    case FBLC_ACCESS_EXPR: {
      FblcName typename = CheckExpr(env, scope, expr->ex.access.object);
      if (typename == NULL) {
        return NULL;
      }

      FblcType* type = FblcLookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");
      for (int i = 0; i < type->num_fields; i++) {
        if (FblcNamesEqual(type->fields[i].name, expr->ex.access.field)) {
          return type->fields[i].type;
        }
      }
      FblcReportError("The type %s has no field %s.",
          expr->ex.access.field.loc, typename, expr->ex.access.field.name);
      return NULL;
    }

    case FBLC_UNION_EXPR: {
      FblcType* type = FblcLookupType(env, expr->ex.union_.type);
      if (type == NULL) {
        FblcReportError("Type %s not found.", expr->loc, expr->ex.union_.type);
        return NULL;
      }

      if (type->kind != FBLC_KIND_UNION) {
        FblcReportError("Type %s is not a union type.",
            expr->loc, expr->ex.union_.type);
        return NULL;
      }

      FblcType arg_type = CheckExpr(env, scope, expr->ex.union_.value);
      if (arg_type == NULL) {
        return NULL;
      }

      for (int i = 0; i < type->num_fields; i++) {
        if (FblcNamesEqual(type->fields[i].name, expr->ex.union_.field)) {
          if (type->fields[i].type != arg_type) {
            FblcReportError("Expected type %s, but found type %s.",
                expr->ex.union_.value->loc, type->fields[i].type, arg_type);
            return NULL;
          }
          return type->name;
        }
      }
      FblcReportError("Type '%s' has no field '%s'.",
          expr->ex.union_.type, expr->ex.union_.field.loc);
      return NULL;
    }

    case FBLC_LET_EXPR: {
      if (FblcLookupType(env, expr->ex.let.type) == NULL) {
        FblcReportError("No type named '%s'.", expr->loc, expr->ex.let.type);
        return NULL;
      }

      if (LookupVar(scope, expr->ex.let.name) != NULL) {
        FblcReportError("Variable %s already defined.",
            expr->ex.let.name.loc, expr->ex.let.name);
        return NULL;
      }

      FblcName type = CheckExpr(env, scope, expr->ex.let.def);
      if (type == NULL) {
        return NULL;
      }

      if (!FblcNamesEqual(expr->ex.let.type, type)) {
        FblcReportError("Expected type %s, but found type %s.",
            expr->ex.let.type, type);
        return NULL;
      }

      Scope* nscope = AddVar(expr->ex.let.name, type, scope);
      return CheckExpr(env, nscope, expr->ex.let.body);
    }

    case FBLC_COND_EXPR: {
      FblcName typename = CheckExpr(env, scope, expr->ex.cond.select);
      if (typename == NULL) {
        return NULL;
      }

      FblcType* type = FblcLookupType(env, typename);
      assert(type != NULL && "Result of CheckExpr refers to undefined type?");

      if (type->kind != FBLC_KIND_UNION) {
        FblcReportError("The condition has type %s, which is not a union type.",
            expr->loc, typename);
        return NULL;
      }

      // TODO: Don't assume the right number of arguments are given.
      assert(type->num_fields > 0 && "Did we not check this condition before?");
      FblcName result_type = NULL;
      for (int i = 0; i < type->num_fields; i++) {
        FblcName arg_type = CheckExpr(env, scope, expr->args[i]);
        if (arg_type == NULL) {
          return NULL;
        }

        if (result_type != NULL && !FblcNamesEqual(result_type, arg_type)) {
          FblcReportError("Expected type %s, but found type %s.",
              expr->args[i]->loc, result_type, arg_type);
          return NULL;
        }
        result_type = arg_type;
      }
      return result_type;
    }

    default: {
      assert(false && "Unknown expression type.");
      return NULL;
    }
  }
}

// CheckFields --
//
//   Verify the given fields have valid types and unique names. This is used
//   to check fields in a type declaration and arguments in a function
//   declaration.
//
// Inputs:
//   env - The program environment.
//   num_fields - The number of fields to verify.
//   fields - The fields to verify.
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
    const FblcEnv* env, int num_fields, FblcField* fields, const char* kind)
{
  // Verify the type for each field exists.
  for (int i = 0; i < num_fields; i++) {
    if (FblcLookupType(env, fields[i].type) == NULL) {
      FblcReportError("Type '%s' not found.", fields[i].loc, fields[i].type);
      return false;
    }
  }

  // Verify fields have unique names.
  for (int i = 0; i < num_fields; i++) {
    for (int j = i+1; j < num_fields; j++) {
      if (FblcNamesEqual(fields[i].name, fields[j].name)) {
        FblcReportError("Multiple %ss named '%s'.", kind,
            fields[i].loc, fields[i].name);
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
  if (type->kind == FBLC_KIND_UNION && type->num_fields == 0) {
    FblcReportError("A union type must have at least one field.", type->loc);
    return false;
  }
  return CheckFields(env, type->num_fields, type->fields, "field");
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
  if (!CheckFields(env, func->num_args, func->args, "arg")) {
    return false;
  }

  // Check the return type.
  FblcXXX out_type = func->out_type;
  if (LookupType(env, out_type.type) == NULL) {
    FblcReportError("Type '%s' not found.", out_type.loc, out_type.type);
    return false;
  }

  // Check the body.
  Scope* scope = NULL;
  for (int i = 0; i < func->num_args; i++) {
    scope = AddVar(func->args[i].name, func->args[i].type, scope);
  }
  FblcName body_type = CheckExpr(env, scope, func->body);
  if (body_type == NULL) {
    return false;
  }
  if (!FblcNamesEqual(out_type.type, body_type)) {
    FblcReportError("Type mismatch. Expected %s, but found %s.",
        func->body.loc, out_type.type, body_type.type);
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
  // Verify declaration names are unique.
  // Verify all type declarations are good.
  // Verify all function declarations are good.
  assert(false && "TODO: implement FblcCheckProgram");
}
