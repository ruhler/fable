// FblcParser.c --
//
//   This file implements routines to parse an Fblc program from a token
//   stream into abstract syntax form.

#include "FblcInternal.h"

static int ParseFields(FblcAllocator* alloc, FblcTokenStream* toks,
    FblcField** plist);
static int ParsePorts(FblcAllocator* alloc, FblcTokenStream* toks,
    FblcPort** ports);
static int ParseArgs(FblcAllocator* alloc, FblcTokenStream* toks,
    FblcExpr*** plist);
static FblcExpr* ParseExpr(FblcAllocator* alloc, FblcTokenStream* toks,
    bool in_stmt);
static FblcActn* ParseActn(FblcAllocator* alloc, FblcTokenStream* toks,
    bool in_stmt);

// ParseFields -
//
//   Parse fields in the form: <type> <name>, <type> <name>, ...
//   This is used for parsing the fields of a struct or union type, and for
//   parsing function input parameters.
//
// Inputs:
//   alloc - The allocator to use for allocations.
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

static int ParseFields(FblcAllocator* alloc, FblcTokenStream* toks,
    FblcField** plist)
{
  if (!FblcIsNameToken(toks)) {
    *plist = NULL;
    return 0;
  }

  FblcVector fields;
  FblcVectorInit(alloc, &fields, sizeof(FblcField));
  FblcField* field = FblcVectorAppend(&fields);
  FblcGetNameToken(toks, "type name", &(field->type));
  if (!FblcGetNameToken(toks, "field name", &(field->name))) {
    return -1;
  }

  int parsed;
  for (parsed = 1; FblcIsToken(toks, ','); parsed++) {
    FblcGetToken(toks, ',');

    field = FblcVectorAppend(&fields);
    if (!FblcGetNameToken(toks, "type name", &(field->type))) {
      return -1;
    }
    if (!FblcGetNameToken(toks, "field name", &(field->name))) {
      return -1;
    }
  }

  int fieldc;
  *plist = FblcVectorExtract(&fields, &fieldc);
  return fieldc;
}

// ParsePorts -
//
//   Parse ports in the form:
//      <type> <polarity> <name>, <type> <polarity> <name>, ...
//   This is used for parsing the process input port parameters.
//
// Inputs:
//   alloc - The allocator to use for allocations.
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

static int ParsePorts(FblcAllocator* alloc, FblcTokenStream* toks,
    FblcPort** ports)
{
  FblcVector portv;
  FblcVectorInit(alloc, &portv, sizeof(FblcPort));
  while (FblcIsNameToken(toks)) {
    FblcPort* port = FblcVectorAppend(&portv);

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
  *ports = FblcVectorExtract(&portv, &portc);
  return portc;
}

// ParseArgs --
//
//   Parse a list of arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls, conditional
//   expressions, and process calls.
//
// Inputs:
//   alloc - The allocator to use for allocations.
//   toks - The token stream to parse the arguments from.
//   plist - An out parameter that will be set to the list of parsed args.
//
// Returns:
//   The number of arguments parsed or -1 on error.
//
// Side effects:
//   *plist is set to point to the parsed arguments.
//   The token stream is advanced past the final ')' token in the
//   argument list. In case of an error, an error message is printed to
//   standard error.

static int ParseArgs(FblcAllocator* alloc, FblcTokenStream* toks,
    FblcExpr*** plist)
{
  FblcVector args;
  FblcVectorInit(alloc, &args, sizeof(FblcExpr*));
  while (!FblcIsToken(toks, ')')) {
    FblcExpr* arg = ParseExpr(alloc, toks, false);
    if (arg == NULL) {
      return -1;
    }
    *((FblcExpr**)FblcVectorAppend(&args)) = arg;

    if (FblcIsToken(toks, ',')) {
      FblcGetToken(toks, ',');
    }
  }
  FblcGetToken(toks, ')');
  int argc;
  *plist = FblcVectorExtract(&args, &argc);
  return argc;
}

// ParseExpr --
//
//   Parse an expression from the token stream.
//   As complete an expression as can be will be parsed.
//   If in_stmt is true, the expression is parsed in a statement context,
//   otherwise the expression must be standalone.
//
// Inputs:
//   alloc - The allocator to use for allocations.
//   toks - The token stream to parse the expression from.
//   in_stmt - True if parsing an expression in the statement context.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.

static FblcExpr* ParseExpr(FblcAllocator* alloc, FblcTokenStream* toks,
    bool in_stmt)
{
  FblcExpr* expr;
  if (FblcIsToken(toks, '{')) {
    FblcGetToken(toks, '{');
    expr = ParseExpr(alloc, toks, true);
    if (expr == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, '}')) {
      return NULL;
    }
  } else if (FblcIsNameToken(toks)) {
    FblcLocName start;
    FblcGetNameToken(toks, "start of expression", &start);

    if (FblcIsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      FblcGetToken(toks, '(');

      FblcExpr** args = NULL;
      int argc = ParseArgs(alloc, toks, &args);
      if (argc < 0) {
        return NULL;
      }
      FblcAppExpr* app_expr = FblcAlloc(alloc, sizeof(FblcAppExpr));
      app_expr->tag = FBLC_APP_EXPR;
      app_expr->loc = start.loc;
      app_expr->func.name = start.name;
      app_expr->func.loc = start.loc;
      app_expr->argc = argc;
      app_expr->argv = args;
      expr = (FblcExpr*)app_expr;
    } else if (FblcIsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      FblcGetToken(toks, ':');
      FblcUnionExpr* union_expr = FblcAlloc(alloc, sizeof(FblcUnionExpr));
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
      union_expr->value = ParseExpr(alloc, toks, false);
      if (union_expr->value == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      expr = (FblcExpr*)union_expr;
    } else if (in_stmt && FblcIsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      FblcLetExpr* let_expr = FblcAlloc(alloc, sizeof(FblcLetExpr));
      let_expr->tag = FBLC_LET_EXPR;
      let_expr->loc = start.loc;
      let_expr->type.name = start.name;
      let_expr->type.loc = start.loc;
      FblcGetNameToken(toks, "variable name", &(let_expr->name));
      if (!FblcGetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(alloc, toks, false);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(alloc, toks, true);
      if (let_expr->body == NULL) {
        return NULL;
      }

      // Return the expression immediately, because it is already complete.
      return (FblcExpr*)let_expr;
    } else {
      // This is the variable expression: start
      FblcVarExpr* var_expr = FblcAlloc(alloc, sizeof(FblcVarExpr));
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
    FblcExpr* condition = ParseExpr(alloc, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }
    
    FblcExpr** args = NULL;
    int argc = ParseArgs(alloc, toks, &args);
    if (argc < 0) {
      return NULL;
    }
    FblcCondExpr* cond_expr = FblcAlloc(alloc, sizeof(FblcCondExpr));
    cond_expr->tag = FBLC_COND_EXPR;
    cond_expr->loc = condition->loc;
    cond_expr->select = condition;
    cond_expr->argc = argc;
    cond_expr->argv = args;
    expr = (FblcExpr*)cond_expr;
  } else {
    FblcUnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (FblcIsToken(toks, '.')) {
    FblcGetToken(toks, '.');

    // This is an access expression of the form: <expr>.<field>
    FblcAccessExpr* access_expr = FblcAlloc(alloc, sizeof(FblcAccessExpr));
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
//   alloc - The allocator to use for allocations.
//   toks - The token stream to parse the action from.
//   in_stmt - True if parsing an action in the statement context.
//
// Returns:
//   The parsed process action, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed action. In case of error,
//   an error message is printed to standard error.

static FblcActn* ParseActn(FblcAllocator* alloc, FblcTokenStream* toks,
    bool in_stmt)
{
  FblcActn* actn = NULL;
  if (FblcIsToken(toks, '{')) {
    FblcGetToken(toks, '{');
    actn = ParseActn(alloc, toks, true);
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
    FblcExpr* expr = ParseExpr(alloc, toks, false);
    if (expr == NULL) {
      return NULL;
    }
    if (!FblcGetToken(toks, ')')) {
      return NULL;
    }

    FblcEvalActn* eval_actn = FblcAlloc(alloc, sizeof(FblcEvalActn));
    eval_actn->tag = FBLC_EVAL_ACTN;
    eval_actn->loc = expr->loc;
    eval_actn->expr = expr;
    actn = (FblcActn*)eval_actn;
  } else if (FblcIsNameToken(toks)) {
    FblcLocName name;
    FblcGetNameToken(toks, "port, process, or type name", &name);

    if (FblcIsToken(toks, '~')) {
      FblcGetToken(toks, '~');
      if (!FblcGetToken(toks, '(')) {
        return NULL;
      }

      if (FblcIsToken(toks, ')')) {
        FblcGetToken(toks, ')');
        FblcGetActn* get_actn = FblcAlloc(alloc, sizeof(FblcGetActn));
        get_actn->tag = FBLC_GET_ACTN;
        get_actn->loc = name.loc;
        get_actn->port.loc = name.loc;
        get_actn->port.name = name.name;
        actn = (FblcActn*)get_actn;
      } else {
        FblcExpr* expr = ParseExpr(alloc, toks, false);
        if (expr == NULL) {
          return NULL;
        }
        if (!FblcGetToken(toks, ')')) {
          return NULL;
        }

        FblcPutActn* put_actn = FblcAlloc(alloc, sizeof(FblcPutActn));
        put_actn->tag = FBLC_PUT_ACTN;
        put_actn->loc = name.loc;
        put_actn->port.loc = name.loc;
        put_actn->port.name = name.name;
        put_actn->expr = expr;
        actn = (FblcActn*)put_actn;
      }
    } else if (FblcIsToken(toks, '(')) {
      FblcGetToken(toks, '(');
      FblcCallActn* call_actn = FblcAlloc(alloc, sizeof(FblcCallActn));
      call_actn->tag = FBLC_CALL_ACTN;
      call_actn->loc = name.loc;
      call_actn->proc.loc = name.loc;
      call_actn->proc.name = name.name;

      FblcVector ports;
      FblcVectorInit(alloc, &ports, sizeof(FblcLocName));
      if (!FblcIsToken(toks, ';')) {
        if (!FblcGetNameToken(toks, "port name", FblcVectorAppend(&ports))) {
          return NULL;
        }

        while (FblcIsToken(toks, ',')) {
          FblcGetToken(toks, ',');
          if (!FblcGetNameToken(toks, "port name", FblcVectorAppend(&ports))) {
            return NULL;
          }
        }
      }
      call_actn->ports = FblcVectorExtract(&ports, &(call_actn->portc));

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      FblcExpr** args = NULL;
      int exprc = ParseArgs(alloc, toks, &args);
      if (exprc < 0) {
        return NULL;
      }
      call_actn->exprc = exprc;
      call_actn->exprs = args;
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
      FblcActn* body = ParseActn(alloc, toks, true);
      if (body == NULL) {
        return NULL;
      }
      FblcLinkActn* link_actn = FblcAlloc(alloc, sizeof(FblcLinkActn));
      link_actn->tag = FBLC_LINK_ACTN;
      link_actn->loc = name.loc;
      link_actn->type = name;
      link_actn->getname = getname;
      link_actn->putname = putname;
      link_actn->body = body;
      return (FblcActn*)link_actn;
    } else if (in_stmt && FblcIsNameToken(toks)) {
      FblcExecActn* exec_actn = FblcAlloc(alloc, sizeof(FblcExecActn));
      exec_actn->tag = FBLC_EXEC_ACTN;
      exec_actn->loc = name.loc;

      FblcVector execs;
      FblcVectorInit(alloc, &execs, sizeof(FblcExec));
      bool first = true;
      do {
        FblcExec* exec = FblcVectorAppend(&execs);
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

        exec->actn = ParseActn(alloc, toks, false);
        if (exec->actn == NULL) {
          return NULL;
        }
      } while (FblcIsToken(toks, ','));
      exec_actn->execv = FblcVectorExtract(&execs, &(exec_actn->execc));

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(alloc, toks, true);
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
    FblcExpr* condition = ParseExpr(alloc, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!FblcGetToken(toks, ';')) {
      return NULL;
    }

    FblcVector args;
    FblcVectorInit(alloc, &args, sizeof(FblcActn*));
    bool first = true;
    do {
      if (!first && !FblcGetToken(toks, ',')) {
        return NULL;
      }
      first = false;

      FblcActn** arg = FblcVectorAppend(&args);
      *arg = ParseActn(alloc, toks, false);
      if (*arg == NULL) {
        return NULL;
      }
    } while (FblcIsToken(toks, ','));

    if (!FblcGetToken(toks, ')')) {
      return NULL;
    }
    FblcCondActn* cond_actn = FblcAlloc(alloc, sizeof(FblcCondActn));
    cond_actn->tag = FBLC_COND_ACTN;
    cond_actn->loc = condition->loc;
    cond_actn->select = condition;
    cond_actn->args = FblcVectorExtract(&args, &(cond_actn->argc));
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
//   alloc - An allocator to use for allocating the program environment.
//   toks - The token stream to parse the program from.
//
// Result:
//   The parsed program environment, or NULL on error.
//
// Side effects:
//   A program environment is allocated. The token stream is advanced to the
//   end of the stream. In the case of an error, an error message is printed
//   to standard error; the caller is still responsible for freeing (unused)
//   allocations made with the allocator in this case.

FblcEnv* FblcParseProgram(FblcAllocator* alloc, FblcTokenStream* toks)
{
  const char* keywords = "'struct', 'union', 'func', or 'proc'";
  FblcEnv* env = FblcNewEnv(alloc);
  while (!FblcIsEOFToken(toks)) {
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
      FblcType* type = FblcAlloc(alloc, sizeof(FblcType));
      type->name.name = name.name;
      type->name.loc = name.loc;
      type->kind = is_struct ? FBLC_KIND_STRUCT : FBLC_KIND_UNION;
      type->fieldc = ParseFields(alloc, toks, &(type->fieldv));
      if (type->fieldc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }
      if (!FblcAddType(env, type)) {
        return NULL;
      }
    } else if (is_func) {
      // Function declarations end with: ... <fields>; <type>) <expr>;
      FblcFunc* func = FblcAlloc(alloc, sizeof(FblcFunc));
      func->name.name = name.name;
      func->name.loc = name.loc;
      func->argc = ParseFields(alloc, toks, &(func->argv));
      if (func->argc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      if (!FblcGetNameToken(toks, "type", &(func->return_type))) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }

      func->body = ParseExpr(alloc, toks, false);
      if (func->body == NULL) {
        return NULL;
      }
      if (!FblcAddFunc(env, func)) {
        return NULL;
      }
    } else if (is_proc) {
      // Proc declarations end with: ... <ports> ; <fields>; [<type>]) <proc>;
      FblcProc* proc = FblcAlloc(alloc, sizeof(FblcProc));
      proc->name.name = name.name;
      proc->name.loc = name.loc;
      proc->portc = ParsePorts(alloc, toks, &(proc->portv));
      if (proc->portc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      proc->argc = ParseFields(alloc, toks, &(proc->argv));
      if (proc->argc < 0) {
        return NULL;
      }

      if (!FblcGetToken(toks, ';')) {
        return NULL;
      }

      if (!FblcGetNameToken(toks, "type", &(proc->return_type))) {
        return NULL;
      }

      if (!FblcGetToken(toks, ')')) {
        return NULL;
      }

      proc->body = ParseActn(alloc, toks, false);
      if (proc->body == NULL) {
        return NULL;
      }
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
