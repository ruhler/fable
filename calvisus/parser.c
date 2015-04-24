
#include "parser.h"

#include <stddef.h>
#include <stdlib.h>

#include <gc/gc.h>

#include "toker.h"

typedef struct field_list_t {
  field_t field;
  struct field_list_t* next;
} field_list_t;
  
// Parse fields in the form:
//  type name, type name, ...
// Fields are returned in reverse order of the declaration.
// Returns the number of fields parsed, or -1 on error.
static int parse_fields(toker_t* tin, field_list_t** plist) {
  int parsed;
  field_list_t* list = NULL;
  for (parsed = 0; toker_is(tin, TOK_NAME); parsed++) {
    field_list_t* nlist = GC_MALLOC(sizeof(field_list_t));
    nlist->next = list;
    list = nlist;
    list->field.type = toker_get_name(tin, "type name");
    list->field.name = toker_get_name(tin, "field name");
    if (list->field.name == NULL) {
      return -1;
    }

    if (toker_is(tin, ',')) {
      toker_get(tin, ',');
    }
  }
  *plist = list;
  return parsed;
}

// Put the fields in the given list into the given array, in reverse order of
// the list.
static void fill_fields(int num_fields, field_list_t* list, field_t* fields) {
  for (int i = 0; i < num_fields; i++) {
    fields[num_fields-1-i].name = list->field.name;
    fields[num_fields-1-i].type = list->field.type;
    list = list->next;
  }
}

typedef struct arg_list_t {
  expr_t* arg;
  struct arg_list_t* next;
} arg_list_t;

expr_t* parse_expr(toker_t* tin);

// Parse a list of arguments in the form:
// (<expr>, <expr>, ...)
// Leaves the tokenizer just after the final ')' character.
// Outputs the list of arguments in reverse order.
// Returns the number of args parsed, or -1 on failure.
int parse_args(toker_t* tin, arg_list_t** plist) {
  int parsed;
  arg_list_t* list = NULL;

  if (!toker_get(tin, '(')) {
    return -1;
  }
  for (parsed = 0; !toker_is(tin, ')'); parsed++) {
    arg_list_t* nlist = GC_MALLOC(sizeof(arg_list_t));
    nlist->next = list;
    list = nlist;
    list->arg = parse_expr(tin);
    if (list->arg == NULL) {
      return -1;
    }

    if (toker_is(tin, ',')) {
      toker_get(tin, ',');
    }
  }
  toker_get(tin, ')');
  *plist = list;
  return parsed;
}

// Fill in the given args array with values based on the list of values.
// Fills in the array in reverse order of the list.
void fill_args(int num_args, arg_list_t* list, expr_t** args) {
  for (int i = 0; i < num_args; i++) {
    args[num_args-1-i] = list->arg;
    list = list->next;
  }
}

expr_t* parse_expr(toker_t* tin) {
  expr_t* expr = NULL;
  if (toker_is(tin, '{')) {
    toker_get(tin, '{');
    expr = parse_expr(tin);
    if (!toker_get(tin, '}')) {
      return NULL;
    }
  } else if (toker_is(tin, TOK_NAME)) {
    const char* name = toker_get_name(tin, "start of expression");
    if (toker_is(tin, '(')) {
      // This is an application. Parse the args.
      arg_list_t* args = NULL;
      int num_args = parse_args(tin, &args);
      if (num_args < 0) {
        return NULL;
      }
      app_expr_t* app_expr = GC_MALLOC(sizeof(app_expr_t) + num_args * sizeof(expr_t*));
      app_expr->tag = EXPR_APP;
      app_expr->function = name;
      fill_args(num_args, args, app_expr->args);
      expr = (expr_t*)app_expr;
    } else if (toker_is(tin, ':')) {
      toker_get(tin, ':');
      fname_t field = toker_get_name(tin, "field name");
      if (field == NULL) {
        return NULL;
      }
      if (!toker_get(tin, '(')) {
        return NULL;
      }
      expr_t* value = parse_expr(tin);
      if (value == NULL) {
        return NULL;
      }
      if (!toker_get(tin, ')')) {
        return NULL;
      }
      union_expr_t* union_expr = GC_MALLOC(sizeof(union_expr_t));
      union_expr->tag = EXPR_UNION;
      union_expr->type = name;
      union_expr->field = field;
      union_expr->value = value;
      expr = (expr_t*)union_expr;
    } else if (toker_is(tin, TOK_NAME)) {
      // Parse a let expression.
      let_expr_t* let_expr = GC_MALLOC(sizeof(let_expr_t));
      let_expr->tag = EXPR_LET;
      let_expr->type = name;
      let_expr->name = toker_get_name(tin, "variable name");
      if (!toker_get(tin, '=')) {
        return NULL;
      }
      let_expr->def = parse_expr(tin);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!toker_get(tin, ';')) {
        return NULL;
      }
      let_expr->body = parse_expr(tin);
      if (let_expr->body == NULL) {
        return NULL;
      }
      expr = (expr_t*)let_expr;
    } else {
      // This is a var expression.
      var_expr_t* var_expr = GC_MALLOC(sizeof(var_expr_t));
      var_expr->tag = EXPR_VAR;
      var_expr->name = name;
      expr = (expr_t*)var_expr;
    }
  } else {
    toker_unexpected(tin, "'{' or a name");
    return NULL;
  }

  while (toker_is(tin, '?') || toker_is(tin, '.')) {
    if (toker_is(tin, '?')) {
      toker_get(tin, '?');
      arg_list_t* args = NULL;
      int num_args = parse_args(tin, &args);
      if (num_args < 0) {
        return NULL;
      }
      cond_expr_t* cond_expr = GC_MALLOC(sizeof(cond_expr_t) + num_args * sizeof(expr_t*));
      cond_expr->tag = EXPR_COND;
      cond_expr->select = expr;
      fill_args(num_args, args, cond_expr->choices);
      expr = (expr_t*)cond_expr;
    } else {
      toker_get(tin, '.');
      fname_t field = toker_get_name(tin, "field name");
      if (field == NULL) {
        return NULL;
      }
      access_expr_t* access_expr = GC_MALLOC(sizeof(access_expr_t));
      access_expr->tag = EXPR_ACCESS;
      access_expr->arg = expr;
      access_expr->field = field;
      expr = (expr_t*) access_expr;
    }
  }
  return expr;
}

env_t* parse(toker_t* tin) {
  type_env_t* tenv = NULL;
  func_env_t* fenv = NULL;

  while (!toker_is(tin, TOK_EOF)) {
    const char* dkind = toker_get_name(tin, "'struct', 'union', or 'func'");
    if (dkind == NULL) {
      return NULL;
    }

    dname_t name = toker_get_name(tin, "declaration name");
    if (name == NULL) {
      return NULL;
    }

    if (!toker_get(tin, '(')) {
      return NULL;
    }

    field_list_t* fields;
    int num_fields = parse_fields(tin, &fields);
    if (num_fields < 0) {
      return NULL;
    }

    if (name_eq("struct", dkind) || name_eq("union", dkind)) {
      if (!toker_get(tin, ')')) {
        return NULL;
      }
      kind_t kind = name_eq("struct", dkind) ? KIND_STRUCT : KIND_UNION;
      type_t* type = GC_MALLOC(sizeof(type_t) + num_fields * sizeof(field_t));
      type->name = name;
      type->kind = kind;
      type->num_fields = num_fields;
      fill_fields(num_fields, fields, type->fields);

      type_env_t* ntenv = GC_MALLOC(sizeof(type_env_t));
      ntenv->decl = type;
      ntenv->next = tenv;
      tenv = ntenv;
    } else if (name_eq("func", dkind)) {
      if (!toker_get(tin, ';')) {
        return NULL;
      }

      dname_t rtype = toker_get_name(tin, "type name");
      if (rtype == NULL) {
        return NULL;
      }

      if (!toker_get(tin, ')')) {
        return NULL;
      }

      expr_t* expr = parse_expr(tin);
      if (expr == NULL) {
        return NULL;
      }
      func_t* func = GC_MALLOC(sizeof(func_t) + num_fields * sizeof(field_t));
      func->name = name;
      func->rtype = rtype;
      func->body = expr;
      func->num_args = num_fields;
      fill_fields(num_fields, fields, func->args);

      func_env_t* nfenv = GC_MALLOC(sizeof(func_env_t));
      nfenv->decl = func;
      nfenv->next = fenv;
      fenv = nfenv;
    } else {
      fprintf(stderr, "Expected 'struct', 'union', or 'func', but got %s\n", dkind);
      return NULL;
    }

    if (!toker_get(tin, ';')) {
      return NULL;
    }
  }

  env_t* env = GC_MALLOC(sizeof(env_t));
  env->types = tenv;
  env->funcs = fenv;
  return env;
}

