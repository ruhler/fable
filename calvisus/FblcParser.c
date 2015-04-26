
#include "FblcInternal.h"

typedef struct field_list_t {
  FblcField field;
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
static void fill_fields(int num_fields, field_list_t* list, FblcField* fields) {
  for (int i = 0; i < num_fields; i++) {
    fields[num_fields-1-i].name = list->field.name;
    fields[num_fields-1-i].type = list->field.type;
    list = list->next;
  }
}

typedef struct arg_list_t {
  FblcExpr* arg;
  struct arg_list_t* next;
} arg_list_t;

static FblcExpr* parse_expr(FblcTokenStream* toks);

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
void fill_args(int num_args, arg_list_t* list, FblcExpr** args) {
  for (int i = 0; i < num_args; i++) {
    args[num_args-1-i] = list->arg;
    list = list->next;
  }
}

static FblcExpr* NewAppExpr(FblcName func, int num_args, arg_list_t* args) {
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr) + num_args * sizeof(FblcExpr*));
  expr->tag = FBLC_APP_EXPR;
  expr->ex.app.func= func;
  fill_args(num_args, args, expr->args);
  return expr;
}

static FblcExpr* NewUnionExpr(FblcName type, FblcName field, const FblcExpr* value) {
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_UNION_EXPR;
  expr->ex.union_.type = type;
  expr->ex.union_.field = field;
  expr->ex.union_.value = value;
  return expr;
}

static FblcExpr* NewLetExpr(FblcName type, FblcName name, const FblcExpr* def,
    const FblcExpr* body) {
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_LET_EXPR;
  expr->ex.let.type = type;
  expr->ex.let.name = name;
  expr->ex.let.def = def;
  expr->ex.let.body = body;
  return expr;
}

static FblcExpr* NewVarExpr(FblcName name) {
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_VAR_EXPR;
  expr->ex.var.name = name;
  return expr;
}

static FblcExpr* NewCondExpr(const FblcExpr* select, int num_args, arg_list_t* args) {
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr) + num_args * sizeof(FblcExpr*));
  expr->tag = FBLC_COND_EXPR;
  expr->ex.cond.select = select;
  fill_args(num_args, args, expr->args);
  return expr;
}

static FblcExpr* NewAccessExpr(const FblcExpr* object, FblcName field) {
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_ACCESS_EXPR;
  expr->ex.access.object = object;
  expr->ex.access.field = field;
  return expr;
}

static FblcExpr* parse_expr(FblcTokenStream* toks) {
  FblcExpr* expr = NULL;
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
      FblcExpr* value = parse_expr(toks);
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
      const FblcExpr* def = parse_expr(toks);
      if (def == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      const FblcExpr* body = parse_expr(toks);
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

FblcEnv* FblcParseProgram(FblcTokenStream* toks) {
  FblcEnv* env = FblcNewEnv();
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
      FblcKind kind = FblcNamesEqual("struct", dkind) ? FBLC_KIND_STRUCT : FBLC_KIND_UNION;
      FblcType* type = GC_MALLOC(sizeof(FblcType) + num_fields * sizeof(FblcField));
      type->name = name;
      type->kind = kind;
      type->num_fields = num_fields;
      fill_fields(num_fields, fields, type->fields);
      FblcAddType(env, type);
    } else if (FblcNamesEqual("func", dkind)) {
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FblcName return_type = FblcGetNameToken(toks, "type name");
      if (return_type == NULL) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }

      FblcExpr* expr = parse_expr(toks);
      if (expr == NULL) {
        return NULL;
      }
      FblcFunc* func = GC_MALLOC(sizeof(FblcFunc) + num_fields * sizeof(FblcField));
      func->name = name;
      func->return_type = return_type;
      func->body = expr;
      func->num_args = num_fields;
      fill_fields(num_fields, fields, func->args);
      FblcAddFunc(env, func);
    } else {
      fprintf(stderr, "Expected 'struct', 'union', or 'func', but got %s\n", dkind);
      return NULL;
    }

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
  }
  return env;
}

