// FblcParser.c --
//
//   This file implements routines to parse an Fblc program from a token
//   stream into abstract syntax form.

#include "FblcInternal.h"

typedef struct FieldList {
  FblcField field;
  struct FieldList* next;
} FieldList;

typedef struct PortList {
  FblcPort port;
  struct PortList* next;
} PortList;

typedef struct ArgList {
  FblcExpr* arg;
  struct ArgList* next;
} ArgList;

static FieldList* AddField(FieldList* tail);
static PortList* AddPort(PortList* tail);
static ArgList* AddArg(FblcExpr* expr, ArgList* tail);
static void FillFields(int fieldc, FieldList* list, FblcField* fieldv);
static void FillArgs(int argc, ArgList* list, FblcExpr** argv);
static void FillPorts(int portc, PortList* list, FblcPort* portv);

static FblcType* NewType(
    const FblcLocName* name, FblcKind kind, int fieldc, FieldList* fields);
static FblcFunc* NewFunc(
    const FblcLocName* name, const FblcLocName* return_type,
    int argc, FieldList* args, FblcExpr* body);
static FblcProc* NewProc(
    const FblcLocName* name, FblcLocName* return_type,
    int portc, PortList* ports, int argc, FieldList* args, FblcActn* body);

static int ParseFields(FblcTokenStream* toks, FieldList** plist);
static int ParsePorts(FblcTokenStream* toks, PortList** plist);
static int ParseArgs(FblcTokenStream* toks, ArgList** plist);
static FblcExpr* ParseExpr(FblcTokenStream* toks, bool in_stmt);
static FblcActn* ParseActn(FblcTokenStream* toks, bool in_stmt);

// AddField --
//
//   Add a field entry to the given list of fields. The contents of the field
//   entry are left uninitialized.
//
// Inputs:
//   tail - The list to add the field entry to.
//
// Result:
//   A new list starting with the added field entry and followed by the given
//   list of fields.
//
// Side effects:
//   None.

static FieldList* AddField(FieldList* tail)
{
    FieldList* list = GC_MALLOC(sizeof(FieldList));
    list->next = tail;
    return list;
}

// AddPort --
//
//   Add a port entry to the given list of ports. The contents of the port
//   entry are left uninitialized.
//
// Inputs:
//   tail - The list to add the port entry to.
//
// Result:
//   A new list starting with the added port entry and followed by the given
//   list of ports.
//
// Side effects:
//   None.

static PortList* AddPort(PortList* tail)
{
    PortList* list = GC_MALLOC(sizeof(PortList));
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

// FillPorts --
//
//   Fill an array of ports in from a reversed list of ports.
//
// Inputs:
//   portc - The number of ports to fill in.
//   list - A list of portc ports in reverse order.
//   portv - An array of portc ports to fill in.
//
// Return:
//   None.
//
// Side effects:
//   The 'portv' array is overwritten to contain the ports from 'list' in
//   reverse order.

static void FillPorts(int portc, PortList* list, FblcPort* portv)
{
  for (int i = 0; i < portc; i++) {
    assert(list != NULL && "Not enough ports in list.");
    portv[portc-1-i].name = list->port.name;
    portv[portc-1-i].type = list->port.type;
    portv[portc-1-i].polarity = list->port.polarity;
    list = list->next;
  }
  assert(list == NULL && "Not all ports from list were used.");
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
    const FblcLocName* name, FblcKind kind, int fieldc, FieldList* fields)
{
  FblcType* type = GC_MALLOC(sizeof(FblcType) + fieldc * sizeof(FblcField));
  type->name.name = name->name;
  type->name.loc = name->loc;
  type->kind = kind;
  type->fieldc = fieldc;
  FillFields(fieldc, fields, type->fieldv);
  return type;
}

// NewFunc --
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
    const FblcLocName* name, const FblcLocName* return_type,
    int argc, FieldList* args, FblcExpr* body)
{
  FblcFunc* func = GC_MALLOC(sizeof(FblcFunc) + argc * sizeof(FblcField));
  func->name.name = name->name;
  func->name.loc = name->loc;
  func->return_type.name = return_type->name;
  func->return_type.loc = return_type->loc;
  func->body = body;
  func->argc = argc;
  FillFields(argc, args, func->argv);
  return func;
}

// NewProc --
//
//   Create a new process declaration of the form:
//     <name>(<ports> ; <args> ; <type>) <proc>;
//
// Inputs:
//   name - The name of the process being declared.
//   return_type - The return type of the process.
//   portc - The number of ports in the process declaration.
//   ports - A list of the portc ports of the process declaration in reverse
//           order.
//   argc - The number of args in the process declaration.
//   args - A list of the argc args of the process declaration in reverse
//          order.
//   body - The body of the process.
//
// Result:
//   The new process declaration.
//
// Side effects:
//   None.

static FblcProc* NewProc(
    const FblcLocName* name, FblcLocName* return_type,
    int portc, PortList* ports, int argc, FieldList* args, FblcActn* body)
{
  FblcProc* proc = GC_MALLOC(sizeof(FblcProc));
  proc->name.name = name->name;
  proc->name.loc = name->loc;
  proc->return_type = return_type;
  proc->body = body;
  proc->portc = portc;
  proc->portv = GC_MALLOC(portc * sizeof(FblcPort));
  FillPorts(portc, ports, proc->portv);
  proc->argc = argc;
  proc->argv = GC_MALLOC(argc * sizeof(FblcField));
  FillFields(argc, args, proc->argv);
  return proc;
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
  if (!FblcIsToken(toks, FBLC_TOK_NAME)) {
    *plist = NULL;
    return 0;
  }

  FieldList* list = AddField(NULL);
  FblcGetNameToken(toks, "type name", &(list->field.type));
  if (!FblcGetNameToken(toks, "field name", &(list->field.name))) {
    return -1;
  }

  int parsed;
  for (parsed = 1; FblcIsToken(toks, ','); parsed++) {
    FblcGetToken(toks, ',');

    list = AddField(list);
    if (!FblcGetNameToken(toks, "type name", &(list->field.type))) {
      return -1;
    }
    if (!FblcGetNameToken(toks, "field name", &(list->field.name))) {
      return -1;
    }
  }
  *plist = list;
  return parsed;
}

// ParsePorts -
//
//   Parse ports in the form:
//      <type> <polarity> <name>, <type> <polarity> <name>, ...
//   This is used for parsing the process input port parameters.
//
// Inputs:
//   toks - The token stream to parse the fields from.
//   plist - A pointer to a list of ports to output the parsed ports to.
//
// Returns:
//   The number of ports parsed or -1 on error.
//
// Side effects:
//   *plist is set to point to a list of the ports parsed in reverse order.
//   The token stream is advanced past the tokens describing the fields.
//   In case of an error, an error message is printed to standard error.

static int ParsePorts(FblcTokenStream* toks, PortList** plist)
{
  int parsed;
  PortList* list = NULL;
  for (parsed = 0; FblcIsToken(toks, FBLC_TOK_NAME); parsed++) {
    list = AddPort(list);

    // Get the type.
    FblcGetNameToken(toks, "type name", &(list->port.type));

    // Get the polarity.
    if (FblcIsToken(toks, '/')) {
      FblcGetToken(toks, '/');
      list->port.polarity = FBLC_POLARITY_GET;
    } else if (FblcIsToken(toks, '\\')) {
      FblcGetToken(toks, '\\');
      list->port.polarity = FBLC_POLARITY_PUT;
    } else {
      FblcUnexpectedToken(toks, "'/' or '\\'");
      return -1;
    }

    // Get the name.
    if (!FblcGetNameToken(toks, "port name", &(list->port.name))) {
      return -1;
    }

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
    }
  }
  *plist = list;
  return parsed;
}

// ParseArgs --
//
//   Parse a list of arguments in the form: <expr>, <expr>, ...)
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
  for (parsed = 0; !FblcIsToken(toks, ')'); parsed++) {
    FblcExpr* arg = ParseExpr(toks, false);
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

// ParseExpr --
//
//   Parse an expression from the token stream.
//   As complete an expression as can be will be parsed.
//   If in_stmt is true, the expression is parsed in a statement context,
//   otherwise the expression must be standalone.
//
// Inputs:
//   toks - The token stream to parse the expression from.
//   in_stmt - True if parsing an expression in the statement context.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.

static FblcExpr* ParseExpr(FblcTokenStream* toks, bool in_stmt)
{
  FblcExpr* expr;
  if (FblcIsToken(toks, '{')) {
    FblcGetToken(toks, '{');
    expr = ParseExpr(toks, true);
    if (expr == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, '}')) {
      return NULL;
    }
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    FblcLocName start;
    FblcGetNameToken(toks, "start of expression", &start);

    if (FblcIsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      FblcGetToken(toks, '(');

      ArgList* args = NULL;
      int argc = ParseArgs(toks, &args);
      if (argc < 0) {
        return NULL;
      }
      expr = GC_MALLOC(sizeof(FblcExpr) + argc * sizeof(FblcExpr*));
      expr->tag = FBLC_APP_EXPR;
      expr->loc = start.loc;
      expr->ex.app.func.name = start.name;
      expr->ex.app.func.loc = start.loc;
      expr->argc = argc;
      FillArgs(argc, args, expr->argv);
    } else if (FblcIsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      FblcGetToken(toks, ':');
      expr = GC_MALLOC(sizeof(FblcExpr));
      expr->tag = FBLC_UNION_EXPR;
      expr->loc = start.loc;
      expr->ex.union_.type.name = start.name;
      expr->ex.union_.type.loc = start.loc;
      if (!FblcGetNameToken(toks, "field name", &(expr->ex.union_.field))) {
        return NULL;
      }
      if (!FblcGetToken(toks, '(')) {
        return NULL;
      }
      expr->ex.union_.value = ParseExpr(toks, false);
      if (expr->ex.union_.value == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
    } else if (in_stmt && FblcIsToken(toks, FBLC_TOK_NAME)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      FblcExpr* expr = GC_MALLOC(sizeof(FblcExpr));
      expr->tag = FBLC_LET_EXPR;
      expr->loc = start.loc;
      expr->ex.let.type.name = start.name;
      expr->ex.let.type.loc = start.loc;
      FblcGetNameToken(toks, "variable name", &(expr->ex.let.name));
      if (!FblcGetToken(toks, '=')) {
        return NULL;
      }
      expr->ex.let.def = ParseExpr(toks, false);
      if (expr->ex.let.def == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      expr->ex.let.body = ParseExpr(toks, true);
      if (expr->ex.let.body == NULL) {
        return NULL;
      }

      // Return the expression immediately, because it is already complete.
      return expr;
    } else {
      // This is the variable expression: start
      expr = GC_MALLOC(sizeof(FblcExpr));
      expr->tag = FBLC_VAR_EXPR;
      expr->loc = start.loc;
      expr->ex.var.name.name = start.name;
      expr->ex.var.name.loc = start.loc;
    }
  } else if (FblcIsToken(toks, '?')) {
    // This is a conditional expression of the form: ?(<expr> ; <args>)
    FblcGetToken(toks, '?');
    if (!FblcGetToken(toks, '(')) {
      return NULL;
    }
    FblcExpr* condition = ParseExpr(toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
    
    ArgList* args = NULL;
    int argc = ParseArgs(toks, &args);
    if (argc < 0) {
      return NULL;
    }
    expr = GC_MALLOC(sizeof(FblcExpr) + argc * sizeof(FblcExpr*));
    expr->tag = FBLC_COND_EXPR;
    expr->loc = condition->loc;
    expr->ex.cond.select = condition;
    expr->argc = argc;
    FillArgs(argc, args, expr->argv);
  } else {
    FblcUnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (FblcIsToken(toks, '.')) {
    FblcGetToken(toks, '.');

    // This is an access expression of the form: <expr>.<field>
    FblcExpr* object = expr;
    expr = GC_MALLOC(sizeof(FblcExpr));
    expr->tag = FBLC_ACCESS_EXPR;
    expr->loc = expr->loc;
    expr->ex.access.object = object;
    if (!FblcGetNameToken(toks, "field name", &(expr->ex.access.field))) {
      return NULL;
    }
  }

  if (in_stmt) {
    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
  }
  return expr;
}

// ParseActn --
//
//   Parse a process action from the token stream.
//   As complete an action as can be will be parsed.
//   If in_stmt is true, the action is parsed in a statement context,
//   otherwise the action must be standalone.
//
// Inputs:
//   toks - The token stream to parse the action from.
//   in_stmt - True if parsing an action in the statement context.
//
// Returns:
//   The parsed process action, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed action. In case of error,
//   an error message is printed to standard error.

static FblcActn* ParseActn(FblcTokenStream* toks, bool in_stmt)
{
  FblcActn* actn = NULL;
  if (FblcIsToken(toks, '{')) {
    FblcGetToken(toks, '{');
    actn = ParseActn(toks, true);
    if (actn == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, '}')) {
      return NULL;
    }
  } else if (FblcIsToken(toks, '$')) {
    // $(<expr>)
    FblcGetToken(toks, '$');
    if (!FblcGetToken(toks, '(')) {
      return NULL;
    }
    FblcExpr* expr = ParseExpr(toks, false);
    if (expr == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, ')')) {
      return NULL;
    }

    actn = GC_MALLOC(sizeof(FblcActn));
    actn->tag = FBLC_EVAL_ACTN;
    actn->loc = expr->loc;
    actn->ac.eval.expr = expr;
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    FblcLocName name;
    FblcGetNameToken(toks, "port or process name", &name);

    if (FblcIsToken(toks, '~')) {
      FblcGetToken(toks, '~');
      if (!FblcGetToken(toks, '(')) {
        return NULL;
      }

      if (FblcIsToken(toks, ')')) {
        FblcGetToken(toks, ')');
        actn = GC_MALLOC(sizeof(FblcActn));
        actn->tag = FBLC_GET_ACTN;
        actn->loc = name.loc;
        actn->ac.get.port = name;
      } else {
        FblcExpr* expr = ParseExpr(toks, false);
        if (expr == NULL) {
          return NULL;
        }
        if (!FblcGetToken(toks, ')')) {
          return NULL;
        }

        actn = GC_MALLOC(sizeof(FblcActn));
        actn->tag = FBLC_PUT_ACTN;
        actn->loc = name.loc;
        actn->ac.put.port = name;
        actn->ac.put.expr = expr;
      }
    } else if (in_stmt && FblcIsToken(toks, '<')) {
      FblcGetToken(toks, '<');
      if (!FblcGetToken(toks, '~')) {
        return NULL;
      }
      if (!FblcGetToken(toks, '>')) {
        return NULL;
      }
      FblcLocName getname;
      if (!FblcGetNameToken(toks, "port name", &getname)) {
        return NULL;
      }
      if (!FblcGetToken(toks, ',')) {
        return NULL;
      }
      FblcLocName putname;
      if (!FblcGetNameToken(toks, "port name", &putname)) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      FblcActn* body = ParseActn(toks, true);
      if (body == NULL) {
        return NULL;
      }
      actn = GC_MALLOC(sizeof(FblcActn));
      actn->tag = FBLC_LINK_ACTN;
      actn->loc = name.loc;
      actn->ac.link.type = name;
      actn->ac.link.getname = getname;
      actn->ac.link.putname = putname;
      actn->ac.link.body = body;
      return actn;
    } else if (FblcIsToken(toks, '(')) {
      assert(false && "TODO: Parse a call process.");
      return NULL;
    } else if (in_stmt && FblcIsToken(toks, FBLC_TOK_NAME)) {
      assert(false && "TODO: Parse an exec process.");
      return NULL;
    }
  } else if (FblcIsToken(toks, '?')) {
    // ?(<expr> ; <proc>, ...)
    assert(false && "TODO: Parse a conditional process.");
  } else {
    FblcUnexpectedToken(toks, "a process action");
    return NULL;
  }

  if (in_stmt) {
    FblcGetToken(toks, ';');

    // TODO: Support generalized exec actions instead of this special case.
    if (!FblcIsToken(toks, '}')) {
      FblcActn* second = ParseActn(toks, true);
      if (second == NULL) {
        return NULL;
      }

      FblcActn* seq = GC_MALLOC(sizeof(FblcActn));
      seq->tag = FBLC_EXEC_ACTN;
      seq->loc = actn->loc;
      seq->ac.exec.execc = 1;
      seq->ac.exec.execv = GC_MALLOC(sizeof(FblcExec));
      seq->ac.exec.execv->var = NULL;
      seq->ac.exec.execv->proc = actn;
      seq->ac.exec.body = second;
      return seq;
    }
  }
  return actn;
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
  const char* keywords = "'struct', 'union', 'func', or 'proc'";
  FblcEnv* env = FblcNewEnv();
  while (!FblcIsToken(toks, FBLC_TOK_EOF)) {
    // All declarations start with the form: <keyword> <name> (...
    FblcLocName keyword;
    if (!FblcGetNameToken(toks, keywords, &keyword)) {
      return NULL;
    }

    FblcLocName name;
    if (!FblcGetNameToken(toks, "declaration name", &name)) {
      return NULL;
    }

    if (!FblcGetToken(toks, '(')) {
      return NULL;
    }

    bool is_struct = FblcNamesEqual("struct", keyword.name);
    bool is_union = FblcNamesEqual("union", keyword.name);
    bool is_func = FblcNamesEqual("func", keyword.name);
    bool is_proc = FblcNamesEqual("proc", keyword.name);

    if (is_struct || is_union) {
      // Struct and union declarations end with: ... <fields>);
      FieldList* fields;
      int fieldc = ParseFields(toks, &fields);
      if (fieldc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      FblcKind kind =  is_struct ? FBLC_KIND_STRUCT : FBLC_KIND_UNION;
      FblcType* type = NewType(&name, kind, fieldc, fields);
      if (!FblcAddType(env, type)) {
        return NULL;
      }
    } else if (is_func) {
      // Function declarations end with: ... <fields>; <type>) <expr>;
      FieldList* fields;
      int fieldc = ParseFields(toks, &fields);
      if (fieldc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FblcLocName return_type;
      if (!FblcGetNameToken(toks, "type", &return_type)) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }

      FblcExpr* expr = ParseExpr(toks, false);
      if (expr == NULL) {
        return NULL;
      }
      FblcFunc* func = NewFunc(&name, &return_type, fieldc, fields, expr);
      if (!FblcAddFunc(env, func)) {
        return NULL;
      }
    } else if (is_proc) {
      // Process declarations end with: ... <ports> ; <fields>; [<type>]) <proc>;
      PortList* ports;
      int portc = ParsePorts(toks, &ports);
      if (portc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FieldList* fields;
      int fieldc = ParseFields(toks, &fields);
      if (fieldc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FblcLocName* return_type = NULL;
      if (FblcIsToken(toks, FBLC_TOK_NAME)) {
        return_type = GC_MALLOC(sizeof(FblcLocName));
        FblcGetNameToken(toks, "type", return_type);
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }

      FblcActn* body = ParseActn(toks, false);
      if (body == NULL) {
        return NULL;
      }
      FblcProc* proc = NewProc(&name, return_type, portc, ports, fieldc, fields, body);
      if (!FblcAddProc(env, proc)) {
        return NULL;
      }
    } else {
      FblcReportError("Expected %s, but got '%s'.\n",
          keyword.loc, keywords, keyword.name);
      return NULL;
    }

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
  }
  return env;
}
