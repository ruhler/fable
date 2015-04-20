
#include "eval.h"

#include <stdlib.h>

typedef enum {
  CMD_EVAL, CMD_ACCESS, CMD_COND, CMD_VAR, CMD_DEVAR, CMD_SCOPE
} cmd_tag_t;

typedef struct cmd_t {
  cmd_tag_t tag;
  union {
    struct { expr_t* expr; value_t** target; } eval;
    struct { value_t* value; int index; value_t** target; } access;
    struct { value_t* value; expr_t** choices; value_t** target; } cond;
    struct { vname_t name; value_t* value; } var;
    struct { } devar;
    struct { scope_t* scope; } scope;
  } data;
  struct cmd_t* next;
} cmd_t;

static cmd_t* mk_eval(expr_t* expr, value_t** target, cmd_t* next) {
  cmd_t* cmd = malloc(sizeof(cmd_t));
  cmd->tag = CMD_EVAL;
  cmd->data.eval.expr = expr;
  cmd->data.eval.target = target;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_access(value_t* value, int index, value_t** target, cmd_t* next) {
  cmd_t* cmd = malloc(sizeof(cmd_t));
  cmd->tag = CMD_ACCESS;
  cmd->data.access.value = value;
  cmd->data.access.index = index;
  cmd->data.access.target = target;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_var(vname_t name, value_t* value, cmd_t* next) {
  cmd_t* cmd = malloc(sizeof(cmd_t));
  cmd->tag = CMD_VAR;
  cmd->data.var.name = name;
  cmd->data.var.value = value;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_devar(cmd_t* next) {
  cmd_t* cmd = malloc(sizeof(cmd_t));
  cmd->tag = CMD_DEVAR;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_cond(value_t* value, expr_t** choices, value_t** target, cmd_t* next) {
  cmd_t* cmd = malloc(sizeof(cmd_t));
  cmd->tag = CMD_COND;
  cmd->data.cond.value = value;
  cmd->data.cond.choices = choices;
  cmd->data.cond.target = target;
  cmd->next = next;
  return cmd;
}

static cmd_t* mk_scope(scope_t* scope, cmd_t* next) {
  cmd_t* cmd = malloc(sizeof(cmd_t));
  cmd->tag = CMD_SCOPE;
  cmd->data.scope.scope = scope;
  cmd->next = next;
  return cmd;
}

value_t* eval(const env_t* env, scope_t* scope, const expr_t* expr) {
  value_t* result = NULL;
  cmd_t* cmd = mk_eval(expr, &result, NULL);
  while (cmd != NULL) {
    switch (cmd->tag) {
      case CMD_EVAL: {
        expr_t* expr = cmd->data.expr;
        value_t** target = cmd->data.eval.target;
        cmd = cmd->next;
        switch (expr->tag) {
          case EXPR_VAR:
            var_expr_t* var_expr = (var_expr_t*) expr;
            *target = lookup_var(scope, var_expr->name);
            break;

          case EXPR_APP: {
            app_expr_t* app_expr = (app_expr_t*) expr;
            type_t* type = lookup_type(env, app_expr->function);
            if (type != NULL) {
              if (type->kind == KIND_STRUCT) {
                *target = mk_struct(type->num_fields);
                for (int i = 0; i < type->num_fields; i++) {
                  cmd = mk_eval(app_expr->args[i], &((*target)->fields[i]), cmd);
                }
              } else {
                assert(false && "Invalid kind of type for application");
              }
              break;
            }

            func_t* func = lookup_func(env, app_expr->function);
            if (func != NULL) {
              // Don't put a scope change if we will immediately 
              // change it back to a different scope. This is important to
              // avoid memory leaks for tail calls.
              if (cmd != NULL && cmd->tag != CMD_SCOPE) {
                cmd = mk_scope(scope, cmd->next);
              }

              cmd = mk_eval(func->body, target, cmd);
              cmd = mk_scope(NULL, cmd);

              cmd_t* scmd = cmd;
              scope_t* nscope = NULL;
              for (int i = 0; i < func->num_args; i++) {
                nscope = extend(nscope, func->args[i].name, NULL);
                cmd = mk_eval(app_expr->args[i], &(nscope->value), cmd);
              }
              cmd->data.scope.scope = nscope;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case EXPR_ACCESS: {
            access_expr_t* access_expr = (access_expr_t*)expr;
            // TODO: we need to know the type of arg to figure out the
            // proper index to do member access!
            type_t* type = lookup_type(env, access_expr->???);
            assert(type != NULL);
            int index = indexof(type, access_expr->field);
            assert(index >= 0);
            cmd = mk_access(NULL, index, target, cmd);
            cmd = mk_eval(access_expr->arg, &(cmd->data.access.value), cmd);
            break;
          }

          case EXPR_UNION:
            union_expr_t* union_expr = (union_expr_t*)expr;
            type_t* type = lookup_type(env, union_expr->type);
            assert(type != NULL);
            int index = indexof(type, union_expr->field);
            *target = mk_union(field);
            cmd = mk_eval(union_expr->value, &((*target)->fields[0]), cmd);
            break;

          case EXPR_LET: {
            let_expr_t* let_expr = (let_expr_t*)expr;
            cmd = mk_devar(cmd);
            cmd = mk_eval(let_expr->body, target, cmd);
            cmd = mk_var(let_expr->name, NULL, cmd);
            cmd = mk_eval(let_expr->def, &(cmd->data.var.value), cmd);
            break;
          }

          case EXPR_COND: {
            cond_expr_t* cond_expr = (cond_expr_t*)expr;
            cmd = mk_cond(NULL, cond_expr->choices, target, cmd);
            cmd = mk_eval(cond_expr->select, &(cmd->data.cond.value), cmd);
            break;
          }
        }
        break;
      }

      case CMD_ACCESS: {
        int index = cmd->data.access.index;
        int field = cmd->data.access.value->field;
        value_t** target = cmd->data.access.target;
        if (field == FIELD_STRUCT) {
          *target = cmd->data.access.value->fields[index];
        } else if (field == index) {
          *target = cmd->data.access.value->fields[0];
        } else {
          fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
          abort();
        }
        break;
      }

      case CMD_COND: {
        value_t* value = cmd->data.cond.value;
        value_t** target = cmd->data.cond.target;
        assert(value->field != FIELD_STRUCT);
        *target = choices[value->field];
        break;
      }

      case CMD_VAR:
        scope = extend(scope, cmd->data.var.name, cmd->data.var.value);
        break;

      case CMD_DEVAR:
        assert(scope != NULL);
        scope = scope->next;
        break;

      case CMD_SCOPE:
        scope = cmd->data.scope.scope;
        break;
    }
  }
  return result;
}

