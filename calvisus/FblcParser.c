// FblcParser.c --
//
//   This file implements routines to parse an Fblc program from a token
//   stream into abstract syntax form.

#include "FblcInternal.h"

typedef struct FieldList {
  FblcField field;
  struct FieldList* next;
} FieldList;

typedef struct ArgList {
  FblcExpr* arg;
  struct ArgList* next;
} ArgList;

static void FillFields(int num_fields, FieldList* list, FblcField* fields);
static void FillArgs(int num_args, ArgList* list, FblcExpr** args);

static FblcExpr* NewVarExpr(FblcName name);
static FblcExpr* NewAppExpr(FblcName func, int num_args, ArgList* args);
static FblcExpr* NewAccessExpr(const FblcExpr* object, FblcName field);
static FblcExpr* NewUnionExpr(
    FblcName type, FblcName field, const FblcExpr* value);
static FblcExpr* NewLetExpr(
    FblcName type, FblcName name, const FblcExpr* def, const FblcExpr* body);
static FblcExpr* NewCondExpr(
    const FblcExpr* select, int num_args, ArgList* args);

static int ParseFields(FblcTokenStream* toks, FieldList** plist);
static int ParseArgs(FblcTokenStream* toks, ArgList** plist);
static FblcExpr* ParseExprTail(FblcTokenStream* toks, FblcExpr* expr);
static FblcExpr* ParseStmtExpr(FblcTokenStream* toks);
static FblcExpr* ParseNonStmtExpr(FblcTokenStream* toks, FblcName start);
static FblcExpr* ParseExpr(FblcTokenStream* toks);
static FblcExpr* ParseStmt(FblcTokenStream* toks);

// FillFields --
//
//   Fill an array of fields in from a reversed list of fields.
//
// Inputs:
//   num_fields - The number of fields to fill in.
//   list - A list of num_fields fields in reverse order.
//   fields - An array of num_fields fields to fill in.
//
// Return:
//   None.
//
// Side effects:
//   The 'fields' array is overwritten to contain the fields from 'list' in
//   reverse order.

static void FillFields(int num_fields, FieldList* list, FblcField* fields)
{
  for (int i = 0; i < num_fields; i++) {
    assert(list != NULL && "Not enough fields in list.");
    fields[num_fields-1-i].name = list->field.name;
    fields[num_fields-1-i].type = list->field.type;
    list = list->next;
  }
  assert(list == NULL && "Not all fields from list were used.");
}

// FillArgs --
//
//   Fill an array of args in from a reversed list of args.
//
// Inputs:
//   num_args - The number of args to fill in.
//   list - A list of num_args args in reverse order.
//   args - An array of num_args args to fill in.
//
// Return:
//   None.
//
// Side effects:
//   The 'args' array is overwritten to contain the args from 'list' in
//   reverse order.

static void FillArgs(int num_args, ArgList* list, FblcExpr** args)
{
  for (int i = 0; i < num_args; i++) {
    assert(list != NULL && "Not enough args in list.");
    args[num_args-1-i] = list->arg;
    list = list->next;
  }
  assert(list == NULL && "Not all args from list were used");
}

// NewVarExpr --
//
//   Create a new var expression of the form: <name>
//
// Inputs:
//   name - The name of the variable.
//
// Result:
//   The new variable expression.
//
// Side effects:
//   None.

static FblcExpr* NewVarExpr(FblcName name)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_VAR_EXPR;
  expr->ex.var.name = name;
  return expr;
}

// NewAppExpr --
//
//   Create a new app expression of the form: <func>(<args>)
//
// Input:
//   func - The function to apply.
//   num_args - The number of arguments to pass to the function.
//   args - A list of num_arg arguments to pass to the function in reverse order.
//
// Result:
//   The new app expression.
//
// Side effects:
//   None.

static FblcExpr* NewAppExpr(FblcName func, int num_args, ArgList* args)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr) + num_args * sizeof(FblcExpr*));
  expr->tag = FBLC_APP_EXPR;
  expr->ex.app.func= func;
  FillArgs(num_args, args, expr->args);
  return expr;
}

// NewAccessExpr --
//
//   Create a new access expression of the form: <object>.<field>
//
// Input:
//   object - The object of the member access expression.
//   field - The name of the field to access.
//
// Result:
//   The new access expression.
//
// Side effects:
//   None.

static FblcExpr* NewAccessExpr(const FblcExpr* object, FblcName field)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_ACCESS_EXPR;
  expr->ex.access.object = object;
  expr->ex.access.field = field;
  return expr;
}

// NewUnionExpr --
//
//   Create a new union expression of the form: <type>:<field>(<value>)
//
// Inputs:
//   type - The type of union to construct.
//   field - The field of the union to set.
//   value - The value to use for the given field of the union.
//
// Result:
//   The new union expression.
//
// Side effects:
//   None.

static FblcExpr* NewUnionExpr(
    FblcName type, FblcName field, const FblcExpr* value)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_UNION_EXPR;
  expr->ex.union_.type = type;
  expr->ex.union_.field = field;
  expr->ex.union_.value = value;
  return expr;
}

// NewLetExpr --
//
//   Create new let expression of the form: <type> <name> = <def> ; <body>.
//
// Inputs:
//   type - The type of the variable to define.
//   name - The name of the variable to define.
//   def - The value of the variable to define.
//   body - The expression that uses the defined variable.
//
// Results:
//   The new let expression.
//
// Side effects:
//   None.

static FblcExpr* NewLetExpr(
    FblcName type, FblcName name, const FblcExpr* def, const FblcExpr* body)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
  expr->tag = FBLC_LET_EXPR;
  expr->ex.let.type = type;
  expr->ex.let.name = name;
  expr->ex.let.def = def;
  expr->ex.let.body = body;
  return expr;
}

// NewCondExpr --
//
//   Create a new conditional expression of the form: <select>?(<args>)
//
// Inputs:
//   select - The expression to be conditioned on.
//   num_args - The number of choices to select from.
//   args - A list of num_args choices to choose from in reverse order. 
//
// Result:
//   The new conditional expression.
//
// Side effects:
//   None.

static FblcExpr* NewCondExpr(
    const FblcExpr* select, int num_args, ArgList* args)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr) + num_args * sizeof(FblcExpr*));
  expr->tag = FBLC_COND_EXPR;
  expr->ex.cond.select = select;
  FillArgs(num_args, args, expr->args);
  return expr;
}



  
// Parse fields in the form:
//  type name, type name, ...
// Fields are returned in reverse order of the declaration.
// Returns the number of fields parsed, or -1 on error.
static int ParseFields(FblcTokenStream* toks, FieldList** plist) {
  int parsed;
  FieldList* list = NULL;
  for (parsed = 0; FblcIsToken(toks, FBLC_TOK_NAME); parsed++) {
    FieldList* nlist = GC_MALLOC(sizeof(FieldList));
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



static FblcExpr* ParseExpr(FblcTokenStream* toks) {
  if (FblcIsToken(toks, '{')) {
    return ParseStmtExpr(toks);
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    const char* start = FblcGetNameToken(toks, "start of expression");
    return ParseNonStmtExpr(toks, start);
  } else {
    FblcUnexpectedToken(toks, "an expression");
    return NULL;
  }
}

static FblcExpr* ParseStmt(FblcTokenStream* toks) {
  if (FblcIsToken(toks, '{')) {
    FblcExpr* expr = ParseStmtExpr(toks);
    return FblcGetToken(toks, ';') ? expr : NULL;
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    const char* start = FblcGetNameToken(toks, "start of expression");
    if (FblcIsToken(toks, FBLC_TOK_NAME)) {
      // Parse a let expression.
      FblcName var_type = start;
      FblcName var_name = FblcGetNameToken(toks, "variable name");
      if (!FblcGetToken(toks, '=')) {
        return NULL;
      }
      const FblcExpr* def = ParseExpr(toks);
      if (def == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      const FblcExpr* body = ParseStmt(toks);
      if (body == NULL) {
        return NULL;
      }
      return NewLetExpr(var_type, var_name, def, body);
    } else {
      FblcExpr* expr = ParseNonStmtExpr(toks, start);
      return FblcGetToken(toks, ';') ? expr : NULL;
    }
  } else {
    FblcUnexpectedToken(toks, "a statement");
    return NULL;
  }
}

// Parse a list of arguments in the form:
// (<expr>, <expr>, ...)
// Leaves the token stream just after the final ')' character.
// Outputs the list of arguments in reverse order.
// Returns the number of args parsed, or -1 on failure.
static int ParseArgs(FblcTokenStream* toks, ArgList** plist) {
  int parsed;
  ArgList* list = NULL;

  if (!FblcGetToken(toks, '(')) {
    return -1;
  }
  for (parsed = 0; !FblcIsToken(toks, ')'); parsed++) {
    ArgList* nlist = GC_MALLOC(sizeof(ArgList));
    nlist->next = list;
    list = nlist;
    list->arg = ParseExpr(toks);
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





static FblcExpr* ParseExprTail(FblcTokenStream* toks, FblcExpr* expr) {
  while (FblcIsToken(toks, '?') || FblcIsToken(toks, '.')) {
    if (FblcIsToken(toks, '?')) {
      FblcGetToken(toks, '?');
      ArgList* args = NULL;
      int num_args = ParseArgs(toks, &args);
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

static FblcExpr* ParseStmtExpr(FblcTokenStream* toks) {
  if (!FblcGetToken(toks, '{')) {
    return NULL;
  }
  FblcExpr* expr = ParseStmt(toks);
  if (expr == NULL) {
    return NULL;
  }
  if (!FblcGetToken(toks, '}')) {
    return NULL;
  }
  return ParseExprTail(toks, expr);
}

static FblcExpr* ParseNonStmtExpr(FblcTokenStream* toks, FblcName start) {
  FblcExpr* expr = NULL;
  if (FblcIsToken(toks, '(')) {
    // This is an application. Parse the args.
    ArgList* args = NULL;
    int num_args = ParseArgs(toks, &args);
    if (num_args < 0) {
      return NULL;
    }
    expr = NewAppExpr(start, num_args, args);
  } else if (FblcIsToken(toks, ':')) {
    FblcGetToken(toks, ':');
    FblcName field = FblcGetNameToken(toks, "field name");
    if (field == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, '(')) {
      return NULL;
    }
    FblcExpr* value = ParseExpr(toks);
    if (value == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, ')')) {
      return NULL;
    }
    expr = NewUnionExpr(start, field, value);
  } else {
    expr = NewVarExpr(start);
  }
  return ParseExprTail(toks, expr);
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

    FieldList* fields;
    int num_fields = ParseFields(toks, &fields);
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
      FillFields(num_fields, fields, type->fields);
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

      FblcExpr* expr = ParseExpr(toks);
      if (expr == NULL) {
        return NULL;
      }
      FblcFunc* func = GC_MALLOC(sizeof(FblcFunc) + num_fields * sizeof(FblcField));
      func->name = name;
      func->return_type = return_type;
      func->body = expr;
      func->num_args = num_fields;
      FillFields(num_fields, fields, func->args);
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

