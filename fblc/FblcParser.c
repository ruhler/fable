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

static FieldList* AddField(FblcName type, FblcName name, FieldList* tail);
static ArgList* AddArg(FblcExpr* expr, ArgList* tail);
static void FillFields(int fieldc, FieldList* list, FblcField* fieldv);
static void FillArgs(int argc, ArgList* list, FblcExpr** argv);

static FblcExpr* NewVarExpr(FblcName name);
static FblcExpr* NewAppExpr(FblcName func, int argc, ArgList* args);
static FblcExpr* NewAccessExpr(const FblcExpr* object, FblcName field);
static FblcExpr* NewUnionExpr(
    FblcName type, FblcName field, const FblcExpr* value);
static FblcExpr* NewLetExpr(
    FblcName type, FblcName name, const FblcExpr* def, const FblcExpr* body);
static FblcExpr* NewCondExpr(
    const FblcExpr* select, int argc, ArgList* args);
static FblcType* NewType(
    FblcName name, FblcKind kind, int fieldc, FieldList* fields);
static FblcFunc* NewFunc(
    FblcName name, FblcName return_type,
    int argc, FieldList* args, FblcExpr* body);

static int ParseFields(FblcTokenStream* toks, FieldList** plist);
static int ParseArgs(FblcTokenStream* toks, ArgList** plist);
static FblcExpr* ParseExprTail(FblcTokenStream* toks, FblcExpr* expr);
static FblcExpr* ParseStmtExpr(FblcTokenStream* toks);
static FblcExpr* ParseNonStmtExpr(FblcTokenStream* toks, FblcName start);
static FblcExpr* ParseExpr(FblcTokenStream* toks);
static FblcExpr* ParseStmt(FblcTokenStream* toks);

// AddField --
//
//   Add a field to the given list of fields.
//
// Inputs:
//   type - The type of the field to add.
//   name - The name of the field to add.
//   tail - An initial list of fields.
//
// Result:
//   A new list starting with the added field and followed by the tail list of
//   fields.
//
// Side effects:
//   None.

static FieldList* AddField(FblcName type, FblcName name, FieldList* tail)
{
    FieldList* list = GC_MALLOC(sizeof(FieldList));
    list->field.type = type;
    list->field.name = name;
    list->next = tail;
    return list;
}

// AddArg --
//
//   Add an argument to the given list of arguments.
//
// Inputs:
//   arg - The argument to add.
//   tail - An initial list of arguments.
//
// Result:
//   A new list starting with the added argument and followed by the tail list
//   of arguments.
//
// Side effects:
//   None.

static ArgList* AddArg(FblcExpr* arg, ArgList* tail)
{
    ArgList* list = GC_MALLOC(sizeof(ArgList));
    list->arg = arg;
    list->next = tail;
    return list;
}

// FillFields --
//
//   Fill an array of fields in from a reversed list of fields.
//
// Inputs:
//   fieldc - The number of fields to fill in.
//   list - A list of fieldc fields in reverse order.
//   fieldv - An array of fieldc fields to fill in.
//
// Return:
//   None.
//
// Side effects:
//   The 'fieldv' array is overwritten to contain the fields from 'list' in
//   reverse order.

static void FillFields(int fieldc, FieldList* list, FblcField* fieldv)
{
  for (int i = 0; i < fieldc; i++) {
    assert(list != NULL && "Not enough fields in list.");
    fieldv[fieldc-1-i].name = list->field.name;
    fieldv[fieldc-1-i].type = list->field.type;
    list = list->next;
  }
  assert(list == NULL && "Not all fields from list were used.");
}

// FillArgs --
//
//   Fill an array of args in from a reversed list of args.
//
// Inputs:
//   argc - The number of args to fill in.
//   list - A list of argc args in reverse order.
//   argv - An array of argc args to fill in.
//
// Return:
//   None.
//
// Side effects:
//   The 'argv' array is overwritten to contain the args from 'list' in
//   reverse order.

static void FillArgs(int argc, ArgList* list, FblcExpr** argv)
{
  for (int i = 0; i < argc; i++) {
    assert(list != NULL && "Not enough args in list.");
    argv[argc-1-i] = list->arg;
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
//   Create a new app expression of the form: <func>(<argv>)
//
// Input:
//   func - The function to apply.
//   argc - The number of arguments to pass to the function.
//   argv - A list of argc arguments to pass to the function in reverse order.
//
// Result:
//   The new app expression.
//
// Side effects:
//   None.

static FblcExpr* NewAppExpr(FblcName func, int argc, ArgList* args)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr) + argc * sizeof(FblcExpr*));
  expr->tag = FBLC_APP_EXPR;
  expr->ex.app.func= func;
  FillArgs(argc, args, expr->argv);
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
//   Create a new conditional expression of the form: <select>?(<argv>)
//
// Inputs:
//   select - The expression to be conditioned on.
//   argc - The number of choices to select from.
//   args - A list of argc choices to choose from in reverse order. 
//
// Result:
//   The new conditional expression.
//
// Side effects:
//   None.

static FblcExpr* NewCondExpr(
    const FblcExpr* select, int argc, ArgList* args)
{
  FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr) + argc * sizeof(FblcExpr*));
  expr->tag = FBLC_COND_EXPR;
  expr->ex.cond.select = select;
  FillArgs(argc, args, expr->argv);
  return expr;
}

// NewType --
//
//   Create a new type declaration of the form: <kind> <name>(<fields>);
//
// Inputs:
//   name - The name of the type declaration.
//   kind - The kind of the type declaration.
//   fieldc - The number of fields in the type declaration.
//   fields - A list of the fieldc fields of the type declaration in
//            reverse order.
//
// Result:
//   The new type declaration.
//
// Side effects:
//   None.

static FblcType* NewType(
    FblcName name, FblcKind kind, int fieldc, FieldList* fields)
{
  FblcType* type = GC_MALLOC(sizeof(FblcType) + fieldc * sizeof(FblcField));
  type->name = name;
  type->kind = kind;
  type->fieldc = fieldc;
  FillFields(fieldc, fields, type->fieldv);
  return type;
}

// NewType --
//
//   Create a new function declaration of the form:
//     <name>(<args> ; <type>) <expr>;
//
// Inputs:
//   name - The name of the function being declared.
//   return_type - The return type of the function.
//   argc - The number of args in the function declaration.
//   args - A list of the argc args of the function declaration in reverse
//          order.
//   body - The body of the function.
//
// Result:
//   The new function declaration.
//
// Side effects:
//   None.

static FblcFunc* NewFunc(
    FblcName name, FblcName return_type,
    int argc, FieldList* args, FblcExpr* body)
{
  FblcFunc* func = GC_MALLOC(sizeof(FblcFunc) + argc * sizeof(FblcField));
  func->name = name;
  func->return_type = return_type;
  func->body = body;
  func->argc = argc;
  FillFields(argc, args, func->argv);
  return func;
}

// ParseFields -
//
//   Parse fields in the form: <type> <name>, <type> <name>, ...
//   This is used for parsing the fields of a struct or union type, and for
//   parsing function input parameters.
//
// Inputs:
//   toks - The token stream to parse the fields from.
//   plist - A pointer to a list of fields to output the parsed fields to.
//
// Returns:
//   The number of fields parsed or -1 on error.
//
// Side effects:
//   *plist is set to point to a list of the fields parsed in reverse order.
//   The token stream is advanced past the tokens describing the fields.
//   In case of an error, an error message is printed to standard error.

static int ParseFields(FblcTokenStream* toks, FieldList** plist)
{
  int parsed;
  FieldList* list = NULL;
  for (parsed = 0; FblcIsToken(toks, FBLC_TOK_NAME); parsed++) {
    FblcName type = FblcGetNameToken(toks, "type name");
    FblcName name = FblcGetNameToken(toks, "field name");
    if (name == NULL) {
      return -1;
    }
    list = AddField(type, name, list);

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
    }
  }
  *plist = list;
  return parsed;
}

// ParseArgs --
//
//   Parse a list of arguments in the form: (<expr>, <expr>, ...)
//   This is used for parsing arguments to function calls and conditional
//   expressions.
//
// Inputs:
//   toks - The token stream to parse the arguments from.
//   plist - A pointer to a list of arguments to output the parsed args to.
//
// Returns:
//   The number of arguments parsed or -1 on error.
//
// Side effects:
//   *plist is ste to point to a list of the arguments parsed in reverse
//   order. The token stream is advanced past the final ')' token in the
//   argument list. In case of an error, an error message is printed to
//   standard error.

static int ParseArgs(FblcTokenStream* toks, ArgList** plist)
{
  int parsed;
  ArgList* list = NULL;
  if (!FblcGetToken(toks, '(')) {
    return -1;
  }

  for (parsed = 0; !FblcIsToken(toks, ')'); parsed++) {
    FblcExpr* arg = ParseExpr(toks);
    if (arg == NULL) {
      return -1;
    }
    list = AddArg(arg, list);

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
    }
  }
  FblcGetToken(toks, ')');
  *plist = list;
  return parsed;
}

// ParseExprTail --
//
//   An expression can be extended to another expression if the first
//   expression is the object of a field access or a conditional expression:
//      expr ::= <expr>.field | <expr>?(...) | ...
//   This function takes a first expression and extends it as much as possible
//   with field accesses and conditional expressions until as complete an
//   expression as there is has been parsed..
//
// Inputs:
//   toks - The token stream to parse the tail of the expression from.
//   expr - The initial expression to extend.
//
// Result:
//   The complete parsed expression, or NULL if there is an error. This may be
//   the same as the given expression if the given expression is not the
//   object of any field access or conditional expressions.
//
// Side effects:
//   Advances the token stream past the parsed expression. If there is an
//   error, an error message will be printed to standard error.

static FblcExpr* ParseExprTail(FblcTokenStream* toks, FblcExpr* expr)
{
  while (FblcIsToken(toks, '?') || FblcIsToken(toks, '.')) {
    if (FblcIsToken(toks, '?')) {
      FblcGetToken(toks, '?');
      ArgList* args = NULL;
      int argc = ParseArgs(toks, &args);
      if (argc < 0) {
        return NULL;
      }
      expr = NewCondExpr(expr, argc, args);
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

// ParseStmtExpr --
//
//   Parse an expression beginning with a statement expression
//   of the form: { <stmt> }
//   The statement expression may be extended with field access or conditional
//   expressions, in which case the complete, extended expression is parsed.
//
// Inputs:
//   toks - The token stream to parse the expression from.
//
// Result:
//   The parsed expression, or NULL if there is an error.
//
// Side effects:
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.

static FblcExpr* ParseStmtExpr(FblcTokenStream* toks)
{
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

// ParseNonStmtExpr --
//
//   Parse an expression beginning with the form: <name>
//   As complete an expression as can be will be parsed.
//
// Inputs:
//   toks - The token stream to parse the expression from, positioned after
//          the starting name token of the expression.
//   start - The value of the starting name token of the expression.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.

static FblcExpr* ParseNonStmtExpr(FblcTokenStream* toks, FblcName start)
{
  FblcExpr* expr = NULL;
  if (FblcIsToken(toks, '(')) {
    // This is an application expression of the form: start(<args>)
    ArgList* args = NULL;
    int argc = ParseArgs(toks, &args);
    if (argc < 0) {
      return NULL;
    }
    expr = NewAppExpr(start, argc, args);
  } else if (FblcIsToken(toks, ':')) {
    // This is a union expression of the form: start:field(<expr>)
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
    // This is the variable expression: start
    expr = NewVarExpr(start);
  }
  return ParseExprTail(toks, expr);
}

// ParseExpr --
//
//   Parse an expression from the token stream.
//   As complete an expression as can be will be parsed.
//
// Inputs:
//   toks - The token stream to parse the expression from.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.

static FblcExpr* ParseExpr(FblcTokenStream* toks)
{
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

// ParseStmt --
//
//   Parse a statement from the token stream. As complete a statement as can
//   be will be parsed. Note that there is no FblcStmt type, because the
//   notion of a statement is an artifact of the concrete syntax. In the
//   abstract syntax expressions and statements are the same thing.
//
// Inputs:
//   toks - The token stream to parse the statement from.
//
// Returns:
//   The parsed statement, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed statement. In case of error,
//   an error message is printed to standard error.

static FblcExpr* ParseStmt(FblcTokenStream* toks)
{
  if (FblcIsToken(toks, '{')) {
    // This is a statement expression of the form: { <expr> };
    FblcExpr* expr = ParseStmtExpr(toks);
    return FblcGetToken(toks, ';') ? expr : NULL;
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    const char* start = FblcGetNameToken(toks, "start of expression");
    if (FblcIsToken(toks, FBLC_TOK_NAME)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
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
      // This is an expression starting with a name of the form: <expr>;
      FblcExpr* expr = ParseNonStmtExpr(toks, start);
      return FblcGetToken(toks, ';') ? expr : NULL;
    }
  } else {
    FblcUnexpectedToken(toks, "a statement");
    return NULL;
  }
}

// FblcParseProgram --
//
//   Parse an Fblc program from the token stream.
//
// Inputs:
//   toks - The token stream to parse the program from.
//
// Result:
//   The parsed program environment, or NULL on error.
//
// Side effects:
//   The token stream is advanced to the end of the stream. In the case of an
//   error, an error message is printed to standard error.

FblcEnv* FblcParseProgram(FblcTokenStream* toks)
{
  const char* keywords = "'struct', 'union', or 'func'";
  FblcEnv* env = FblcNewEnv();
  while (!FblcIsToken(toks, FBLC_TOK_EOF)) {
    // All declarations start with the form: <keyword> <name> (<fields> ...
    FblcName keyword = FblcGetNameToken(toks, keywords);
    if (keyword == NULL) {
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
    int fieldc = ParseFields(toks, &fields);
    if (fieldc < 0) {
      return NULL;
    }

    bool is_struct = FblcNamesEqual("struct", keyword);
    bool is_union = FblcNamesEqual("union", keyword);
    bool is_func = FblcNamesEqual("func", keyword);

    if (is_struct || is_union) {
      // Struct and union declarations end with: ...);
      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      FblcKind kind =  is_struct ? FBLC_KIND_STRUCT : FBLC_KIND_UNION;
      FblcType* type = NewType(name, kind, fieldc, fields);
      FblcAddType(env, type);
    } else if (is_func) {
      // Function declarations end with: ...; <type>) <expr>;
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FblcName return_type = FblcGetNameToken(toks, "type");
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
      FblcFunc* func = NewFunc(name, return_type, fieldc, fields, expr);
      FblcAddFunc(env, func);
    } else {
      fprintf(stderr, "Expected %s, but got %s.\n", keywords, keyword);
      return NULL;
    }

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
  }
  return env;
}
