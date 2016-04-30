// FblcEvaluator.c --
//
//   This file implements routines for evaluating Fblc expressions.

#include "FblcInternal.h"

// An Fblc value is represented using the following FblcValue data structure.
// For struct values, 'tag' is unused and 'fieldv' contains the field data in
// the order the fields are declared in the type declaration.
// For union values, 'tag' is the index of the active field and 'fieldv' is a
// single element array with the field value.

struct FblcValue {
  FblcType* type;
  int tag;
  struct FblcValue* fieldv[];
};

// The following defines a Vars structure for storing the value of local
// variables. It is possible to extend a local variable scope without
// modifying the original local variable scope, and it is possible to access
// the address of a value stored in the variable scope. Typical usage is to
// extend the variable scope by reserving slots for values that are yet to be
// computed, and to ensure the values are computed and updated by the time the
// scope is used.

typedef struct Vars {
  FblcName name;
  FblcValue* value;
  struct Vars* next;
} Vars;

// The evaluator works by breaking down action and expression evaluation into
// a sequence of commands that can be executed in turn. All of the state of
// evaluation, including the stack, is stored explicitly in the command list.
// By storing the stack explicitly instead of piggy-backing off of the C
// runtime stack, we are able to implement the evaluator as a single while
// loop and avoid problems with supporting tail recursive Fblc programs.

// The following CmdTag enum identifies the types of commands used to evaluate
// actions and expressions. The Cmd struct represents a command list. See the
// comments on the extra data in the Cmd structure for the specification of
// the commands.
// The state of the stack is captured in a command list by storing variables
// in 'vars' commands.

typedef enum {
  CMD_EXPR, CMD_ACCESS, CMD_COND, CMD_VARS
} CmdTag;

typedef struct Cmd {
  CmdTag tag;
  union {
    // The expr command evaluates expr and stores the resulting value in
    // *target.
    struct {
      const FblcExpr* expr;
      FblcValue** target;
    } expr;

    // The actn command executes actn and stores the resulting value, if any,
    // in *target.
    struct {
      FblcActn* actn;
      FblcValue** target;
    } actn;

    // The access command accesses the given field of the given value and
    // stores the resulting value in *target.
    struct {
      FblcValue* value;
      FblcName field;
      FblcValue** target;
    } access;

    // The cond command uses the tag of 'value' to select the choice to
    // evaluate. It then evaluates the chosen expression and stores the
    // resulting value in *target.
    struct {
      FblcValue* value;
      FblcExpr* const* choices;
      FblcValue** target;
    } cond;

    // The vars command sets the current vars to the given vars.
    struct {
      Vars* vars;
    } vars;
  } ex;
  struct Cmd* next;
} Cmd;

static FblcValue** LookupRef(Vars* vars, FblcName name);
static FblcValue* LookupVal(Vars* vars, FblcName name);
static Vars* AddVar(Vars* vars, FblcName name);

static FblcValue* NewValue(FblcType* type);
static FblcValue* NewUnionValue(FblcType* type, int tag);
static Cmd* MkExpr(const FblcExpr* expr, FblcValue** target, Cmd* next);
static Cmd* MkAccess(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next);
static Cmd* MkCond(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next);
static Cmd* MkVars(Vars* vars, Cmd* next);
static int TagForField(const FblcType* type, FblcName field);

// LookupRef --
//
//   Look up a reference to the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable reference.
//   name - The name of the variable to lookup.
//
// Returns:
//   A pointer to the value of the variable with the given name in scope, or
//   NULL if no such variable or value was found in scope.
//
// Side effects:
//   None.

static FblcValue** LookupRef(Vars* vars, FblcName name)
{
  for ( ; vars != NULL; vars = vars->next) {
    if (FblcNamesEqual(vars->name, name)) {
      return &(vars->value);
    }
  }
  return NULL;
}

// LookupVal --
//
//   Look up the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable value.
//   name - The name of the variable to lookup.
//
// Returns:
//   The value of the variable with the given name in scope, or NULL if no
//   such variable or value was found in scope.
//
// Side effects:
//   None.

static FblcValue* LookupVal(Vars* vars, FblcName name)
{
  FblcValue** ref = LookupRef(vars, name);
  return ref == NULL ? NULL : *ref;
}

// AddVar --
//   
//   Extend the given scope with a new variable. Use LookupRef to access the
//   newly added variable.
//
// Inputs:
//   vars - The scope to extend.
//   name - The name of the variable to add.
//
// Returns:
//   A new scope with the new variable and the contents of the given scope.
//
// Side effects:
//   None.

static Vars* AddVar(Vars* vars, FblcName name)
{
  Vars* newvars = GC_MALLOC(sizeof(Vars));
  newvars->name = name;
  newvars->value = NULL;
  newvars->next = vars;
  return newvars;
}

// NewValue --
//
//   Create an uninitialized new value of the given type.
//
// Inputs:
//   type - The type of the value to create.
//
// Result:
//   The newly created value. The value with have the type field set, but the
//   fields will be uninitialized.
//
// Side effects:
//   None.

static FblcValue* NewValue(FblcType* type)
{
  int fieldc = type->fieldc;
  if (type->kind == FBLC_KIND_UNION) {
    fieldc = 1;
  }
  FblcValue* value = GC_MALLOC(sizeof(FblcValue) + fieldc * sizeof(FblcValue*));
  value->type = type;
  return value;
}

// NewUnionValue --
//
//   Create a new union value with the given type and tag.
//
// Inputs:
//   type - The type of the union value to create. This should be a union type.
//   tag - The tag of the union value to set.
//
// Result:
//   The new union value, with type and tag set. The field value will be
//   uninitialized.
//
// Side effects:
//   None.

static FblcValue* NewUnionValue(FblcType* type, int tag)
{
  assert(type->kind == FBLC_KIND_UNION);
  FblcValue* value = NewValue(type);
  value->tag = tag;
  return value;
}

// MkExpr --
//
//   Creates a command to evaluate the given expression, storing the resulting
//   value at the given target location.
//
// Inputs:
//   expr - The expression for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation. This must not be NULL.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkExpr(const FblcExpr* expr, FblcValue** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_EXPR;
  cmd->ex.expr.expr = expr;
  cmd->ex.expr.target = target;
  cmd->next = next;
  return cmd;
}

// MkAccess --
//   
//   Create a command to access a field from a value, storing the result at
//   the given target location.
//
// Inputs:
//   value - The value that will be accessed.
//   field - The name of the field to access.
//   target - The target destination of the accessed field.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkAccess(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_ACCESS;
  cmd->ex.access.value = value;
  cmd->ex.access.field = field;
  cmd->ex.access.target = target;
  cmd->next = next;
  return cmd;
}

// MkCond --
//   
//   Create a command to select and evaluate an expression based on the tag of
//   the given value.
//
// Inputs:
//   value - The value to condition on.
//   choices - The list of expressions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkCond(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_COND;
  cmd->ex.cond.value = value;
  cmd->ex.cond.choices = choices;
  cmd->ex.cond.target = target;
  cmd->next = next;
  return cmd;
}

// MkVars --
//   
//   Create a command to change the current local variable scope.
//
// Inputs:
//   vars - The new value of the local variable scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkVars(Vars* vars, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_VARS;
  cmd->ex.vars.vars = vars;
  cmd->next = next;
  return cmd;
}

// TagForField --
//
//   Return the tag corresponding to the named field for the given type.
//   It is a program error if the named field does not exist for the given
//   type.
//
// Inputs:
//   type - The type in question.
//   field - The field to get the tag for.
//
// Result:
//   The index in the list of fields for the type containing that field name.
//
// Side effects:
//   None.

static int TagForField(const FblcType* type, FblcName field)
{
  for (int i = 0; i < type->fieldc; i++) {
    if (FblcNamesEqual(field, type->fieldv[i].name.name)) {
      return i;
    }
  }
  assert(false && "No such field.");
}

// FblcPrintValue --
//
//   Print a value in standard format to the given FILE stream.
//
// Inputs:
//   stream - The stream to print the value to.
//   value - The value to print.
//
// Result:
//   None.
//
// Side effects:
//   The value is printed to the given file stream.

void FblcPrintValue(FILE* stream, FblcValue* value)
{
  FblcType* type = value->type;
  if (type->kind == FBLC_KIND_STRUCT) {
    fprintf(stream, "%s(", type->name.name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcPrintValue(stream, value->fieldv[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_KIND_UNION) {
    fprintf(stream, "%s:%s(",
        type->name.name, type->fieldv[value->tag].name.name);
    FblcPrintValue(stream, value->fieldv[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcEvaluate --
//
//   Evaluate an expression under the given program environment. The program
//   and expression must be well formed.
//
// Inputs:
//   env - The program environment.
//   expr - The expression to evaluate.
//
// Returns:
//   The result of evaluating the given expression in the program environment
//   in a scope with no local variables.
//
// Side effects:
//   None.

FblcValue* FblcEvaluate(const FblcEnv* env, const FblcExpr* expr)
{
  FblcValue* result = NULL;
  Vars* vars = NULL;
  Cmd* cmd = MkExpr(expr, &result, NULL);
  while (cmd != NULL) {
    switch (cmd->tag) {
      case CMD_EXPR: {
        const FblcExpr* expr = cmd->ex.expr.expr;
        FblcValue** target = cmd->ex.expr.target;
        cmd = cmd->next;
        switch (expr->tag) {
          case FBLC_VAR_EXPR: {
            FblcName var_name = expr->ex.var.name.name;
            *target = LookupVal(vars, var_name);
            assert(*target != NULL && "Var not in scope");
            break;
          }

          case FBLC_APP_EXPR: {
            FblcType* type = FblcLookupType(env, expr->ex.app.func.name);
            if (type != NULL) {
              if (type->kind == FBLC_KIND_STRUCT) {
                // Create the struct value now, then add commands to evaluate
                // the arguments to fill in the fields with the proper results.
                *target = NewValue(type);
                for (int i = 0; i < type->fieldc; i++) {
                  cmd = MkExpr(expr->argv[i], &((*target)->fieldv[i]), cmd);
                }
              } else {
                assert(false && "Invalid kind of type for application");
              }
              break;
            }

            FblcFunc* func = FblcLookupFunc(env, expr->ex.app.func.name);
            if (func != NULL) {
              // Add to the top of the command list:
              // arg -> ... -> arg -> vars -> body -> (vars) -> ...
              // The results of the arg evaluations will be stored directly in
              // the vars for the vars command.

              // Don't put a vars change if we will immediately 
              // change it back to different vars. This is important to
              // avoid memory leaks for tail calls.
              if (cmd != NULL && cmd->tag != CMD_VARS) {
                cmd = MkVars(vars, cmd);
              }

              cmd = MkExpr(func->body, target, cmd);
              cmd = MkVars(NULL, cmd);

              Cmd* vcmd = cmd;
              Vars* nvars = NULL;
              for (int i = 0; i < func->argc; i++) {
                FblcName var_name = func->argv[i].name.name;
                nvars = AddVar(nvars, var_name);
                cmd = MkExpr(expr->argv[i], LookupRef(nvars, var_name), cmd);
              }
              vcmd->ex.vars.vars = nvars;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            // Add to the top of the command list:
            // object -> access -> ...
            cmd = MkAccess(NULL, expr->ex.access.field.name, target, cmd);
            cmd = MkExpr(expr->ex.access.object, &(cmd->ex.access.value), cmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            FblcType* type = FblcLookupType(env, expr->ex.union_.type.name);
            assert(type != NULL);
            int tag = TagForField(type, expr->ex.union_.field.name);
            *target = NewUnionValue(type, tag);
            cmd = MkExpr(expr->ex.union_.value, &((*target)->fieldv[0]), cmd);
            break;
          }

          case FBLC_LET_EXPR: {
            // Add to the top of the command list:
            // def -> var -> body -> (vars) -> ...

            // No need to pop the variable if we are going to switch to
            // different vars immediately after anyway.
            if (cmd != NULL && cmd->tag != CMD_VARS) {
              cmd = MkVars(vars, cmd);
            }
            FblcName var_name = expr->ex.let.name.name;
            Vars* nvars = AddVar(vars, var_name);
            cmd = MkExpr(expr->ex.let.body, target, cmd);
            cmd = MkVars(nvars, cmd);
            cmd = MkExpr(expr->ex.let.def, LookupRef(nvars, var_name), cmd);
            break;
          }

          case FBLC_COND_EXPR: {
            // Add to the top of the command list:
            // select -> cond -> ...
            cmd = MkCond(NULL, expr->argv, target, cmd);
            cmd = MkExpr(expr->ex.cond.select, &(cmd->ex.cond.value), cmd);
            break;
          }
        }
        break;
      }

      case CMD_ACCESS: {
        FblcType* type = cmd->ex.access.value->type;
        int target_tag = TagForField(type, cmd->ex.access.field);
        int actual_tag = cmd->ex.access.value->tag;
        FblcValue** target = cmd->ex.access.target;
        if (type->kind == FBLC_KIND_STRUCT) {
          *target = cmd->ex.access.value->fieldv[target_tag];
        } else if (actual_tag == target_tag) {
          *target = cmd->ex.access.value->fieldv[0];
        } else {
          fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
          abort();
        }
        cmd = cmd->next;
        break;
      }

      case CMD_COND: {
        FblcValue* value = cmd->ex.cond.value;
        FblcValue** target = cmd->ex.cond.target;
        assert(value->type->kind == FBLC_KIND_UNION);
        cmd = MkExpr(cmd->ex.cond.choices[value->tag], target, cmd->next);
        break;
      }

      case CMD_VARS:
        vars = cmd->ex.vars.vars;
        cmd = cmd->next;
        break;
    }
  }
  return result;
}
