// FblcEvaluator.c --
//
//   This file implements routines for evaluating Fblc expressions.

#include "FblcInternal.h"

// An Fblc value is represented using the following FblcValue data structure.
// For struct values, 'tag' is unused and 'fields' contains the field data in
// the order the fields are declared in the type declaration.
// For union values, 'tag' is the index of the active field and 'fields' is a
// single element array with the field value.

struct FblcValue {
  FblcType* type;
  int tag;
  struct FblcValue* fields[];
};

typedef struct Scope {
  FblcName name;
  FblcValue* value;
  struct Scope* next;
} Scope;

typedef enum {
  CMD_EVAL, CMD_ACCESS, CMD_COND, CMD_VAR, CMD_DEVAR, CMD_SCOPE
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

    // The var command adds a variable with the given name and value to the
    // current scope.
    // There is a corresponding devar command that has no extra information.
    // The devar command removes the variable most recently added to the
    // current scope.
    struct {
      FblcName name;
      FblcValue* value;
    } var;

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
static Cmd* MkVar(FblcName name, FblcValue* value, Cmd* next);
static Cmd* MkDevar(Cmd* next);
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

static FblcValue* NewValue(FblcType* type)
{
  int fields = type->num_fields;
  if (type->kind == FBLC_KIND_UNION) {
    fields = 1;
  }
  FblcValue* value = GC_MALLOC(sizeof(FblcValue) + fields * sizeof(FblcValue*));
  value->type = type;
  return value;
}

static FblcValue* NewUnionValue(FblcType* type, int tag)
{
  FblcValue* value = NewValue(type);
  value->tag = tag;
  return value;
}

static Cmd* MkEval(const FblcExpr* expr, FblcValue** target, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_EVAL;
  cmd->ex.eval.expr = expr;
  cmd->ex.eval.target = target;
  cmd->next = next;
  return cmd;
}

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

static Cmd* MkVar(FblcName name, FblcValue* value, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_VAR;
  cmd->ex.var.name = name;
  cmd->ex.var.value = value;
  cmd->next = next;
  return cmd;
}

static Cmd* MkDevar(Cmd* next) {
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_DEVAR;
  cmd->next = next;
  return cmd;
}

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

static Cmd* MkScope(Scope* scope, Cmd* next)
{
  Cmd* cmd = GC_MALLOC(sizeof(Cmd));
  cmd->tag = CMD_SCOPE;
  cmd->ex.scope.scope = scope;
  cmd->next = next;
  return cmd;
}

static int TagForField(const FblcType* type, FblcName field)
{
  for (int i = 0; i < type->num_fields; i++) {
    if (FblcNamesEqual(field, type->fields[i].name)) {
      return i;
    }
  }
  assert(false && "No such field.");
}

void FblcPrintValue(FILE* stream, FblcValue* value)
{
  FblcType* type = value->type;
  if (type->kind == FBLC_KIND_STRUCT) {
    fprintf(stream, "%s(", type->name);
    for (int i = 0; i < type->num_fields; i++) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcPrintValue(stream, value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_KIND_UNION) {
    fprintf(stream, "%s:%s(", type->name, type->fields[value->tag].name);
    FblcPrintValue(stream, value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

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
                *target = NewValue(type);
                for (int i = 0; i < type->num_fields; i++) {
                  cmd = MkEval(expr->args[i], &((*target)->fields[i]), cmd);
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
              for (int i = 0; i < func->num_args; i++) {
                nscope = AddVar(nscope, func->args[i].name, NULL);
                cmd = MkEval(expr->args[i], &(nscope->value), cmd);
              }
              scmd->ex.scope.scope = nscope;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            cmd = MkAccess(NULL, expr->ex.access.field, target, cmd);
            cmd = MkEval(expr->ex.access.object, &(cmd->ex.access.value), cmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            FblcType* type = FblcLookupType(env, expr->ex.union_.type);
            assert(type != NULL);
            int tag = TagForField(type, expr->ex.union_.field);
            *target = NewUnionValue(type, tag);
            cmd = MkEval(expr->ex.union_.value, &((*target)->fields[0]), cmd);
            break;
          }

          case FBLC_LET_EXPR: {
            // No need to pop the variable if we are going to switch to a
            // different scope immediately after anyway.
            if (cmd != NULL && cmd->tag != CMD_SCOPE) {
              cmd = MkDevar(cmd);
            }
            cmd = MkEval(expr->ex.let.body, target, cmd);
            cmd = MkVar(expr->ex.let.name, NULL, cmd);
            cmd = MkEval(expr->ex.let.def, &(cmd->ex.var.value), cmd);
            break;
          }

          case FBLC_COND_EXPR: {
            cmd = MkCond(NULL, expr->args, target, cmd);
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
          *target = cmd->ex.access.value->fields[target_tag];
        } else if (actual_tag == target_tag) {
          *target = cmd->ex.access.value->fields[0];
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

      case CMD_VAR:
        scope = AddVar(scope, cmd->ex.var.name, cmd->ex.var.value);
        cmd = cmd->next;
        break;

      case CMD_DEVAR:
        assert(scope != NULL);
        scope = scope->next;
        cmd = cmd->next;
        break;

      case CMD_SCOPE:
        scope = cmd->ex.scope.scope;
        cmd = cmd->next;
        break;
    }
  }
  return result;
}
