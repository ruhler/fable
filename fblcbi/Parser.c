// Parser.c --
//
//   This file implements routines to parse an fblc program from a token
//   stream into abstract syntax form.

#include <assert.h>     // for assert

#include "fblct.h"

static int ParseFields(FblcArena* arena, TokenStream* toks, Field** plist);
static int ParsePorts(FblcArena* arena, TokenStream* toks, Port** ports);
static int ParseArgs(FblcArena* arena, TokenStream* toks, Expr*** plist);
static Expr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt);
static Actn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt);

// ParseFields -
//
//   Parse fields in the form: <type> <name>, <type> <name>, ...
//   This is used for parsing the fields of a struct or union type, and for
//   parsing function input parameters.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the fields from.
//   plist - A pointer to a list of fields to output the parsed fields to.
//
// Returns:
//   The number of fields parsed or -1 on error.
//
// Side effects:
//   *plist is set to point to a list of the parsed fields.
//   The token stream is advanced past the tokens describing the fields.
//   In case of an error, an error message is printed to standard error.

static int ParseFields(FblcArena* arena, TokenStream* toks, Field** plist)
{
  if (!IsNameToken(toks)) {
    *plist = NULL;
    return 0;
  }

  Field* fieldv;
  int fieldc;
  FblcVectorInit(arena, fieldv, fieldc);
  FblcVectorExtend(arena, fieldv, fieldc);
  GetNameToken(arena, toks, "type name", &(fieldv[fieldc].type));
  if (!GetNameToken(arena, toks, "field name", &(fieldv[fieldc++].name))) {
    return -1;
  }

  int parsed;
  for (parsed = 1; IsToken(toks, ','); parsed++) {
    GetToken(toks, ',');

    FblcVectorExtend(arena, fieldv, fieldc);
    Field* field = fieldv + fieldc++;
    if (!GetNameToken(arena, toks, "type name", &(field->type))) {
      return -1;
    }
    if (!GetNameToken(arena, toks, "field name", &(field->name))) {
      return -1;
    }
  }

  *plist = fieldv;
  return fieldc;
}

// ParsePorts -
//
//   Parse a list of zero or more ports in the form:
//      <type> <polarity> <name>, <type> <polarity> <name>, ...
//   This is used for parsing the process input port parameters.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the fields from.
//   ports - An out parameter that will be set to the list of parsed ports.
//
// Returns:
//   The number of ports parsed or -1 on error.
//
// Side effects:
//   *ports is set to point to a list parsed ports.
//   The token stream is advanced past the last port token.
//   In case of an error, an error message is printed to standard error.

static int ParsePorts(FblcArena* arena, TokenStream* toks, Port** ports)
{
  if (!IsNameToken(toks)) {
    *ports = NULL;
    return 0;
  }

  Port* portv;
  int portc;
  FblcVectorInit(arena, portv, portc);
  bool first = true;
  while (first || IsToken(toks, ',')) {
    if (first) {
      first = false;
    } else {
      GetToken(toks, ',');
    }

    FblcVectorExtend(arena, portv, portc);
    Port* port = portv + portc++;

    // Get the type.
    if (!GetNameToken(arena, toks, "type name", &(port->type))) {
      return -1;
    }

    // Get the polarity.
    if (IsToken(toks, '<')) {
      GetToken(toks, '<');
      if (!GetToken(toks, '~')) {
        return -1;
      }
      port->polarity = POLARITY_GET;
    } else if (IsToken(toks, '~')) {
      GetToken(toks, '~');
      if (!GetToken(toks, '>')) {
        return -1;
      }
      port->polarity = POLARITY_PUT;
    } else {
      UnexpectedToken(toks, "'<~' or '~>'");
      return -1;
    }

    // Get the name.
    if (!GetNameToken(arena, toks, "port name", &(port->name))) {
      return -1;
    }
  }
  *ports = portv;
  return portc;
}

// ParseArgs --
//
//   Parse a list of zero or more arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls, conditional
//   expressions, and process calls.
//
// Inputs:
//   arena - The arena to use for allocations.
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
static int ParseArgs(FblcArena* arena, TokenStream* toks, Expr*** plist)
{
  Expr** argv;
  size_t argc;
  FblcVectorInit(arena, argv, argc);
  if (!IsToken(toks, ')')) {
    Expr* arg = ParseExpr(arena, toks, false);
    if (arg == NULL) {
      return -1;
    }
    FblcVectorAppend(arena, argv, argc, arg);

    while (IsToken(toks, ',')) {
      GetToken(toks, ',');
      arg = ParseExpr(arena, toks, false);
      if (arg == NULL) {
        return -1;
      }
      FblcVectorAppend(arena, argv, argc, arg);
    }
  }
  if (!GetToken(toks, ')')) {
    return -1;
  }

  *plist = argv;
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
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the expression from.
//   in_stmt - True if parsing an expression in the statement context.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.

static Expr* ParseExpr(FblcArena* arena, TokenStream* toks,
    bool in_stmt)
{
  Expr* expr;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    expr = ParseExpr(arena, toks, true);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsNameToken(toks)) {
    LocName start;
    GetNameToken(arena, toks, "start of expression", &start);

    if (IsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      GetToken(toks, '(');

      Expr** args = NULL;
      int argc = ParseArgs(arena, toks, &args);
      if (argc < 0) {
        return NULL;
      }
      AppExpr* app_expr = arena->alloc(arena, sizeof(AppExpr));
      app_expr->tag = APP_EXPR;
      app_expr->loc = start.loc;
      app_expr->func.name = start.name;
      app_expr->func.loc = start.loc;
      app_expr->argc = argc;
      app_expr->argv = args;
      expr = (Expr*)app_expr;
    } else if (IsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      GetToken(toks, ':');
      UnionExpr* union_expr = arena->alloc(arena, sizeof(UnionExpr));
      union_expr->tag = UNION_EXPR;
      union_expr->loc = start.loc;
      union_expr->type.name = start.name;
      union_expr->type.loc = start.loc;
      union_expr->type.id = UNRESOLVED_ID;
      if (!GetNameToken(arena, toks, "field name", &(union_expr->field))) {
        return NULL;
      }
      if (!GetToken(toks, '(')) {
        return NULL;
      }
      union_expr->value = ParseExpr(arena, toks, false);
      if (union_expr->value == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      expr = (Expr*)union_expr;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      LetExpr* let_expr = arena->alloc(arena, sizeof(LetExpr));
      let_expr->tag = LET_EXPR;
      let_expr->loc = start.loc;
      let_expr->type.name = start.name;
      let_expr->type.loc = start.loc;
      let_expr->type.id = UNRESOLVED_ID;
      GetNameToken(arena, toks, "variable name", &(let_expr->name));
      if (!GetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(arena, toks, false);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(arena, toks, true);
      if (let_expr->body == NULL) {
        return NULL;
      }

      // Return the expression immediately, because it is already complete.
      return (Expr*)let_expr;
    } else {
      // This is the variable expression: start
      VarExpr* var_expr = arena->alloc(arena, sizeof(VarExpr));
      var_expr->tag = VAR_EXPR;
      var_expr->loc = start.loc;
      var_expr->name.name = start.name;
      var_expr->name.loc = start.loc;
      var_expr->name.id = UNRESOLVED_ID;
      expr = (Expr*)var_expr;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional expression of the form: ?(<expr> ; <args>)
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    Expr* condition = ParseExpr(arena, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }
    
    Expr** args = NULL;
    int argc = ParseArgs(arena, toks, &args);
    if (argc < 0) {
      return NULL;
    }
    CondExpr* cond_expr = arena->alloc(arena, sizeof(CondExpr));
    cond_expr->tag = COND_EXPR;
    cond_expr->loc = condition->loc;
    cond_expr->select = condition;
    cond_expr->argc = argc;
    cond_expr->argv = args;
    expr = (Expr*)cond_expr;
  } else {
    UnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (IsToken(toks, '.')) {
    GetToken(toks, '.');

    // This is an access expression of the form: <expr>.<field>
    AccessExpr* access_expr = arena->alloc(arena, sizeof(AccessExpr));
    access_expr->tag = ACCESS_EXPR;
    access_expr->loc = expr->loc;
    access_expr->object = expr;
    if (!GetNameToken(arena, toks, "field name", &(access_expr->field))) {
      return NULL;
    }
    expr = (Expr*)access_expr;
  }

  if (in_stmt) {
    if (!GetToken(toks, ';')) {
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
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the action from.
//   in_stmt - True if parsing an action in the statement context.
//
// Returns:
//   The parsed process action, or NULL on error.
//
// Side effects:
//   Advances the token stream past the parsed action. In case of error,
//   an error message is printed to standard error.

static Actn* ParseActn(FblcArena* arena, TokenStream* toks,
    bool in_stmt)
{
  Actn* actn = NULL;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    actn = ParseActn(arena, toks, true);
    if (actn == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsToken(toks, '$')) {
    // $(<expr>)
    GetToken(toks, '$');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    Expr* expr = ParseExpr(arena, toks, false);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }

    EvalActn* eval_actn = arena->alloc(arena, sizeof(EvalActn));
    eval_actn->tag = EVAL_ACTN;
    eval_actn->loc = expr->loc;
    eval_actn->expr = expr;
    actn = (Actn*)eval_actn;
  } else if (IsNameToken(toks)) {
    LocName name;
    GetNameToken(arena, toks, "port, process, or type name", &name);

    if (IsToken(toks, '~')) {
      GetToken(toks, '~');
      if (!GetToken(toks, '(')) {
        return NULL;
      }

      if (IsToken(toks, ')')) {
        GetToken(toks, ')');
        GetActn* get_actn = arena->alloc(arena, sizeof(GetActn));
        get_actn->tag = GET_ACTN;
        get_actn->loc = name.loc;
        get_actn->port.loc = name.loc;
        get_actn->port.name = name.name;
        actn = (Actn*)get_actn;
      } else {
        Expr* expr = ParseExpr(arena, toks, false);
        if (expr == NULL) {
          return NULL;
        }
        if (!GetToken(toks, ')')) {
          return NULL;
        }

        PutActn* put_actn = arena->alloc(arena, sizeof(PutActn));
        put_actn->tag = PUT_ACTN;
        put_actn->loc = name.loc;
        put_actn->port.loc = name.loc;
        put_actn->port.name = name.name;
        put_actn->expr = expr;
        actn = (Actn*)put_actn;
      }
    } else if (IsToken(toks, '(')) {
      GetToken(toks, '(');
      CallActn* call_actn = arena->alloc(arena, sizeof(CallActn));
      call_actn->tag = CALL_ACTN;
      call_actn->loc = name.loc;
      call_actn->proc.loc = name.loc;
      call_actn->proc.name = name.name;

      FblcVectorInit(arena, call_actn->ports, call_actn->portc);
      if (!IsToken(toks, ';')) {
        FblcVectorExtend(arena, call_actn->ports, call_actn->portc);
        if (!GetNameToken(arena, toks, "port name", call_actn->ports + call_actn->portc++)) {
          return NULL;
        }

        while (IsToken(toks, ',')) {
          GetToken(toks, ',');
          FblcVectorExtend(arena, call_actn->ports, call_actn->portc);
          if (!GetNameToken(arena, toks, "port name", call_actn->ports + call_actn->portc++)) {
            return NULL;
          }
        }
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      Expr** args = NULL;
      int exprc = ParseArgs(arena, toks, &args);
      if (exprc < 0) {
        return NULL;
      }
      call_actn->exprc = exprc;
      call_actn->exprs = args;
      actn = (Actn*)call_actn;
    } else if (in_stmt && IsToken(toks, '<')) {
      GetToken(toks, '<');
      if (!GetToken(toks, '~')) {
        return NULL;
      }
      if (!GetToken(toks, '>')) {
        return NULL;
      }
      LocName getname;
      if (!GetNameToken(arena, toks, "port name", &getname)) {
        return NULL;
      }
      if (!GetToken(toks, ',')) {
        return NULL;
      }
      LocName putname;
      if (!GetNameToken(arena, toks, "port name", &putname)) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      Actn* body = ParseActn(arena, toks, true);
      if (body == NULL) {
        return NULL;
      }
      LinkActn* link_actn = arena->alloc(arena, sizeof(LinkActn));
      link_actn->tag = LINK_ACTN;
      link_actn->loc = name.loc;
      link_actn->type = name;
      link_actn->getname = getname;
      link_actn->putname = putname;
      link_actn->body = body;
      return (Actn*)link_actn;
    } else if (in_stmt && IsNameToken(toks)) {
      ExecActn* exec_actn = arena->alloc(arena, sizeof(ExecActn));
      exec_actn->tag = EXEC_ACTN;
      exec_actn->loc = name.loc;

      FblcVectorInit(arena, exec_actn->execv, exec_actn->execc);
      bool first = true;
      while (first || IsToken(toks, ',')) {
        FblcVectorExtend(arena, exec_actn->execv, exec_actn->execc);
        Exec* exec = exec_actn->execv + exec_actn->execc++;
        if (first) {
          exec->var.type.loc = name.loc;
          exec->var.type.name = name.name;
          first = false;
        } else {
          assert(IsToken(toks, ','));
          GetToken(toks, ',');

          if (!GetNameToken(arena, toks, "type name", &(exec->var.type))) {
            return NULL;
          }
        }

        if (!GetNameToken(arena, toks, "variable name", &(exec->var.name))) {
          return NULL;
        }

        if (!GetToken(toks, '=')) {
          return NULL;
        }

        exec->actn = ParseActn(arena, toks, false);
        if (exec->actn == NULL) {
          return NULL;
        }
      };

      if (!GetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(arena, toks, true);
      if (exec_actn->body == NULL) {
        return NULL;
      }
      return (Actn*)exec_actn;
    } else {
      UnexpectedToken(toks, "The rest of a process starting with a name");
      return NULL;
    }
  } else if (IsToken(toks, '?')) {
    // ?(<expr> ; <proc>, ...)
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    Expr* condition = ParseExpr(arena, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }

    CondActn* cond_actn = arena->alloc(arena, sizeof(CondActn));
    cond_actn->tag = COND_ACTN;
    cond_actn->loc = condition->loc;
    cond_actn->select = condition;

    FblcVectorInit(arena, cond_actn->args, cond_actn->argc);
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }

      Actn* arg = ParseActn(arena, toks, false);
      if (arg == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, cond_actn->args, cond_actn->argc, arg);
    } while (IsToken(toks, ','));

    if (!GetToken(toks, ')')) {
      return NULL;
    }
    actn = (Actn*)cond_actn;
  } else {
    UnexpectedToken(toks, "a process action");
    return NULL;
  }

  if (in_stmt) {
    if (!GetToken(toks, ';')) {
      return NULL;
    }
  }
  return actn;
}

// ParseProgram --
//
//   Parse an  program from the token stream.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the program from.
//
// Result:
//   The parsed program environment, or NULL on error.
//   The 'id' for LocNames throughout the parsed program will be set to
//   UNRESOLVED_ID in the returned result.
//
// Side effects:
//   A program environment is allocated. The token stream is advanced to the
//   end of the stream. In the case of an error, an error message is printed
//   to standard error; the caller is still responsible for freeing (unused)
//   allocations made with the allocator in this case.

Env* ParseProgram(FblcArena* arena, TokenStream* toks)
{
  const char* keywords = "'struct', 'union', 'func', or 'proc'";
  Decl** declv;
  int declc;
  FblcVectorInit(arena, declv, declc);
  while (!IsEOFToken(toks)) {
    // All declarations start with the form: <keyword> <name> (...
    LocName keyword;
    if (!GetNameToken(arena, toks, keywords, &keyword)) {
      return NULL;
    }

    LocName name;
    if (!GetNameToken(arena, toks, "declaration name", &name)) {
      return NULL;
    }

    if (!GetToken(toks, '(')) {
      return NULL;
    }

    bool is_struct = NamesEqual("struct", keyword.name);
    bool is_union = NamesEqual("union", keyword.name);
    bool is_func = NamesEqual("func", keyword.name);
    bool is_proc = NamesEqual("proc", keyword.name);

    if (is_struct || is_union) {
      // Struct and union declarations end with: ... <fields>);
      TypeDecl* type = arena->alloc(arena, sizeof(TypeDecl));
      type->tag = is_struct ? STRUCT_DECL : UNION_DECL;
      type->name.name = name.name;
      type->name.loc = name.loc;
      type->name.id = UNRESOLVED_ID;
      type->fieldc = ParseFields(arena, toks, &(type->fieldv));
      if (type->fieldc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ')')) {
        return NULL;
      }
      FblcVectorAppend(arena, declv, declc, (Decl*)type);
    } else if (is_func) {
      // Function declarations end with: ... <fields>; <type>) <expr>;
      FuncDecl* func = arena->alloc(arena, sizeof(FuncDecl));
      func->tag = FUNC_DECL;
      func->name.name = name.name;
      func->name.loc = name.loc;
      func->name.id = UNRESOLVED_ID;
      func->argc = ParseFields(arena, toks, &(func->argv));
      if (func->argc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(arena, toks, "type", &(func->return_type))) {
        return NULL;
      }

      if (!GetToken(toks, ')')) {
        return NULL;
      }

      func->body = ParseExpr(arena, toks, false);
      if (func->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, declv, declc, (Decl*)func);
    } else if (is_proc) {
      // Proc declarations end with: ... <ports> ; <fields>; [<type>]) <proc>;
      ProcDecl* proc = arena->alloc(arena, sizeof(ProcDecl));
      proc->tag = PROC_DECL;
      proc->name.name = name.name;
      proc->name.loc = name.loc;
      proc->name.id = UNRESOLVED_ID;
      proc->portc = ParsePorts(arena, toks, &(proc->portv));
      if (proc->portc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      proc->argc = ParseFields(arena, toks, &(proc->argv));
      if (proc->argc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(arena, toks, "type", &(proc->return_type))) {
        return NULL;
      }

      if (!GetToken(toks, ')')) {
        return NULL;
      }

      proc->body = ParseActn(arena, toks, false);
      if (proc->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, declv, declc, (Decl*)proc);
    } else {
      ReportError("Expected %s, but got '%s'.\n",
          keyword.loc, keywords, keyword.name);
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }
  }
  return NewEnv(arena, declc, declv);
}
