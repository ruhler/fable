// parse.y --
//   This file describes the bison grammar for parsing fbld module
//   declarations and definitions.
%{
  #include <assert.h>       // for assert
  #include <ctype.h>        // for isalnum, isspace
  #include <stdio.h>        // for FILE*, fgetc
  #include <string.h>       // for strcmp

  #include "fbld.h"

  // Lex --
  //   State for the lexer.
  //
  // Fields:
  //   c - The next character in the input stream. EOF is used to indicate the
  //   end of the input stream.
  //   loc - The location corresponding to the character c.
  //   fin - The input stream if reading from a file, NULL otherwise.
  //   sin - The input stream if reading from a string, NULL otherwise.
  typedef struct {
    int c;
    FbldLoc loc;
    FILE* fin;
    const char* sin;
  } Lex;

  // ParseResult --
  //   Used to store the final parsed object depending on what kind of object
  //   is parsed.
  typedef union {
    FbldMType* mtype;
    FbldMDefn* mdefn;
    FbldValue* value;
    FbldQRef* qref;
  } ParseResult;

  // Decls --
  //   Structure to store a list of declarations of various kind.
  typedef struct {
    FbldUsingV* usingv;
    FbldTypeV* typev;
    FbldFuncV* funcv;
    FbldProcV* procv;
  } Decls;

  #define YYLTYPE FbldLoc*

  #define YYLLOC_DEFAULT(Cur, Rhs, N)  \
  do                                   \
    if (N) (Cur) = YYRHSLOC(Rhs, 1);   \
    else   (Cur) = YYRHSLOC(Rhs, 0);   \
  while (0)
%}

%union {
  FbldName* name;
  FbldQRef* qref;
  FbldQRefV* qrefv;
  FbldIRef* iref;
  FbldMRef* mref;
  FbldMRefV* mrefv;
  FbldMType* mtype;
  FbldMDefn* mdefn;
  Decls* decls;
  FbldUsingItemV* uitemv;
  FbldUsing* using;
  FbldType* type;
  FbldFunc* func;
  FbldProc* proc;
  FbldExpr* expr;
  FbldActn* actn;
  FbldNameV* namev;
  FbldMArgV* margv;
  FbldArgV* argv;
  FbldPolarity polarity;
  FbldPortV* portv;
  FbldExprV* exprv;
  FbldIdV* idv;
  FbldActnV* actnv;
  FbldExecV* execv;
  FbldValue* value;
  FbldValueV* valuev;
}

%{
  static bool IsNameChar(int c);
  static bool IsSingleChar(int c);
  static void ReadNextChar(Lex* lex);
  static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FblcArena* arena, Lex* lex);
  static void yyerror(YYLTYPE* llocp, FblcArena* arena, Lex* lex, ParseResult* result, const char* msg);
%}

%locations
%define api.pure
%define parse.error verbose
%param {FblcArena* arena} {Lex* lex}
%parse-param {ParseResult* result}

%token END 0 "end of file"
%token INVALID "invalid character"

// Bison grammars only support a single start symbol, but we sometimes want to
// parse different things. To get around this limitation in bison, we insert a
// single-character non-whitespace, non-name indicator token at the beginning
// of the input stream to indicate the start symbol to use depending on what
// we want to parse.
%token START_MTYPE 1 "mtype parse start indicator"
%token START_MDEFN 2 "mdefn parse start indicator"
%token START_VALUE 3 "value parse start indicator"
%token START_QNAME 4 "qref parse start indicator"

%token <name> NAME

// Keywords. These have type 'name' because in many contexts they are treated
// as normal names rather than keywords.
%token <name> MTYPE "mtype"
%token <name> MDEFN "mdefn"
%token <name> TYPE "type"
%token <name> STRUCT "struct"
%token <name> UNION "union"
%token <name> FUNC "func"
%token <name> PROC "proc"
%token <name> USING "using"

%type <name> keyword name
%type <qref> qref
%type <iref> iref
%type <mref> mref
%type <mtype> mtype
%type <mdefn> mdefn
%type <decls> decl_list defn_list
%type <uitemv> using_item_list
%type <using> using
%type <type> type_decl type_defn abstract_type_decl struct_decl union_decl
%type <func> func_decl func_defn
%type <proc> proc_decl proc_defn
%type <expr> expr stmt
%type <actn> actn pstmt
%type <namev> name_list
%type <qrefv> qref_list
%type <mrefv> mref_list
%type <margv> marg_list
%type <argv> arg_list non_empty_arg_list
%type <polarity> polarity
%type <portv> port_list non_empty_port_list
%type <exprv> expr_list non_empty_expr_list
%type <actnv> non_empty_actn_list
%type <execv> non_empty_exec_list
%type <idv> id_list non_empty_id_list
%type <value> value
%type <valuev> value_list non_empty_value_list

%%

// This grammar is used for parsing starting from multiple different start
// tokens. We insert an arbitrary, artificial single-character token to
// indicate which start token we actually want to use.
start:
     START_MTYPE mtype { result->mtype = $2; }
   | START_MDEFN mdefn { result->mdefn = $2; }
   | START_VALUE value { result->value = $2; }
   | START_QNAME qref { result->qref = $2; }
   ;
 
mtype: "mtype" name '<' name_list '>' '{' decl_list '}' ';' {
          $$ = FBLC_ALLOC(arena, FbldMType);
          $$->name = $2;
          $$->targv = $4;
          $$->usingv = $7->usingv;
          $$->typev = $7->typev;
          $$->funcv = $7->funcv;
          $$->procv = $7->procv;
          // TODO: Don't leak the allocated Decls object?
        }
     ;

mdefn: "mdefn" name '<' name_list ';' marg_list ';' iref '>' '{' defn_list '}' ';' {
          $$ = FBLC_ALLOC(arena, FbldMDefn);
          $$->name = $2;
          $$->targv = $4;
          $$->margv = $6;
          $$->iref = $8;
          $$->usingv = $11->usingv;
          $$->typev = $11->typev;
          $$->funcv = $11->funcv;
          $$->procv = $11->procv;
          // TODO: Don't leak the allocated Decls object?
        }
     ;

marg_list:
    %empty {
       $$ = FBLC_ALLOC(arena, FbldMArgV);
       FblcVectorInit(arena, *$$);
    } 
  | iref name {
       $$ = FBLC_ALLOC(arena, FbldMArgV);
       FblcVectorInit(arena, *$$);
       FbldMArg* marg = FBLC_ALLOC(arena, FbldMArg);
       marg->iref = $1;
       marg->name = $2;
       FblcVectorAppend(arena, *$$, marg);
    }
  | marg_list ',' iref name {
       $$ = $1;
       FbldMArg* marg = FBLC_ALLOC(arena, FbldMArg);
       marg->iref = $3;
       marg->name = $4;
       FblcVectorAppend(arena, *$$, marg);
    }
  ;

name: NAME | keyword ;

keyword: "mtype" | "mdefn" | "type" | "struct" | "union" | "func" | "proc" | "using" ;

decl_list:
    %empty {
      $$ = FBLC_ALLOC(arena, Decls);
      $$->usingv = FBLC_ALLOC(arena, FbldUsingV);
      $$->typev = FBLC_ALLOC(arena, FbldTypeV);
      $$->funcv = FBLC_ALLOC(arena, FbldFuncV);
      $$->procv = FBLC_ALLOC(arena, FbldProcV);
      FblcVectorInit(arena, *($$->usingv));
      FblcVectorInit(arena, *($$->typev));
      FblcVectorInit(arena, *($$->funcv));
      FblcVectorInit(arena, *($$->procv));
    }
  | decl_list using ';' {
      FblcVectorAppend(arena, *($1->usingv), $2);
      $$ = $1;
    }
  | decl_list type_decl ';' {
      FblcVectorAppend(arena, *($1->typev), $2);
      $$ = $1;
    }
  | decl_list func_decl ';' {
      FblcVectorAppend(arena, *($1->funcv), $2);
      $$ = $1;
    }
  | decl_list proc_decl ';' {
      FblcVectorAppend(arena, *($1->procv), $2);
      $$ = $1;
    }
  ;

expr_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldExprV);
      FblcVectorInit(arena, *$$);
    }
  | non_empty_expr_list
  ;

non_empty_expr_list:
  expr {
      $$ = FBLC_ALLOC(arena, FbldExprV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | non_empty_expr_list ',' expr {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

non_empty_exec_list:
  qref name '=' actn {
      $$ = FBLC_ALLOC(arena, FbldExecV);
      FblcVectorInit(arena, *$$);
      FbldExec* exec = FblcVectorExtend(arena, *$$);
      exec->type = $1;
      exec->name = $2;
      exec->actn = $4;
    }
  | non_empty_exec_list ',' qref name '=' actn {
      $$ = $1;
      FbldExec* exec = FblcVectorExtend(arena, *$$);
      exec->type = $3;
      exec->name = $4;
      exec->actn = $6;
    }
  ;

id_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldIdV);
      FblcVectorInit(arena, *$$);
    }
  | non_empty_id_list
  ;

non_empty_id_list:
  name {
      $$ = FBLC_ALLOC(arena, FbldIdV);
      FblcVectorInit(arena, *$$);
      FbldId* id = FblcVectorExtend(arena, *$$);
      id->name = $1;
      id->id = FBLC_NULL_ID;
    }
  | non_empty_id_list ',' name {
      $$ = $1;
      FblcVectorInit(arena, *$$);
      FbldId* id = FblcVectorExtend(arena, *$$);
      id->name = $3;
      id->id = FBLC_NULL_ID;
    }
  ;

non_empty_actn_list:
  actn {
      $$ = FBLC_ALLOC(arena, FbldActnV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | non_empty_actn_list ',' actn {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

using_item_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldUsingItemV);
      FblcVectorInit(arena, *$$);
    }
  | using_item_list name ';' {
      FbldUsingItem* item = FBLC_ALLOC(arena, FbldUsingItem);
      item->source = $2;
      item->dest = $2;
      FblcVectorAppend(arena, *$1, item);
      $$ = $1;
    }
  | using_item_list name '=' name ';' {
      FbldUsingItem* item = FBLC_ALLOC(arena, FbldUsingItem);
      item->source = $4;
      item->dest = $2;
      FblcVectorAppend(arena, *$1, item);
      $$ = $1;
    }
    ;

using: "using" mref '{' using_item_list '}' {
      $$ = FBLC_ALLOC(arena, FbldUsing);
      $$->mref = $2;
      $$->itemv = $4;
    }
    ;

abstract_type_decl: "type" name {
      $$ = FBLC_ALLOC(arena, FbldType);
      $$->name = $2;
      $$->kind = FBLD_ABSTRACT_KIND;
      $$->fieldv = NULL;
    }
    ;

struct_decl: "struct" name '(' arg_list ')' {
      $$ = FBLC_ALLOC(arena, FbldType);
      $$->name = $2;
      $$->kind = FBLD_STRUCT_KIND;
      $$->fieldv = $4;
    }
    ;

union_decl: "union" name '(' non_empty_arg_list ')' {
      $$ = FBLC_ALLOC(arena, FbldType);
      $$->name = $2;
      $$->kind = FBLD_UNION_KIND;
      $$->fieldv = $4;
    }
    ;

type_decl: abstract_type_decl | struct_decl | union_decl ;
type_defn: struct_decl | union_decl ;

func_decl: "func" name '(' arg_list ';' qref ')' {
      $$ = FBLC_ALLOC(arena, FbldFunc);
      $$->name = $2;
      $$->argv = $4;
      $$->return_type = $6;
      $$->body = NULL;
    }
    ;

func_defn: func_decl expr {
      $$ = $1;
      $$->body = $2;
    }
    ;

proc_decl: "proc" name '(' port_list ';' arg_list ';' qref ')' {
      $$ = FBLC_ALLOC(arena, FbldProc);
      $$->name = $2;
      $$->portv = $4;
      $$->argv = $6;
      $$->return_type = $8;
      $$->body = NULL;
    }
    ;

proc_defn: proc_decl actn {
      $$ = $1;
      $$->body = $2;
    }
    ;

stmt: expr ';' { $$ = $1; }
    | qref name '=' expr ';' stmt {
        FbldLetExpr* let_expr = FBLC_ALLOC(arena, FbldLetExpr);
        let_expr->_base.tag = FBLD_LET_EXPR;
        let_expr->_base.loc = @$;
        let_expr->type = $1;
        let_expr->var = $2;
        let_expr->def = $4;
        let_expr->body = $6;
        $$ = &let_expr->_base;
      } 
    ;

expr:
    '{' stmt '}' {
       $$ = $2;
    }
  | name {
      FbldVarExpr* var_expr = FBLC_ALLOC(arena, FbldVarExpr);
      var_expr->_base.tag = FBLD_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var.name = $1;
      var_expr->var.id = FBLC_NULL_ID;
      $$ = &var_expr->_base;
    }
  | qref '(' expr_list ')' {
      FbldAppExpr* app_expr = FBLC_ALLOC(arena, FbldAppExpr);
      app_expr->_base.tag = FBLD_APP_EXPR;
      app_expr->_base.loc = @$;
      app_expr->func = $1;
      app_expr->argv = $3;
      $$ = &app_expr->_base;
    }
  | qref ':' name '(' expr ')' {
      FbldUnionExpr* union_expr = FBLC_ALLOC(arena, FbldUnionExpr);
      union_expr->_base.tag = FBLD_UNION_EXPR;
      union_expr->_base.loc = @$;
      union_expr->type = $1;
      union_expr->field.name = $3;
      union_expr->field.id = FBLC_NULL_ID;
      union_expr->arg = $5;
      $$ = &union_expr->_base;
    }
  | expr '.' name {
      FbldAccessExpr* access_expr = FBLC_ALLOC(arena, FbldAccessExpr);
      access_expr->_base.tag = FBLD_ACCESS_EXPR;
      access_expr->_base.loc = @$;
      access_expr->obj = $1;
      access_expr->field.name = $3;
      access_expr->field.id = FBLC_NULL_ID;
      $$ = &access_expr->_base;
    }
  | '?' '(' expr ';' non_empty_expr_list ')' {
      FbldCondExpr* cond_expr = FBLC_ALLOC(arena, FbldCondExpr);
      cond_expr->_base.tag = FBLD_COND_EXPR;
      cond_expr->_base.loc = @$;
      cond_expr->select = $3;
      cond_expr->argv = $5;
      $$ = &cond_expr->_base;
  }
   ;

pstmt: actn ';' { $$ = $1; }
     | qref '<' '~' '>' name ',' name ';' pstmt {
         FbldLinkActn* link_actn = FBLC_ALLOC(arena, FbldLinkActn);
         link_actn->_base.tag = FBLD_LINK_ACTN;
         link_actn->_base.loc = @$;
         link_actn->type = $1;
         link_actn->get = $5;
         link_actn->put = $7;
         link_actn->body = $9;
         $$ = &link_actn->_base;
       }
     | non_empty_exec_list ';' pstmt {
         FbldExecActn* exec_actn = FBLC_ALLOC(arena, FbldExecActn);
         exec_actn->_base.tag = FBLD_EXEC_ACTN;
         exec_actn->_base.loc = @$;
         exec_actn->execv = $1;
         exec_actn->body = $3;
         $$ = &exec_actn->_base;
       }
     ;

actn:
    '{' pstmt '}' {
        $$ = $2;
    }
  | '$' '(' expr ')' {
      FbldEvalActn* eval_actn = FBLC_ALLOC(arena, FbldEvalActn);
      eval_actn->_base.tag = FBLD_EVAL_ACTN;
      eval_actn->_base.loc = @$;
      eval_actn->arg = $3;
      $$ = &eval_actn->_base;
    }
  | '~' name '(' ')' {
      FbldGetActn* get_actn = FBLC_ALLOC(arena, FbldGetActn);
      get_actn->_base.tag = FBLD_GET_ACTN;
      get_actn->_base.loc = @$;
      get_actn->port.name = $2;
      get_actn->port.id = FBLC_NULL_ID;
      $$ = &get_actn->_base;
    }
  | '~' name '(' expr ')' {
      FbldPutActn* put_actn = FBLC_ALLOC(arena, FbldPutActn);
      put_actn->_base.tag = FBLD_PUT_ACTN;
      put_actn->_base.loc = @$;
      put_actn->port.name = $2;
      put_actn->port.id = FBLC_NULL_ID;
      put_actn->arg = $4;
      $$ = &put_actn->_base;
    }
  | '?' '(' expr ';' non_empty_actn_list ')' {
      FbldCondActn* cond_actn = FBLC_ALLOC(arena, FbldCondActn);
      cond_actn->_base.tag = FBLD_COND_ACTN;
      cond_actn->_base.loc = @$;
      cond_actn->select = $3;
      cond_actn->argv = $5;
      $$ = &cond_actn->_base;
  }
  | qref '(' id_list ';' expr_list ')' {
      FbldCallActn* call_actn = FBLC_ALLOC(arena, FbldCallActn);
      call_actn->_base.tag = FBLD_CALL_ACTN;
      call_actn->_base.loc = @$;
      call_actn->proc = $1;
      call_actn->portv = $3;
      call_actn->argv = $5;
      $$ = &call_actn->_base;
    }
  ;
  
    
defn_list:
    %empty {
      $$ = FBLC_ALLOC(arena, Decls);
      $$->usingv = FBLC_ALLOC(arena, FbldUsingV);
      $$->typev = FBLC_ALLOC(arena, FbldTypeV);
      $$->funcv = FBLC_ALLOC(arena, FbldFuncV);
      $$->procv = FBLC_ALLOC(arena, FbldProcV);
      FblcVectorInit(arena, *($$->usingv));
      FblcVectorInit(arena, *($$->typev));
      FblcVectorInit(arena, *($$->funcv));
      FblcVectorInit(arena, *($$->procv));
    }
  | defn_list using ';' {
      FblcVectorAppend(arena, *($1->usingv), $2);
      $$ = $1;
    }
  | defn_list type_defn ';' {
      FblcVectorAppend(arena, *($1->typev), $2);
      $$ = $1;
    }
  | defn_list func_defn ';' {
      FblcVectorAppend(arena, *($1->funcv), $2);
      $$ = $1;
    }
  | defn_list proc_defn ';' {
      FblcVectorAppend(arena, *($1->procv), $2);
      $$ = $1;
    }
  ;

name_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldNameV);
      FblcVectorInit(arena, *$$);
    }
  | name {
      $$ = FBLC_ALLOC(arena, FbldNameV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | name_list ',' name {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

qref_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldQRefV);
      FblcVectorInit(arena, *$$);
    }
  | qref {
      $$ = FBLC_ALLOC(arena, FbldQRefV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | qref_list ',' qref {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

mref_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldMRefV);
      FblcVectorInit(arena, *$$);
    }
  | mref {
      $$ = FBLC_ALLOC(arena, FbldMRefV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | mref_list ',' mref {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

arg_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldArgV);
      FblcVectorInit(arena, *$$);
    }
  | non_empty_arg_list ;

non_empty_arg_list:
    qref name {
      $$ = FBLC_ALLOC(arena, FbldArgV);
      FblcVectorInit(arena, *$$);
      FbldArg* tname = FBLC_ALLOC(arena, FbldArg);
      tname->type = $1;
      tname->name = $2;
      FblcVectorAppend(arena, *$$, tname);
    }
  | non_empty_arg_list ',' qref name {
      FbldArg* tname = FBLC_ALLOC(arena, FbldArg);
      tname->type = $3;
      tname->name = $4;
      FblcVectorAppend(arena, *$1, tname);
      $$ = $1;
    }
  ;

polarity: '<' '~' { $$ = FBLD_GET_POLARITY; }
        | '~' '>' { $$ = FBLD_PUT_POLARITY; }
        ;

port_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldPortV);
      FblcVectorInit(arena, *$$);
    }
  | non_empty_port_list ;

non_empty_port_list:
    qref polarity name {
      $$ = FBLC_ALLOC(arena, FbldPortV);
      FblcVectorInit(arena, *$$);
      FbldPort* port = FblcVectorExtend(arena, *$$);
      port->type = $1;
      port->name = $3;
      port->polarity = $2;
    }
  | non_empty_port_list ',' qref polarity name {
      $$ = $1;
      FbldPort* port = FblcVectorExtend(arena, *$$);
      port->type = $3;
      port->name = $5;
      port->polarity = $4;
    }
  ;

qref:
    name {
      $$ = FBLC_ALLOC(arena, FbldQRef);
      $$->uname = $1;
      $$->umref = NULL;
      $$->rname = NULL;
      $$->rmref = NULL;
    }
  | name '@' mref {
      $$ = FBLC_ALLOC(arena, FbldQRef);
      $$->uname = $1;
      $$->umref = $3;
      $$->rname = NULL;
      $$->rmref = NULL;
    }
  ;

iref: name '<' qref_list '>' {
      $$ = FBLC_ALLOC(arena, FbldIRef);
      $$->name = $1;
      $$->targv = $3;
    }
  ;

mref: name '<' qref_list ';' mref_list '>' {
      $$ = FBLC_ALLOC(arena, FbldMRef);
      $$->name = $1;
      $$->targv = $3;
      $$->margv = $5;
    }
  ;

value:
    qref '(' value_list ')' {
      $$ = FBLC_ALLOC(arena, FbldValue);
      $$->kind = FBLD_STRUCT_KIND;
      $$->type = $1;
      $$->tag = NULL;
      $$->fieldv = $3;
    }
  | qref ':' name '(' value ')' {
      $$ = FBLC_ALLOC(arena, FbldValue);
      $$->kind = FBLD_UNION_KIND;
      $$->type = $1;
      $$->tag = $3;
      $$->fieldv = FBLC_ALLOC(arena, FbldValueV);
      FblcVectorInit(arena, *$$->fieldv);
      FblcVectorAppend(arena, *$$->fieldv, $5);
    }
  ;

value_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldValueV);
      FblcVectorInit(arena, *$$);
    }
  | non_empty_value_list
  ;

non_empty_value_list:
    value {
      $$ = FBLC_ALLOC(arena, FbldValueV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | non_empty_value_list ',' value {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;
           
%%

// IsNameChar --
//   Tests whether a character is an acceptable character to use in a name
//   token.
//
// Inputs:
//   c - The character to test. This must have a value of an unsigned char or
//       EOF.
//
// Result:
//   The value true if the character is an acceptable character to use in a
//   name token, and the value false otherwise.
//
// Side effects:
//   None.
static bool IsNameChar(int c)
{
  return isalnum(c) || c == '_';
}

// IsNameChar --
//   Tests whether a character is an acceptable single-character token.
//   token.
//
// Inputs:
//   c - The character to test. This must have a value of an unsigned char or
//       EOF.
//
// Result:
//   The value true if the character is an acceptable character to use as a
//   single character token, and the value false otherwise.
//
// Side effects:
//   None.
static bool IsSingleChar(int c)
{
  return strchr("(){};,@:?=.<>~$", c) != NULL
    || c == START_MTYPE
    || c == START_MDEFN
    || c == START_VALUE
    || c == START_QNAME;
}

// ReadNextChar --
//   Advance to the next character in the input stream.
//
// Inputs:
//   lex - The context of the lexer to advance.
//
// Results:
//   None.
//
// Side effects:
//   Updates the character, location, and input stream of the lex context.
static void ReadNextChar(Lex* lex)
{
  int c = lex->c;
  if (lex->fin) {
    lex->c = fgetc(lex->fin);
  } else if (lex->sin) {
    if (*lex->sin == '\0') {
      lex->c = EOF;
    } else {
      lex->c = *lex->sin++;
    }
  }

  if (c == '\n') {
    lex->loc.line++;
    lex->loc.col = 1;
  } else if (c != EOF) {
    lex->loc.col++;
  }
}

// yylex -- 
//   Return the next token in the input stream for the given lex context.
//   This is the lexer for the bison generated parser.
//
// Inputs:
//   lvalp - Output parameter for returned token value.
//   llocp - Output parameter for the returned token's location.
//   arena - Arena used for allocating names.
//   lex - The lex context to parse the next token from.
// 
// Results:
//   The id of the next terminal symbol in the input stream.
//
// Side effects:
//   Sets lvalp with the semantic value of the next token in the input
//   stream, sets llocp with the location of the next token in the input
//   stream, advances the stream to the subsequent token and updates the lex
//   loc accordingly.
static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FblcArena* arena, Lex* lex)
{
  // Skip past white space and comments.
  bool is_comment_start = (lex->c == '#');
  while (isspace(lex->c) || is_comment_start) {
    ReadNextChar(lex);
    if (is_comment_start) {
      while (lex->c != EOF && lex->c != '\n') {
        ReadNextChar(lex);
      }
    }
    is_comment_start = (lex->c == '#');
  }

  FbldLoc* loc = FBLC_ALLOC(arena, FbldLoc);
  loc->source = lex->loc.source;
  loc->line = lex->loc.line;
  loc->col = lex->loc.col;
  *llocp = loc;
  
  if (lex->c == EOF) {
    return END;
  }

  if (IsSingleChar(lex->c)) {
    // Return the character and set the lexer character to whitespace so it
    // will be skipped over the next time we go to read a token from the
    // stream.
    int c = lex->c;
    lex->c = ' ';
    return c;
  };

  if (!IsNameChar(lex->c)) {
    return INVALID;
  }

  struct { size_t size; char* xs; } namev;
  FblcVectorInit(arena, namev);
  while (IsNameChar(lex->c)) {
    FblcVectorAppend(arena, namev, lex->c);
    ReadNextChar(lex);
  }
  FblcVectorAppend(arena, namev, '\0');
  lvalp->name = FBLC_ALLOC(arena, FbldName);
  lvalp->name->name = namev.xs;
  lvalp->name->loc = loc;

  struct { char* keyword; int symbol; } keywords[] = {
    {.keyword = "mtype", .symbol = MTYPE},
    {.keyword = "mdefn", .symbol = MDEFN},
    {.keyword = "type", .symbol = TYPE},
    {.keyword = "struct", .symbol = STRUCT},
    {.keyword = "union", .symbol = UNION},
    {.keyword = "func", .symbol = FUNC},
    {.keyword = "proc", .symbol = PROC},
    {.keyword = "using", .symbol = USING}
  };
  size_t num_keywords = sizeof(keywords)/(sizeof(keywords[0]));
  for (size_t i = 0; i < num_keywords; ++i) {
    if (strcmp(namev.xs, keywords[i].keyword) == 0) {
      return keywords[i].symbol;
    }
  }
  return NAME;
}

// yyerror --
//   The error reporting function for the bison generated parser.
// 
// Inputs:
//   llocp - The location of the error.
//   arena - unused.
//   fin - unused.
//   lex - The lexical context.
//   result - unused.
//   msg - The error message.
//
// Results:
//   None.
//
// Side effects:
//   An error message is printed to stderr.
static void yyerror(YYLTYPE* llocp, FblcArena* arena, Lex* lex, ParseResult* result, const char* msg)
{
  FbldReportError("%s\n", *llocp, msg);
}

// FbldParseMType -- see documentation in fbld.h
FbldMType* FbldParseMType(FblcArena* arena, const char* filename)
{
  FILE* fin = fopen(filename, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename);
    return NULL;
  }

  Lex lex = {
    .c = START_MTYPE,
    .loc = { .source = filename, .line = 1, .col = 0 },
    .fin = fin,
    .sin = NULL
  };
  ParseResult result;
  result.mtype = NULL;
  yyparse(arena, &lex, &result);
  return result.mtype;
}

// FbldParseMDefn -- see documentation in fbld.h
FbldMDefn* FbldParseMDefn(FblcArena* arena, const char* filename)
{
  FILE* fin = fopen(filename, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename);
    return NULL;
  }

  Lex lex = {
    .c = START_MDEFN,
    .loc = { .source = filename, .line = 1, .col = 0 },
    .fin = fin,
    .sin = NULL
  };
  ParseResult result;
  result.mdefn = NULL;
  yyparse(arena, &lex, &result);
  return result.mdefn;
}

// FbldParseValueFromString -- see documentation in fbld.h
FbldValue* FbldParseValueFromString(FblcArena* arena, const char* string)
{
  Lex lex = {
    .c = START_VALUE,
    .loc = { .source = string, .line = 1, .col = 0 },
    .fin = NULL,
    .sin = string
  };
  ParseResult result;
  result.value = NULL;
  yyparse(arena, &lex, &result);
  return result.value;
}

// FbldParseQRefFromString -- see documentation in fbld.h
FbldQRef* FbldParseQRefFromString(FblcArena* arena, const char* string)
{
  Lex lex = {
    .c = START_QNAME,
    .loc = { .source = string, .line = 1, .col = 0 },
    .fin = NULL,
    .sin = string
  };
  ParseResult result;
  result.qref = NULL;
  yyparse(arena, &lex, &result);
  return result.qref;
}
