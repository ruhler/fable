// FblcParser.c --
//
//   This file implements routines to parse an Fblc program from a token
//   stream into abstract syntax form.

#include "FblcInternal.h"

// Vector is a helper for dynamically allocating an array of data whose
// size is not known ahead of time.
// 'size' is the number of bytes taken by a single element.
// 'capacity' is the maximum number of elements supported by the current
// allocation of data.
// 'count' is the number of elements currently in use.
// 'data' is where the data is stored.

typedef struct {
  int size;
  int capacity;
  int count;
  void* data;
} Vector;

static void VectorInit(Vector* vector, int size);
static void* VectorAppend(Vector* vector);
static void* VectorFinish(Vector* vector, int* count);

typedef struct FieldList {
  FblcField field;
  struct FieldList* next;
} FieldList;

typedef struct ArgList {
  FblcExpr* arg;
  struct ArgList* next;
} ArgList;

static FieldList* AddField(FieldList* tail);
static ArgList* AddArg(FblcExpr* expr, ArgList* tail);
static void FillFields(int fieldc, FieldList* list, FblcField* fieldv);
static void FillArgs(int argc, ArgList* list, FblcExpr** argv);

static FblcType* NewType(
    const FblcLocName* name, FblcKind kind, int fieldc, FieldList* fields);
static FblcFunc* NewFunc(
    const FblcLocName* name, const FblcLocName* return_type,
    int argc, FieldList* args, FblcExpr* body);
static FblcProc* NewProc(
    const FblcLocName* name, FblcLocName* return_type,
    int portc, FblcPort* ports, int argc, FieldList* args, FblcActn* body);

static int ParseFields(FblcTokenStream* toks, FieldList** plist);
static int ParsePorts(FblcTokenStream* toks, FblcPort** ports);
static int ParseArgs(FblcTokenStream* toks, ArgList** plist);
static FblcExpr* ParseExpr(FblcTokenStream* toks, bool in_stmt);
static FblcActn* ParseActn(FblcTokenStream* toks, bool in_stmt);

// VectorInit --
//
//   Initialize a Vector for allocations.
//
// Inputs:
//   vector - The vector to initialize.
//   size - The size in bytes of a single element of the vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized for allocation.

static void VectorInit(Vector* vector, int size)
{
  vector->size = size;
  vector->capacity = 4;
  vector->count = 0;
  vector->data = GC_MALLOC(vector->capacity * size);
}

// VectorAppend --
//
//   Append an element to a vector.
//
// Inputs:
//   vector - The vector to append an element to.
//
// Results:
//   A pointer to the newly appended element.
//
// Side effects:
//   The size of the vector is expanded by one element. Pointers returned for
//   previous elements may become invalid.

static void* VectorAppend(Vector* vector)
{
  if (vector->count == vector->capacity) {
    vector->capacity *= 2;
    vector->data = GC_REALLOC(vector->data, vector->capacity * vector->size);
  }
  return (void*)((uintptr_t)vector->data + vector->count++ * vector->size);
}

// VectorFinish
//
//   Get the completed data from a vector.
//
// Inputs:
//   vector - The vector that has been completed.
//   count - An out parameter for the final number of elements in the vector.
//
// Results:
//   The element data for the vector.
//
// Side effects:
//   count is updated with the final number of elements in the vector.

static void* VectorFinish(Vector* vector, int* count)
{
  *count = vector->count;
  return GC_REALLOC(vector->data, vector->count * vector->size);
}

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
    int portc, FblcPort* ports, int argc, FieldList* args, FblcActn* body)
{
  FblcProc* proc = GC_MALLOC(sizeof(FblcProc));
  proc->name.name = name->name;
  proc->name.loc = name->loc;
  proc->return_type = return_type;
  proc->body = body;
  proc->portc = portc;
  proc->portv = ports;
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
//   ports - An out parameter that will be set to the list of parsed ports.
//
// Returns:
//   The number of ports parsed or -1 on error.
//
// Side effects:
//   *ports is set to point to a list parsed ports.
//   The token stream is advanced past the tokens describing the fields.
//   In case of an error, an error message is printed to standard error.

static int ParsePorts(FblcTokenStream* toks, FblcPort** ports)
{
  Vector portv;
  VectorInit(&portv, sizeof(FblcPort));
  while (FblcIsToken(toks, FBLC_TOK_NAME)) {
    FblcPort* port = VectorAppend(&portv);

    // Get the type.
    FblcGetNameToken(toks, "type name", &(port->type));

    // Get the polarity.
    if (FblcIsToken(toks, '<')) {
      FblcGetToken(toks, '<');
      if (!FblcGetToken(toks, '~')) {
        return -1;
      }
      port->polarity = FBLC_POLARITY_GET;
    } else if (FblcIsToken(toks, '~')) {
      FblcGetToken(toks, '~');
      if (!FblcGetToken(toks, '>')) {
        return -1;
      }
      port->polarity = FBLC_POLARITY_PUT;
    } else {
      FblcUnexpectedToken(toks, "'<~' or '~>'");
      return -1;
    }

    // Get the name.
    if (!FblcGetNameToken(toks, "port name", &(port->name))) {
      return -1;
    }

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
    }
  }
  int portc;
  *ports = VectorFinish(&portv, &portc);
  return portc;
}

// ParseArgs --
//
//   Parse a list of arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls, conditional
//   expressions, and process calls.
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
      FblcAppExpr* app_expr = GC_MALLOC(sizeof(FblcAppExpr));
      app_expr->tag = FBLC_APP_EXPR;
      app_expr->loc = start.loc;
      app_expr->func.name = start.name;
      app_expr->func.loc = start.loc;
      app_expr->argc = argc;
      app_expr->argv = GC_MALLOC(argc * sizeof(FblcExpr*));
      FillArgs(argc, args, app_expr->argv);
      expr = (FblcExpr*)app_expr;
    } else if (FblcIsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      FblcGetToken(toks, ':');
      FblcUnionExpr* union_expr = GC_MALLOC(sizeof(FblcUnionExpr));
      union_expr->tag = FBLC_UNION_EXPR;
      union_expr->loc = start.loc;
      union_expr->type.name = start.name;
      union_expr->type.loc = start.loc;
      if (!FblcGetNameToken(toks, "field name", &(union_expr->field))) {
        return NULL;
      }
      if (!FblcGetToken(toks, '(')) {
        return NULL;
      }
      union_expr->value = ParseExpr(toks, false);
      if (union_expr->value == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      expr = (FblcExpr*)union_expr;
    } else if (in_stmt && FblcIsToken(toks, FBLC_TOK_NAME)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      FblcLetExpr* let_expr = GC_MALLOC(sizeof(FblcLetExpr));
      let_expr->tag = FBLC_LET_EXPR;
      let_expr->loc = start.loc;
      let_expr->type.name = start.name;
      let_expr->type.loc = start.loc;
      FblcGetNameToken(toks, "variable name", &(let_expr->name));
      if (!FblcGetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(toks, false);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(toks, true);
      if (let_expr->body == NULL) {
        return NULL;
      }

      // Return the expression immediately, because it is already complete.
      return (FblcExpr*)let_expr;
    } else {
      // This is the variable expression: start
      FblcVarExpr* var_expr = GC_MALLOC(sizeof(FblcVarExpr));
      var_expr->tag = FBLC_VAR_EXPR;
      var_expr->loc = start.loc;
      var_expr->name.name = start.name;
      var_expr->name.loc = start.loc;
      expr = (FblcExpr*)var_expr;
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
    FblcCondExpr* cond_expr = GC_MALLOC(sizeof(FblcCondExpr));
    cond_expr->tag = FBLC_COND_EXPR;
    cond_expr->loc = condition->loc;
    cond_expr->select = condition;
    cond_expr->argc = argc;
    cond_expr->argv = GC_MALLOC(argc * sizeof(FblcExpr*));
    FillArgs(argc, args, cond_expr->argv);
    expr = (FblcExpr*)cond_expr;
  } else {
    FblcUnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (FblcIsToken(toks, '.')) {
    FblcGetToken(toks, '.');

    // This is an access expression of the form: <expr>.<field>
    FblcAccessExpr* access_expr = GC_MALLOC(sizeof(FblcAccessExpr));
    access_expr->tag = FBLC_ACCESS_EXPR;
    access_expr->loc = expr->loc;
    access_expr->object = expr;
    if (!FblcGetNameToken(toks, "field name", &(access_expr->field))) {
      return NULL;
    }
    expr = (FblcExpr*)access_expr;
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

    FblcEvalActn* eval_actn = GC_MALLOC(sizeof(FblcEvalActn));
    eval_actn->tag = FBLC_EVAL_ACTN;
    eval_actn->loc = expr->loc;
    eval_actn->expr = expr;
    actn = (FblcActn*)eval_actn;
  } else if (FblcIsToken(toks, FBLC_TOK_NAME)) {
    FblcLocName name;
    FblcGetNameToken(toks, "port, process, or type name", &name);

    if (FblcIsToken(toks, '~')) {
      FblcGetToken(toks, '~');
      if (!FblcGetToken(toks, '(')) {
        return NULL;
      }

      if (FblcIsToken(toks, ')')) {
        FblcGetToken(toks, ')');
        FblcGetActn* get_actn = GC_MALLOC(sizeof(FblcGetActn));
        get_actn->tag = FBLC_GET_ACTN;
        get_actn->loc = name.loc;
        get_actn->port.loc = name.loc;
        get_actn->port.name = name.name;
        actn = (FblcActn*)get_actn;
      } else {
        FblcExpr* expr = ParseExpr(toks, false);
        if (expr == NULL) {
          return NULL;
        }
        if (!FblcGetToken(toks, ')')) {
          return NULL;
        }

        FblcPutActn* put_actn = GC_MALLOC(sizeof(FblcPutActn));
        put_actn->tag = FBLC_PUT_ACTN;
        put_actn->loc = name.loc;
        put_actn->port.loc = name.loc;
        put_actn->port.name = name.name;
        put_actn->expr = expr;
        actn = (FblcActn*)put_actn;
      }
    } else if (FblcIsToken(toks, '(')) {
      FblcGetToken(toks, '(');
      FblcCallActn* call_actn = GC_MALLOC(sizeof(FblcCallActn));
      call_actn->tag = FBLC_CALL_ACTN;
      call_actn->loc = name.loc;
      call_actn->proc.loc = name.loc;
      call_actn->proc.name = name.name;

      Vector ports;
      VectorInit(&ports, sizeof(FblcLocName));
      if (!FblcIsToken(toks, ';')) {
        if (!FblcGetNameToken(toks, "port name", VectorAppend(&ports))) {
          return NULL;
        }

        while (FblcIsToken(toks, ',')) {
          FblcGetToken(toks, ',');
          if (!FblcGetNameToken(toks, "port name", VectorAppend(&ports))) {
            return NULL;
          }
        }
      }
      call_actn->ports = VectorFinish(&ports, &(call_actn->portc));

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      ArgList* args = NULL;
      int exprc = ParseArgs(toks, &args);
      if (exprc < 0) {
        return NULL;
      }
      FblcExpr** exprs = GC_MALLOC(sizeof(FblcExpr*) * exprc);
      call_actn->exprc = exprc;
      FillArgs(exprc, args, exprs);
      call_actn->exprs = exprs;
      actn = (FblcActn*)call_actn;
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
      FblcLinkActn* link_actn = GC_MALLOC(sizeof(FblcLinkActn));
      link_actn->tag = FBLC_LINK_ACTN;
      link_actn->loc = name.loc;
      link_actn->type = name;
      link_actn->getname = getname;
      link_actn->putname = putname;
      link_actn->body = body;
      return (FblcActn*)link_actn;
    } else if (in_stmt && FblcIsToken(toks, FBLC_TOK_NAME)) {
      FblcExecActn* exec_actn = GC_MALLOC(sizeof(FblcExecActn));
      exec_actn->tag = FBLC_EXEC_ACTN;
      exec_actn->loc = name.loc;

      Vector execs;
      VectorInit(&execs, sizeof(FblcExec));
      bool first = true;
      do {
        FblcExec* exec = VectorAppend(&execs);
        if (first) {
          exec->var.type.loc = name.loc;
          exec->var.type.name = name.name;
          first = false;
        } else {
          if (!FblcGetToken(toks, ',')) {
            return NULL;
          }

          if (!FblcGetNameToken(toks, "type name", &(exec->var.type))) {
            return NULL;
          }
        }

        if (!FblcGetNameToken(toks, "variable name", &(exec->var.name))) {
          return NULL;
        }

        if (!FblcGetToken(toks, '=')) {
          return NULL;
        }

        exec->actn = ParseActn(toks, false);
        if (exec->actn == NULL) {
          return NULL;
        }
      } while (FblcIsToken(toks, ','));
      exec_actn->execv = VectorFinish(&execs, &(exec_actn->execc));

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(toks, true);
      if (exec_actn->body == NULL) {
        return NULL;
      }
      return (FblcActn*)exec_actn;
    } else {
      FblcUnexpectedToken(toks, "The rest of a process starting with a name.");
      return NULL;
    }
  } else if (FblcIsToken(toks, '?')) {
    // ?(<expr> ; <proc>, ...)
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

    Vector args;
    VectorInit(&args, sizeof(FblcActn*));
    bool first = true;
    do {
      if (!first && !FblcGetToken(toks, ',')) {
        return NULL;
      }
      first = false;

      FblcActn** arg = VectorAppend(&args);
      *arg = ParseActn(toks, false);
      if (*arg == NULL) {
        return NULL;
      }
    } while (FblcIsToken(toks, ','));

    if (!FblcGetToken(toks, ')')) {
      return NULL;
    }
    FblcCondActn* cond_actn = GC_MALLOC(sizeof(FblcCondActn));
    cond_actn->tag = FBLC_COND_ACTN;
    cond_actn->loc = condition->loc;
    cond_actn->select = condition;
    cond_actn->args = VectorFinish(&args, &(cond_actn->argc));
    actn = (FblcActn*)cond_actn;
  } else {
    FblcUnexpectedToken(toks, "a process action");
    return NULL;
  }

  if (in_stmt) {
    if (!FblcGetToken(toks, ';')) {
      return NULL;
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
      // Proc declarations end with: ... <ports> ; <fields>; [<type>]) <proc>;
      FblcPort* ports;
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
      FblcProc* proc = NewProc(
          &name, return_type, portc, ports, fieldc, fields, body);
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
