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

// The following defines a Scope structure for storing the value of local
// variables. Currently the code assumes it is possible to extend a scope
// without modifying the original scope, and it is possible to access the
// address of a value stored in scope.
// TODO: Make this more explicit in the code to allow for fancier
// implementations of scopes if needed in the future.

typedef struct Scope {
  FblcName name;
  FblcValue* value;
  struct Scope* next;
} Scope;

// The evaluator works by breaking down expression evaluation into a sequence
// of commands that can be executed in turn. All of the state of evaluation,
// including the stack, is stored explicitly in the command list. By storing
// the stack explicitly instead of piggy-backing off of the C runtime stack,
// we are able to implement the evaluator as a single while loop and avoid
// problems with supporting tail recursive Fblc programs.

// The following CmdTag enum identifies the types of commands used to evaluate
// expression. The Cmd struct represents a command list. See the comments on
// the extra data in the Cmd structure for the specification of the commands.
// The state of the stack is captured in a command list by storing scopes in
// 'scope' commands.

typedef enum {
  CMD_EVAL, CMD_ACCESS, CMD_COND, CMD_SCOPE
} CmdTag;

typedef struct Cmd {
  CmdTag tag;
  union {
    // The eval command evaluates expr and stores the resulting value in
    // *target.
    struct {
      const FblcExpr* expr;
      FblcValue** target;
    } eval;

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

    // The scope command sets the current scope to the given scope.
    struct {
      Scope* scope;
    } scope;
  } ex;
  struct Cmd* next;
} Cmd;

static FblcValue* LookupVar(Scope* scope, FblcName name);
static Scope* AddVar(Scope* scope, FblcName name, FblcValue* value);
static void PrintScope(FILE* stream, Scope* scope);
static FblcValue* NewValue(FblcType* type);
static FblcValue* NewUnionValue(FblcType* type, int tag);
static Cmd* MkEval(const FblcExpr* expr, FblcValue** target, Cmd* next);
static Cmd* MkAccess(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next);
static Cmd* MkCond(
    FblcValue* value, FblcExpr* const* choices, FblcValue** target, Cmd* next);
static Cmd* MkScope(Scope* scope, Cmd* next);
static int TagForField(const FblcType* type, FblcName field);

// LookupVar --
//
//   Look up the value of a variable in the given scope.
//
// Inputs:
//   scope - The scope to look in for the variable.
//   name - The name of the variable to lookup.
//
// Returns:
//   The value of the variable with the given name in scope, or NULL if no
//   such variable was found in scope.
//
// Side effects:
//   None.

static FblcValue* LookupVar(Scope* scope, FblcName name)
{
  for ( ; scope != NULL; scope = scope->next) {
    if (FblcNamesEqual(scope->name, name)) {
      return scope->value;
    }
  }
  return NULL;
}

// AddVar --
//   
//   Extend the given scope with a new variable.
//
// Inputs:
//   scope - The scope to extend.
//   name - The name of the variable to add.
//   value - The value of the variable to add.
//
// Returns:
//   A new scope with the new variable and the contents of the given scope.
//
// Side effects:
//   None.

static Scope* AddVar(Scope* scope, FblcName name, FblcValue* value)
{
  Scope* newscope = GC_MALLOC(sizeof(Scope));
  newscope->name = name;
  newscope->value = value;
  newscope->next = scope;
  return newscope;
}

// PrintScope --
//
//   Print a human readable representation of a scope. This is used for
//   debugging and in error messages.
//
// Inputs:
//   stream - The file stream to print the scope to.
//   scope - The scope to print.
//
// Results:
//   None.
//
// Side effects:
//   The scope is printed to the given file stream.  

static void PrintScope(FILE* stream, Scope* scope)
{
  for ( ; scope != NULL; scope = scope->next) {
    fprintf(stream, " %s = ...", scope->name);
    FblcPrintValue(stream, scope->value);
    fprintf(stream, "\n");
  }
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

// MkEval --
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

static Cmd* MkEval(const FblcExpr* expr, FblcValue** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_EVAL;
  cmd->ex.eval.expr = expr;
  cmd->ex.eval.target = target;
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

// MkScope --
//   
//   Create a command to set the scope to a new value.
//
// Inputs:
//   scope - The new value of the scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkScope(Scope* scope, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_SCOPE;
  cmd->ex.scope.scope = scope;
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
    if (FblcNamesEqual(field, type->fieldv[i].name)) {
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
    fprintf(stream, "%s(", type->name);
    for (int i = 0; i < type->fieldc; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcPrintValue(stream, value->fieldv[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_KIND_UNION) {
    fprintf(stream, "%s:%s(", type->name, type->fieldv[value->tag].name);
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
  Scope* scope = NULL;
  Cmd* cmd = MkEval(expr, &result, NULL);
  while (cmd != NULL) {
    switch (cmd->tag) {
      case CMD_EVAL: {
        const FblcExpr* expr = cmd->ex.eval.expr;
        FblcValue** target = cmd->ex.eval.target;
        cmd = cmd->next;
        switch (expr->tag) {
          case FBLC_VAR_EXPR: {
            FblcName var_name = expr->ex.var.name;
            *target = LookupVar(scope, var_name);
            if (*target == NULL) {
              fprintf(stderr, "FATAL: var %s not in scope:\n", var_name);
              PrintScope(stderr, scope);
              abort();
            }
            break;
          }

          case FBLC_APP_EXPR: {
            FblcType* type = FblcLookupType(env, expr->ex.app.func);
            if (type != NULL) {
              if (type->kind == FBLC_KIND_STRUCT) {
                // Create the struct value now, then add commands to evaluate
                // the arguments to fill in the fields with the proper results.
                *target = NewValue(type);
                for (int i = 0; i < type->fieldc; i++) {
                  cmd = MkEval(expr->argv[i], &((*target)->fieldv[i]), cmd);
                }
              } else {
                assert(false && "Invalid kind of type for application");
              }
              break;
            }

            FblcFunc* func = FblcLookupFunc(env, expr->ex.app.func);
            if (func != NULL) {
              // Add to the top of the command list:
              // arg -> ... -> arg -> scope -> body -> (scope) -> ...
              // The results of the arg evaluations will be stored directly in
              // the scope for the scope command.

              // Don't put a scope change if we will immediately 
              // change it back to a different scope. This is important to
              // avoid memory leaks for tail calls.
              if (cmd != NULL && cmd->tag != CMD_SCOPE) {
                cmd = MkScope(scope, cmd);
              }

              cmd = MkEval(func->body, target, cmd);
              cmd = MkScope(NULL, cmd);

              Cmd* scmd = cmd;
              Scope* nscope = NULL;
              for (int i = 0; i < func->argc; i++) {
                nscope = AddVar(nscope, func->argv[i].name, NULL);
                cmd = MkEval(expr->argv[i], &(nscope->value), cmd);
              }
              scmd->ex.scope.scope = nscope;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            // Add to the top of the command list:
            // object -> access -> ...
            cmd = MkAccess(NULL, expr->ex.access.field, target, cmd);
            cmd = MkEval(expr->ex.access.object, &(cmd->ex.access.value), cmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            FblcType* type = FblcLookupType(env, expr->ex.union_.type);
            assert(type != NULL);
            int tag = TagForField(type, expr->ex.union_.field);
            *target = NewUnionValue(type, tag);
            cmd = MkEval(expr->ex.union_.value, &((*target)->fieldv[0]), cmd);
            break;
          }

          case FBLC_LET_EXPR: {
            // Add to the top of the command list:
            // def -> var -> body -> (scope) -> ...

            // No need to pop the variable if we are going to switch to a
            // different scope immediately after anyway.
            if (cmd != NULL && cmd->tag != CMD_SCOPE) {
              cmd = MkScope(scope, cmd);
            }
            cmd = MkEval(expr->ex.let.body, target, cmd);
            cmd = MkScope(AddVar(scope, expr->ex.let.name, NULL), cmd);
            cmd = MkEval(expr->ex.let.def, &(cmd->ex.scope.scope->value), cmd);
            break;
          }

          case FBLC_COND_EXPR: {
            // Add to the top of the command list:
            // select -> cond -> ...
            cmd = MkCond(NULL, expr->argv, target, cmd);
            cmd = MkEval(expr->ex.cond.select, &(cmd->ex.cond.value), cmd);
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
        cmd = MkEval(cmd->ex.cond.choices[value->tag], target, cmd->next);
        break;
      }

      case CMD_SCOPE:
        scope = cmd->ex.scope.scope;
        cmd = cmd->next;
        break;
    }
  }
  return result;
}
