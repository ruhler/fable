// Parser.c --
//
//   This file implements routines to parse an fblc program from a token
//   stream into abstract syntax form.

#include "Internal.h"

static int ParseFields(Allocator* alloc, TokenStream* toks,
    Field** plist);
static int ParsePorts(Allocator* alloc, TokenStream* toks,
    Port** ports);
static int ParseArgs(Allocator* alloc, TokenStream* toks,
    Expr*** plist);
static Expr* ParseExpr(Allocator* alloc, TokenStream* toks,
    bool in_stmt);
static Actn* ParseActn(Allocator* alloc, TokenStream* toks,
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
//   *plist is set to point to a list of the parsed fields.
//   The token stream is advanced past the tokens describing the fields.
//   In case of an error, an error message is printed to standard error.

static int ParseFields(Allocator* alloc, TokenStream* toks,
    Field** plist)
{
  if (!IsNameToken(toks)) {
    *plist = NULL;
    return 0;
  }

  Vector fields;
  VectorInit(alloc, &fields, sizeof(Field));
  Field* field = VectorAppend(&fields);
  GetNameToken(alloc, toks, "type name", &(field->type));
  if (!GetNameToken(alloc, toks, "field name", &(field->name))) {
    return -1;
  }

  int parsed;
  for (parsed = 1; IsToken(toks, ','); parsed++) {
    GetToken(toks, ',');

    field = VectorAppend(&fields);
    if (!GetNameToken(alloc, toks, "type name", &(field->type))) {
      return -1;
    }
    if (!GetNameToken(alloc, toks, "field name", &(field->name))) {
      return -1;
    }
  }

  int fieldc;
  *plist = VectorExtract(&fields, &fieldc);
  return fieldc;
}

// ParsePorts -
//
//   Parse a list of zero or more ports in the form:
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
//   The token stream is advanced past the last port token.
//   In case of an error, an error message is printed to standard error.

static int ParsePorts(Allocator* alloc, TokenStream* toks,
    Port** ports)
{
  if (!IsNameToken(toks)) {
    *ports = NULL;
    return 0;
  }

  Vector portv;
  VectorInit(alloc, &portv, sizeof(Port));
  bool first = true;
  while (first || IsToken(toks, ',')) {
    if (first) {
      first = false;
    } else {
      GetToken(toks, ',');
    }

    Port* port = VectorAppend(&portv);

    // Get the type.
    if (!GetNameToken(alloc, toks, "type name", &(port->type))) {
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
    if (!GetNameToken(alloc, toks, "port name", &(port->name))) {
      return -1;
    }
  }
  int portc;
  *ports = VectorExtract(&portv, &portc);
  return portc;
}

// ParseArgs --
//
//   Parse a list of zero or more arguments in the form: <expr>, <expr>, ...)
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

static int ParseArgs(Allocator* alloc, TokenStream* toks,
    Expr*** plist)
{
  Vector args;
  VectorInit(alloc, &args, sizeof(Expr*));
  if (!IsToken(toks, ')')) {
    Expr* arg = ParseExpr(alloc, toks, false);
    if (arg == NULL) {
      return -1;
    }
    *((Expr**)VectorAppend(&args)) = arg;

    while (IsToken(toks, ',')) {
      GetToken(toks, ',');
      arg = ParseExpr(alloc, toks, false);
      if (arg == NULL) {
        return -1;
      }
      *((Expr**)VectorAppend(&args)) = arg;
    }
  }
  if (!GetToken(toks, ')')) {
    return -1;
  }

  int argc;
  *plist = VectorExtract(&args, &argc);
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

static Expr* ParseExpr(Allocator* alloc, TokenStream* toks,
    bool in_stmt)
{
  Expr* expr;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    expr = ParseExpr(alloc, toks, true);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsNameToken(toks)) {
    LocName start;
    GetNameToken(alloc, toks, "start of expression", &start);

    if (IsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      GetToken(toks, '(');

      Expr** args = NULL;
      int argc = ParseArgs(alloc, toks, &args);
      if (argc < 0) {
        return NULL;
      }
      AppExpr* app_expr = Alloc(alloc, sizeof(AppExpr));
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
      UnionExpr* union_expr = Alloc(alloc, sizeof(UnionExpr));
      union_expr->tag = UNION_EXPR;
      union_expr->loc = start.loc;
      union_expr->type.name = start.name;
      union_expr->type.loc = start.loc;
      union_expr->type.id = UNRESOLVED_ID;
      if (!GetNameToken(alloc, toks, "field name", &(union_expr->field))) {
        return NULL;
      }
      if (!GetToken(toks, '(')) {
        return NULL;
      }
      union_expr->value = ParseExpr(alloc, toks, false);
      if (union_expr->value == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      expr = (Expr*)union_expr;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      LetExpr* let_expr = Alloc(alloc, sizeof(LetExpr));
      let_expr->tag = LET_EXPR;
      let_expr->loc = start.loc;
      let_expr->type.name = start.name;
      let_expr->type.loc = start.loc;
      let_expr->type.id = UNRESOLVED_ID;
      GetNameToken(alloc, toks, "variable name", &(let_expr->name));
      if (!GetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(alloc, toks, false);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(alloc, toks, true);
      if (let_expr->body == NULL) {
        return NULL;
      }

      // Return the expression immediately, because it is already complete.
      return (Expr*)let_expr;
    } else {
      // This is the variable expression: start
      VarExpr* var_expr = Alloc(alloc, sizeof(VarExpr));
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
    Expr* condition = ParseExpr(alloc, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }
    
    Expr** args = NULL;
    int argc = ParseArgs(alloc, toks, &args);
    if (argc < 0) {
      return NULL;
    }
    CondExpr* cond_expr = Alloc(alloc, sizeof(CondExpr));
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
    AccessExpr* access_expr = Alloc(alloc, sizeof(AccessExpr));
    access_expr->tag = ACCESS_EXPR;
    access_expr->loc = expr->loc;
    access_expr->object = expr;
    if (!GetNameToken(alloc, toks, "field name", &(access_expr->field))) {
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

static Actn* ParseActn(Allocator* alloc, TokenStream* toks,
    bool in_stmt)
{
  Actn* actn = NULL;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    actn = ParseActn(alloc, toks, true);
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
    Expr* expr = ParseExpr(alloc, toks, false);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }

    EvalActn* eval_actn = Alloc(alloc, sizeof(EvalActn));
    eval_actn->tag = EVAL_ACTN;
    eval_actn->loc = expr->loc;
    eval_actn->expr = expr;
    actn = (Actn*)eval_actn;
  } else if (IsNameToken(toks)) {
    LocName name;
    GetNameToken(alloc, toks, "port, process, or type name", &name);

    if (IsToken(toks, '~')) {
      GetToken(toks, '~');
      if (!GetToken(toks, '(')) {
        return NULL;
      }

      if (IsToken(toks, ')')) {
        GetToken(toks, ')');
        GetActn* get_actn = Alloc(alloc, sizeof(GetActn));
        get_actn->tag = GET_ACTN;
        get_actn->loc = name.loc;
        get_actn->port.loc = name.loc;
        get_actn->port.name = name.name;
        actn = (Actn*)get_actn;
      } else {
        Expr* expr = ParseExpr(alloc, toks, false);
        if (expr == NULL) {
          return NULL;
        }
        if (!GetToken(toks, ')')) {
          return NULL;
        }

        PutActn* put_actn = Alloc(alloc, sizeof(PutActn));
        put_actn->tag = PUT_ACTN;
        put_actn->loc = name.loc;
        put_actn->port.loc = name.loc;
        put_actn->port.name = name.name;
        put_actn->expr = expr;
        actn = (Actn*)put_actn;
      }
    } else if (IsToken(toks, '(')) {
      GetToken(toks, '(');
      CallActn* call_actn = Alloc(alloc, sizeof(CallActn));
      call_actn->tag = CALL_ACTN;
      call_actn->loc = name.loc;
      call_actn->proc.loc = name.loc;
      call_actn->proc.name = name.name;

      Vector ports;
      VectorInit(alloc, &ports, sizeof(LocName));
      if (!IsToken(toks, ';')) {
        if (!GetNameToken(
              alloc, toks, "port name", VectorAppend(&ports))) {
          return NULL;
        }

        while (IsToken(toks, ',')) {
          GetToken(toks, ',');
          if (!GetNameToken(
                alloc, toks, "port name", VectorAppend(&ports))) {
            return NULL;
          }
        }
      }
      call_actn->ports = VectorExtract(&ports, &(call_actn->portc));

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      Expr** args = NULL;
      int exprc = ParseArgs(alloc, toks, &args);
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
      if (!GetNameToken(alloc, toks, "port name", &getname)) {
        return NULL;
      }
      if (!GetToken(toks, ',')) {
        return NULL;
      }
      LocName putname;
      if (!GetNameToken(alloc, toks, "port name", &putname)) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      Actn* body = ParseActn(alloc, toks, true);
      if (body == NULL) {
        return NULL;
      }
      LinkActn* link_actn = Alloc(alloc, sizeof(LinkActn));
      link_actn->tag = LINK_ACTN;
      link_actn->loc = name.loc;
      link_actn->type = name;
      link_actn->getname = getname;
      link_actn->putname = putname;
      link_actn->body = body;
      return (Actn*)link_actn;
    } else if (in_stmt && IsNameToken(toks)) {
      ExecActn* exec_actn = Alloc(alloc, sizeof(ExecActn));
      exec_actn->tag = EXEC_ACTN;
      exec_actn->loc = name.loc;

      Vector execs;
      VectorInit(alloc, &execs, sizeof(Exec));
      bool first = true;
      while (first || IsToken(toks, ',')) {
        Exec* exec = VectorAppend(&execs);
        if (first) {
          exec->var.type.loc = name.loc;
          exec->var.type.name = name.name;
          first = false;
        } else {
          assert(IsToken(toks, ','));
          GetToken(toks, ',');

          if (!GetNameToken(alloc, toks, "type name", &(exec->var.type))) {
            return NULL;
          }
        }

        if (!GetNameToken(
              alloc, toks, "variable name", &(exec->var.name))) {
          return NULL;
        }

        if (!GetToken(toks, '=')) {
          return NULL;
        }

        exec->actn = ParseActn(alloc, toks, false);
        if (exec->actn == NULL) {
          return NULL;
        }
      };
      exec_actn->execv = VectorExtract(&execs, &(exec_actn->execc));

      if (!GetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(alloc, toks, true);
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
    Expr* condition = ParseExpr(alloc, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }

    Vector args;
    VectorInit(alloc, &args, sizeof(Actn*));
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }

      Actn** arg = VectorAppend(&args);
      *arg = ParseActn(alloc, toks, false);
      if (*arg == NULL) {
        return NULL;
      }
    } while (IsToken(toks, ','));

    if (!GetToken(toks, ')')) {
      return NULL;
    }
    CondActn* cond_actn = Alloc(alloc, sizeof(CondActn));
    cond_actn->tag = COND_ACTN;
    cond_actn->loc = condition->loc;
    cond_actn->select = condition;
    cond_actn->args = VectorExtract(&args, &(cond_actn->argc));
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
//   alloc - An allocator to use for allocating the program environment.
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

Env* ParseProgram(Allocator* alloc, TokenStream* toks)
{
  const char* keywords = "'struct', 'union', 'func', or 'proc'";
  Vector decls;
  VectorInit(alloc, &decls, sizeof(Decl*));
  while (!IsEOFToken(toks)) {
    // All declarations start with the form: <keyword> <name> (...
    LocName keyword;
    if (!GetNameToken(alloc, toks, keywords, &keyword)) {
      return NULL;
    }

    LocName name;
    if (!GetNameToken(alloc, toks, "declaration name", &name)) {
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
      TypeDecl* type = Alloc(alloc, sizeof(TypeDecl));
      type->tag = is_struct ? STRUCT_DECL : UNION_DECL;
      type->name.name = name.name;
      type->name.loc = name.loc;
      type->name.id = UNRESOLVED_ID;
      type->fieldc = ParseFields(alloc, toks, &(type->fieldv));
      if (type->fieldc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ')')) {
        return NULL;
      }
      *((Decl**)VectorAppend(&decls)) = (Decl*)type;
    } else if (is_func) {
      // Function declarations end with: ... <fields>; <type>) <expr>;
      FuncDecl* func = Alloc(alloc, sizeof(FuncDecl));
      func->tag = FUNC_DECL;
      func->name.name = name.name;
      func->name.loc = name.loc;
      func->name.id = UNRESOLVED_ID;
      func->argc = ParseFields(alloc, toks, &(func->argv));
      if (func->argc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(alloc, toks, "type", &(func->return_type))) {
        return NULL;
      }

      if (!GetToken(toks, ')')) {
        return NULL;
      }

      func->body = ParseExpr(alloc, toks, false);
      if (func->body == NULL) {
        return NULL;
      }
      *((Decl**)VectorAppend(&decls)) = (Decl*)func;
    } else if (is_proc) {
      // Proc declarations end with: ... <ports> ; <fields>; [<type>]) <proc>;
      ProcDecl* proc = Alloc(alloc, sizeof(ProcDecl));
      proc->tag = PROC_DECL;
      proc->name.name = name.name;
      proc->name.loc = name.loc;
      proc->name.id = UNRESOLVED_ID;
      proc->portc = ParsePorts(alloc, toks, &(proc->portv));
      if (proc->portc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      proc->argc = ParseFields(alloc, toks, &(proc->argv));
      if (proc->argc < 0) {
        return NULL;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(alloc, toks, "type", &(proc->return_type))) {
        return NULL;
      }

      if (!GetToken(toks, ')')) {
        return NULL;
      }

      proc->body = ParseActn(alloc, toks, false);
      if (proc->body == NULL) {
        return NULL;
      }
      *((Decl**)VectorAppend(&decls)) = (Decl*)proc;
    } else {
      ReportError("Expected %s, but got '%s'.\n",
          keyword.loc, keywords, keyword.name);
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }
  }
  int declc;
  Decl** declv = VectorExtract(&decls, &declc);
  return NewEnv(alloc, declc, declv);
}
