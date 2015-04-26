
#include "FblcInternal.h"

// For struct value, 'tag' is unused and 'fields' contains the field data.
// For union value, 'tag' is the index of the active field and 'fields' is a
// single element array with the field value.
struct FblcValue {
  FblcType* type;
  int tag;
  struct FblcValue* fields[];
};

typedef struct scope_t {
  FblcName name;
  FblcValue* value;
  struct scope_t* next;
} scope_t;

FblcValue* lookup_var(scope_t* scope, FblcName name);
scope_t* extend(scope_t* scope, FblcName name, FblcValue* value);

void dump_scope(FILE* fout, scope_t* scope);

FblcValue* lookup_var(scope_t* scope, FblcName name) {
  if (scope == NULL) {
    return NULL;
  } else if (FblcNamesEqual(scope->name, name)) {
    return scope->value;
  } else {
    return lookup_var(scope->next, name);
  }
}

scope_t* extend(scope_t* scope, FblcName name, FblcValue* value) {
  scope_t* newscope = GC_MALLOC(sizeof(scope_t));
  newscope->name = name;
  newscope->value = value;
  newscope->next = scope;
  return newscope;
}

void dump_scope(FILE* fout, scope_t* scope) {
  for ( ; scope != NULL; scope = scope->next) {
    fprintf(fout, " %s = ...\n", scope->name);
  }
}



typedef enum {
  CMD_EVAL, CMD_ACCESS, CMD_COND, CMD_VAR, CMD_DEVAR, CMD_SCOPE
} cmd_tag_t;

typedef struct cmd_t {
  cmd_tag_t tag;
  union {
    struct { const FblcExpr* expr; FblcValue** target; } eval;
    struct { FblcValue* value; FblcName field; FblcValue** target; } access;
    struct { FblcValue* value; FblcExpr* const* choices; FblcValue** target; } cond;
    struct { FblcName name; FblcValue* value; } var;
    struct { scope_t* scope; } scope;
  } data;
  struct cmd_t* next;
} cmd_t;

FblcValue* NewValue(FblcType* type) {
  int fields = type->num_fields;
  if (type->kind == FBLC_KIND_UNION) {
    fields = 1;
  }
  FblcValue* value = GC_MALLOC(sizeof(FblcValue) + fields * sizeof(FblcValue*));
  value->type = type;
  return value;
}

FblcValue* NewUnionValue(FblcType* type, int tag) {
  FblcValue* value = NewValue(type);
  value->tag = tag;
  return value;
}

void FblcPrintValue(FILE* fout, FblcValue* value) {
  FblcType* type = value->type;
  if (type->kind == FBLC_KIND_STRUCT) {
    fprintf(fout, "%s(", type->name);
    for (int i = 0; i < type->num_fields; i++) {
      if (i > 0) {
        fprintf(fout, ",");
      }
      FblcPrintValue(fout, value->fields[i]);
    }
    fprintf(fout, ")");
    return;
  }
  
  if (type->kind == FBLC_KIND_UNION) {
    fprintf(fout, "%s:%s(", type->name, type->fields[value->tag].name);
    FblcPrintValue(fout, value->fields[0]);
    fprintf(fout, ")");
    return;
  }

  assert(false && "Invalid Kind");
}

static cmd_t* mk_eval(const FblcExpr* expr, FblcValue** target, cmd_t* next) {
  cmd_t* cmd = GC_MALLOC(sizeof(cmd_t));
  cmd->tag = CMD_EVAL;
  cmd->data.eval.expr = expr;
  cmd->data.eval.target = target;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_access(FblcValue* value, FblcName field, FblcValue** target, cmd_t* next) {
  cmd_t* cmd = GC_MALLOC(sizeof(cmd_t));
  cmd->tag = CMD_ACCESS;
  cmd->data.access.value = value;
  cmd->data.access.field = field;
  cmd->data.access.target = target;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_var(FblcName name, FblcValue* value, cmd_t* next) {
  cmd_t* cmd = GC_MALLOC(sizeof(cmd_t));
  cmd->tag = CMD_VAR;
  cmd->data.var.name = name;
  cmd->data.var.value = value;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_devar(cmd_t* next) {
  cmd_t* cmd = GC_MALLOC(sizeof(cmd_t));
  cmd->tag = CMD_DEVAR;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_cond(FblcValue* value, FblcExpr* const* choices, FblcValue** target, cmd_t* next) {
  cmd_t* cmd = GC_MALLOC(sizeof(cmd_t));
  cmd->tag = CMD_COND;
  cmd->data.cond.value = value;
  cmd->data.cond.choices = choices;
  cmd->data.cond.target = target;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_scope(scope_t* scope, cmd_t* next) {
  cmd_t* cmd = GC_MALLOC(sizeof(cmd_t));
  cmd->tag = CMD_SCOPE;
  cmd->data.scope.scope = scope;
  cmd->next = next;
  return cmd;
}

static int indexof(const FblcType* type, FblcName field) {
  for (int i = 0; i < type->num_fields; i++) {
    if (FblcNamesEqual(field, type->fields[i].name)) {
      return i;
    }
  }
  assert(false && "No such field.");
}

FblcValue* FblcEvaluate(const FblcEnv* env, const FblcExpr* expr) {
  FblcValue* result = NULL;
  scope_t* scope = NULL;
  cmd_t* cmd = mk_eval(expr, &result, NULL);
  while (cmd != NULL) {
    switch (cmd->tag) {
      case CMD_EVAL: {
        const FblcExpr* expr = cmd->data.eval.expr;
        FblcValue** target = cmd->data.eval.target;
        cmd = cmd->next;
        switch (expr->tag) {
          case FBLC_VAR_EXPR: {
            FblcName var_name = expr->ex.var.name;
            *target = lookup_var(scope, var_name);
            if (*target == NULL) {
              fprintf(stderr, "FATAL: var %s not in scope:\n", var_name);
              dump_scope(stderr, scope);
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
                  cmd = mk_eval(expr->args[i], &((*target)->fields[i]), cmd);
                }
              } else {
                assert(false && "Invalid kind of type for application");
              }
              break;
            }

            FblcFunc* func = FblcLookupFunc(env, expr->ex.app.func);
            if (func != NULL) {
              // Add to the top of the command list
              // arg -> ... -> arg -> scope -> body -> (scope) -> ...

              // Don't put a scope change if we will immediately 
              // change it back to a different scope. This is important to
              // avoid memory leaks for tail calls.
              if (cmd != NULL && cmd->tag != CMD_SCOPE) {
                cmd = mk_scope(scope, cmd);
              }

              cmd = mk_eval(func->body, target, cmd);
              cmd = mk_scope(NULL, cmd);

              cmd_t* scmd = cmd;
              scope_t* nscope = NULL;
              for (int i = 0; i < func->num_args; i++) {
                nscope = extend(nscope, func->args[i].name, NULL);
                cmd = mk_eval(expr->args[i], &(nscope->value), cmd);
              }
              scmd->data.scope.scope = nscope;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            cmd = mk_access(NULL, expr->ex.access.field, target, cmd);
            cmd = mk_eval(expr->ex.access.object, &(cmd->data.access.value), cmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            FblcType* type = FblcLookupType(env, expr->ex.union_.type);
            assert(type != NULL);
            int tag = indexof(type, expr->ex.union_.field);
            *target = NewUnionValue(type, tag);
            cmd = mk_eval(expr->ex.union_.value, &((*target)->fields[0]), cmd);
            break;
          }

          case FBLC_LET_EXPR: {
            // No need to pop the variable if we are going to switch to a
            // different scope immediately after anyway.
            if (cmd != NULL && cmd->tag != CMD_SCOPE) {
              cmd = mk_devar(cmd);
            }

            cmd = mk_eval(expr->ex.let.body, target, cmd);
            cmd = mk_var(expr->ex.let.name, NULL, cmd);
            cmd = mk_eval(expr->ex.let.def, &(cmd->data.var.value), cmd);
            break;
          }

          case FBLC_COND_EXPR: {
            cmd = mk_cond(NULL, expr->args, target, cmd);
            cmd = mk_eval(expr->ex.cond.select, &(cmd->data.cond.value), cmd);
            break;
          }
        }
        break;
      }

      case CMD_ACCESS: {
        FblcType* type = cmd->data.access.value->type;
        int target_tag = indexof(type, cmd->data.access.field);
        int actual_tag = cmd->data.access.value->tag;
        FblcValue** target = cmd->data.access.target;
        if (type->kind == FBLC_KIND_STRUCT) {
          *target = cmd->data.access.value->fields[target_tag];
        } else if (actual_tag == target_tag) {
          *target = cmd->data.access.value->fields[0];
        } else {
          fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
          abort();
        }
        cmd = cmd->next;
        break;
      }

      case CMD_COND: {
        FblcValue* value = cmd->data.cond.value;
        FblcValue** target = cmd->data.cond.target;
        assert(value->type->kind == FBLC_KIND_UNION);
        cmd = mk_eval(cmd->data.cond.choices[value->tag], target, cmd->next);
        break;
      }

      case CMD_VAR:
        scope = extend(scope, cmd->data.var.name, cmd->data.var.value);
        cmd = cmd->next;
        break;

      case CMD_DEVAR:
        assert(scope != NULL);
        scope = scope->next;
        cmd = cmd->next;
        break;

      case CMD_SCOPE:
        scope = cmd->data.scope.scope;
        cmd = cmd->next;
        break;
    }
  }
  return result;
}

