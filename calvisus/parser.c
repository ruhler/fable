
#include "FblcInternal.h"
#include "parser.h"

typedef struct field_list_t {
  field_t field;
  struct field_list_t* next;
} field_list_t;
  
// Parse fields in the form:
//  type name, type name, ...
// Fields are returned in reverse order of the declaration.
// Returns the number of fields parsed, or -1 on error.
static int parse_fields(FblcTokenStream* toks, field_list_t** plist) {
  int parsed;
  field_list_t* list = NULL;
  for (parsed = 0; FblcIsToken(toks, FBLC_TOK_NAME); parsed++) {
    field_list_t* nlist = GC_MALLOC(sizeof(field_list_t));
    nlist->next = list;
    list = nlist;
    list->field.type = FblcGetNameToken(toks, "type name");
    list->field.name = FblcGetNameToken(toks, "field name");
    if (list->field.name == NULL) {
      return -1;
    }

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
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

expr_t* parse_expr(FblcTokenStream* toks);

// Parse a list of arguments in the form:
// (<expr>, <expr>, ...)
// Leaves the token stream just after the final ')' character.
// Outputs the list of arguments in reverse order.
// Returns the number of args parsed, or -1 on failure.
int parse_args(FblcTokenStream* toks, arg_list_t** plist) {
  int parsed;
  arg_list_t* list = NULL;

  if (!FblcGetToken(toks, '(')) {
    return -1;
  }
  for (parsed = 0; !FblcIsToken(toks, ')'); parsed++) {
    arg_list_t* nlist = GC_MALLOC(sizeof(arg_list_t));
    nlist->next = list;
    list = nlist;
    list->arg = parse_expr(toks);
    if (list->arg == NULL) {
      return -1;
    }

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
    }
  }
  FblcGetToken(toks, ')');
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

static expr_t* NewAppExpr(FblcName function, int num_args, arg_list_t* args) {
  expr_t* expr = GC_MALLOC(sizeof(expr_t) + num_args * sizeof(expr_t*));
  expr->tag = EXPR_APP;
  expr->ex.app.function = function;
  fill_args(num_args, args, expr->args);
  return expr;
}

static expr_t* NewUnionExpr(FblcName type, FblcName field, const expr_t* value) {
  expr_t* expr = GC_MALLOC(sizeof(expr_t));
  expr->tag = EXPR_UNION;
  expr->ex.union_.type = type;
  expr->ex.union_.field = field;
  expr->ex.union_.value = value;
  return expr;
}

static expr_t* NewLetExpr(FblcName type, FblcName name, const expr_t* def,
    const expr_t* body) {
  expr_t* expr = GC_MALLOC(sizeof(expr_t));
  expr->tag = EXPR_LET;
  expr->ex.let.type = type;
  expr->ex.let.name = name;
  expr->ex.let.def = def;
  expr->ex.let.body = body;
  return expr;
}

static expr_t* NewVarExpr(FblcName name) {
  expr_t* expr = GC_MALLOC(sizeof(expr_t));
  expr->tag = EXPR_VAR;
  expr->ex.var.name = name;
  return expr;
}

static expr_t* NewCondExpr(const expr_t* select, int num_args, arg_list_t* args) {
  expr_t* expr = GC_MALLOC(sizeof(expr_t) + num_args * sizeof(expr_t*));
  expr->tag = EXPR_COND;
  expr->ex.cond.select = select;
  fill_args(num_args, args, expr->args);
  return expr;
}

static expr_t* NewAccessExpr(const expr_t* arg, FblcName field) {
  expr_t* expr = GC_MALLOC(sizeof(expr_t));
  expr->tag = EXPR_ACCESS;
  expr->ex.access.arg = arg;
  expr->ex.access.field = field;
  return expr;
}

expr_t* parse_expr(FblcTokenStream* toks) {
  expr_t* expr = NULL;
  if (FblcIsToken(toks, '{')) {
    FblcGetToken(toks, '{');
    expr = parse_expr(toks);
    if (expr == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, '}')) {
      return NULL;
    }
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    const char* name = FblcGetNameToken(toks, "start of expression");
    if (FblcIsToken(toks, '(')) {
      // This is an application. Parse the args.
      arg_list_t* args = NULL;
      int num_args = parse_args(toks, &args);
      if (num_args < 0) {
        return NULL;
      }
      expr = NewAppExpr(name, num_args, args);
    } else if (FblcIsToken(toks, ':')) {
      FblcGetToken(toks, ':');
      FblcName field = FblcGetNameToken(toks, "field name");
      if (field == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, '(')) {
        return NULL;
      }
      expr_t* value = parse_expr(toks);
      if (value == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      expr = NewUnionExpr(name, field, value);
    } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
      // Parse a let expression.
      FblcName var_type = name;
      FblcName var_name = FblcGetNameToken(toks, "variable name");
      if (!FblcGetToken(toks, '=')) {
        return NULL;
      }
      const expr_t* def = parse_expr(toks);
      if (def == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      const expr_t* body = parse_expr(toks);
      if (body == NULL) {
        return NULL;
      }
      expr = NewLetExpr(var_type, var_name, def, body);
    } else {
      expr = NewVarExpr(name);
    }
  } else {
    FblcUnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (FblcIsToken(toks, '?') || FblcIsToken(toks, '.')) {
    if (FblcIsToken(toks, '?')) {
      FblcGetToken(toks, '?');
      arg_list_t* args = NULL;
      int num_args = parse_args(toks, &args);
      if (num_args < 0) {
        return NULL;
      }
      expr = NewCondExpr(expr, num_args, args);
    } else {
      FblcGetToken(toks, '.');
      FblcName field = FblcGetNameToken(toks, "field name");
      if (field == NULL) {
        return NULL;
      }
      expr = NewAccessExpr(expr, field);
    }
  }
  return expr;
}

env_t* parse(FblcTokenStream* toks) {
  type_env_t* tenv = NULL;
  func_env_t* fenv = NULL;

  while (!FblcIsToken(toks, FBLC_TOK_EOF)) {
    const char* dkind = FblcGetNameToken(toks, "'struct', 'union', or 'func'");
    if (dkind == NULL) {
      return NULL;
    }

    FblcName name = FblcGetNameToken(toks, "declaration name");
    if (name == NULL) {
      return NULL;
    }

    if (!FblcGetToken(toks, '(')) {
      return NULL;
    }

    field_list_t* fields;
    int num_fields = parse_fields(toks, &fields);
    if (num_fields < 0) {
      return NULL;
    }

    if (FblcNamesEqual("struct", dkind) || FblcNamesEqual("union", dkind)) {
      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      kind_t kind = FblcNamesEqual("struct", dkind) ? KIND_STRUCT : KIND_UNION;
      type_t* type = GC_MALLOC(sizeof(type_t) + num_fields * sizeof(field_t));
      type->name = name;
      type->kind = kind;
      type->num_fields = num_fields;
      fill_fields(num_fields, fields, type->fields);

      type_env_t* ntenv = GC_MALLOC(sizeof(type_env_t));
      ntenv->decl = type;
      ntenv->next = tenv;
      tenv = ntenv;
    } else if (FblcNamesEqual("func", dkind)) {
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FblcName rtype = FblcGetNameToken(toks, "type name");
      if (rtype == NULL) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }

      expr_t* expr = parse_expr(toks);
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

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
  }

  env_t* env = GC_MALLOC(sizeof(env_t));
  env->types = tenv;
  env->funcs = fenv;
  return env;
}

