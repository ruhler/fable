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
    FbldProgram* prgm;
    FbldValue* value;
    FbldQRef* qref;
  } ParseResult;

  #define YYLTYPE FbldLoc*

  #define YYLLOC_DEFAULT(Cur, Rhs, N)  \
  do                                   \
    if (N) (Cur) = YYRHSLOC(Rhs, 1);   \
    else   (Cur) = YYRHSLOC(Rhs, 0);   \
  while (0)
%}

%union {
  FbldName* name;
  FbldIdV idv;
  FbldQRef* qref;
  FbldQRefV* qrefv;
  FbldArgV* argv;
  FbldExpr* expr;
  FbldExprV* exprv;
  FbldActn* actn;
  FbldActnV* actnv;
  FbldExecV* execv;
  FbldImportItemV* iitemv;
  FbldImport* import;
  FbldDecl* decl;
  FbldDeclV* declv;
  FbldPolarity polarity;
  FbldPortV* portv;
  FbldProgram* program;
  FbldValue* value;
  FbldValueV* valuev;
  FbldAccess access;
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
%token START_PROGRAM 1 "program parse start indicator"
%token START_VALUE 2 "value parse start indicator"
%token START_QNAME 3 "qref parse start indicator"

%token <name> NAME

// Keywords. These have type 'name' because in many contexts they are treated
// as normal names rather than keywords.
%token <name> INTERF "interf"
%token <name> MODULE "module"
%token <name> TYPE "type"
%token <name> STRUCT "struct"
%token <name> UNION "union"
%token <name> FUNC "func"
%token <name> PROC "proc"
%token <name> IMPORT "import"
%token <name> PRIV "priv"
%token <name> ABST "abst"

%type <name> keyword name
%type <idv> id_list non_empty_id_list
%type <qref> qref module_defn_iref
%type <qrefv> qref_list sargs
%type <argv> arg_list non_empty_arg_list
%type <expr> expr stmt
%type <exprv> expr_list non_empty_expr_list
%type <actn> actn pstmt
%type <actnv> non_empty_actn_list
%type <execv> non_empty_exec_list
%type <iitemv> import_item_list
%type <import> import
%type <declv> params param_list
%type <polarity> polarity
%type <portv> port_list non_empty_port_list
%type <decl> abstract_type_proto struct_proto union_proto type_proto
%type <decl> func_proto proc_proto interf_proto module_proto proto 
%type <decl> type_decl func_decl proc_decl interf_decl module_decl plain_decl decl
%type <program> proto_env decl_env module_body
%type <value> value
%type <valuev> value_list non_empty_value_list
%type <access> access

%%

// This grammar is used for parsing starting from multiple different start
// tokens. We insert an arbitrary, artificial single-character token to
// indicate which start token we actually want to use.
start:
     START_PROGRAM decl_env { result->prgm = $2; }
   | START_VALUE value { result->value = $2; }
   | START_QNAME qref { result->qref = $2; }
   ;

keyword: "interf" | "module" | "type" | "struct" | "union" | "func" | "proc" | "import" ;

name: NAME | keyword ;

id_list:
    %empty {
      FblcVectorInit(arena, $$);
    }
  | non_empty_id_list
  ;

non_empty_id_list:
  name {
      FblcVectorInit(arena, $$);
      FbldId* id = FblcVectorExtend(arena, $$);
      id->name = $1;
      id->id = FBLC_NULL_ID;
    }
  | non_empty_id_list ',' name {
      $$ = $1;
      FbldId* id = FblcVectorExtend(arena, $$);
      id->name = $3;
      id->id = FBLC_NULL_ID;
    }
  ;

sargs: 
    %empty {
      $$ = FBLC_ALLOC(arena, FbldQRefV);
      FblcVectorInit(arena, *$$);
    }
  | '<' qref_list '>' {
      $$ = $2;
    }
  ;

qref:
    name sargs {
      $$ = FBLC_ALLOC(arena, FbldQRef);
      $$->name = $1;
      $$->paramv = $2;
      $$->mref = NULL;
      $$->r = NULL;
    }
  | name sargs '@' qref {
      $$ = FBLC_ALLOC(arena, FbldQRef);
      $$->name = $1;
      $$->paramv = $2;
      $$->mref = $4;
      $$->r = NULL;
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
  | '-' name '(' ')' {
      FbldGetActn* get_actn = FBLC_ALLOC(arena, FbldGetActn);
      get_actn->_base.tag = FBLD_GET_ACTN;
      get_actn->_base.loc = @$;
      get_actn->port.name = $2;
      get_actn->port.id = FBLC_NULL_ID;
      $$ = &get_actn->_base;
    }
  | '+' name '(' expr ')' {
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

pstmt: actn ';' { $$ = $1; }
     | qref '+' '-' name ',' name ';' pstmt {
         FbldLinkActn* link_actn = FBLC_ALLOC(arena, FbldLinkActn);
         link_actn->_base.tag = FBLD_LINK_ACTN;
         link_actn->_base.loc = @$;
         link_actn->type = $1;
         link_actn->put = $4;
         link_actn->get = $6;
         link_actn->body = $8;
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

import_item_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldImportItemV);
      FblcVectorInit(arena, *$$);
    }
  | import_item_list qref ';' {
      FbldImportItem* item = FBLC_ALLOC(arena, FbldImportItem);
      item->source = $2;
      item->dest = $2->name;
      FblcVectorAppend(arena, *$1, item);
      $$ = $1;
    }
  | import_item_list name '=' qref ';' {
      FbldImportItem* item = FBLC_ALLOC(arena, FbldImportItem);
      item->source = $4;
      item->dest = $2;
      FblcVectorAppend(arena, *$1, item);
      $$ = $1;
    }
  ;

import: "import" qref '{' import_item_list '}' {
      $$ = FBLC_ALLOC(arena, FbldImport);
      $$->mref = $2;
      $$->itemv = $4;
    }
  | "import" '@' '{' import_item_list '}' {
      $$ = FBLC_ALLOC(arena, FbldImport);
      $$->mref = NULL;
      $$->itemv = $4;
    }
  ;

params: 
    %empty {
      $$ = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *$$);
    }
  | '<' param_list '>' {
      $$ = $2;
    }
  ;

param_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *$$);
    }
  | proto {
      $$ = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | param_list ',' proto {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

abstract_type_proto: "type" name params {
      FbldType* type = FBLC_ALLOC(arena, FbldType);
      type->_base.tag = FBLD_TYPE_DECL;
      type->_base.name = $2;
      type->_base.paramv = $3;
      type->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
      type->kind = FBLD_ABSTRACT_KIND;
      type->fieldv = NULL;
      $$ = &type->_base;
    }
  ;

struct_proto: "struct" name params '(' arg_list ')' {
      FbldType* type = FBLC_ALLOC(arena, FbldType);
      type->_base.tag = FBLD_TYPE_DECL;
      type->_base.name = $2;
      type->_base.paramv = $3;
      type->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
      type->kind = FBLD_STRUCT_KIND;
      type->fieldv = $5;
      $$ = &type->_base;
    }
  ;

union_proto: "union" name params '(' non_empty_arg_list ')' {
      FbldType* type = FBLC_ALLOC(arena, FbldType);
      type->_base.tag = FBLD_TYPE_DECL;
      type->_base.name = $2;
      type->_base.paramv = $3;
      type->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
      type->kind = FBLD_UNION_KIND;
      type->fieldv = $5;
      $$ = &type->_base;
    }
  ;

type_proto: abstract_type_proto | struct_proto | union_proto ;

func_proto: "func" name params '(' arg_list ';' qref ')' {
      FbldFunc* func = FBLC_ALLOC(arena, FbldFunc);
      func->_base.tag = FBLD_FUNC_DECL;
      func->_base.name = $2;
      func->_base.paramv = $3;
      func->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
      func->argv = $5;
      func->return_type = $7;
      func->body = NULL;
      $$ = &func->_base;
    }
  ;

polarity: '-' { $$ = FBLD_GET_POLARITY; }
        | '+' { $$ = FBLD_PUT_POLARITY; }
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

proc_proto: "proc" name params '(' port_list ';' arg_list ';' qref ')' {
      FbldProc* proc = FBLC_ALLOC(arena, FbldProc);
      proc->_base.tag = FBLD_PROC_DECL;
      proc->_base.name = $2;
      proc->_base.paramv = $3;
      proc->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
      proc->portv = $5;
      proc->argv = $7;
      proc->return_type = $9;
      proc->body = NULL;
      $$ = &proc->_base;
    }
  ;

interf_proto: "interf" name params '{' proto_env '}' {
          FbldInterf* interf = FBLC_ALLOC(arena, FbldInterf);
          interf->_base.tag = FBLD_INTERF_DECL;
          interf->_base.name = $2;
          interf->_base.paramv = $3;
          interf->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
          interf->body = $5;
          $$ = &interf->_base;
        }
     ;

module_proto: "module" name params '(' qref ')' {
          FbldModule* module = FBLC_ALLOC(arena, FbldModule);
          module->_base.tag = FBLD_MODULE_DECL;
          module->_base.name = $2;
          module->_base.paramv = $3;
          module->_base.access = FBLD_PUBLIC_ACCESS;  // TODO: What to put here?
          module->iref = $5;
          module->body = NULL;
          $$ = &module->_base;
        }
     ;

proto: type_proto | func_proto | proc_proto | interf_proto | module_proto ;

type_decl: struct_proto | union_proto ;

func_decl: func_proto expr {
      FbldFunc* func = (FbldFunc*)$1;
      func->body = $2;
      $$ = &func->_base;
    }
  ;

proc_decl: proc_proto actn {
      FbldProc* proc = (FbldProc*)$1;
      proc->body = $2;
      $$ = &proc->_base;
    }
  ;

interf_decl: interf_proto ;

module_defn_iref: 
     %empty      { $$ = NULL; }
  | '(' qref ')' { $$ = $2; }
  ;

module_body:
     %empty           { $$ = NULL; }
  | '{' decl_env '}' { $$ = $2; }
  ;

module_decl: "module" name params module_defn_iref module_body {
      FbldModule* module = FBLC_ALLOC(arena, FbldModule);
      module->_base.tag = FBLD_MODULE_DECL;
      module->_base.name = $2;
      module->_base.paramv = $3;
      module->_base.access = FBLD_PUBLIC_ACCESS;
      module->iref = $4;
      module->body = $5;
      $$ = &module->_base;
    }
  ;

plain_decl: type_decl | func_decl | proc_decl | interf_decl | module_decl ;

access:
   %empty { $$ = FBLD_PUBLIC_ACCESS; }
 | "priv" { $$ = FBLD_PRIVATE_ACCESS; }
 | "abst" { $$ = FBLD_ABSTRACT_ACCESS; }
 ;

decl: access plain_decl {
    $$ = $2;
    $$->access = $1;
  }
 ;

proto_env:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldProgram);
      $$->importv = FBLC_ALLOC(arena, FbldImportV);
      $$->declv = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *($$->importv));
      FblcVectorInit(arena, *($$->declv));
    }
  | proto_env import ';' {
      FblcVectorAppend(arena, *($1->importv), $2);
      $$ = $1;
    }
  | proto_env proto ';' {
      FblcVectorAppend(arena, *($1->declv), $2);
      $$ = $1;
    }
  ;

decl_env:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldProgram);
      $$->importv = FBLC_ALLOC(arena, FbldImportV);
      $$->declv = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *($$->importv));
      FblcVectorInit(arena, *($$->declv));
    }
  | decl_env import ';' {
      FblcVectorAppend(arena, *($1->importv), $2);
      $$ = $1;
    }
  | decl_env decl ';' {
      FblcVectorAppend(arena, *($1->declv), $2);
      $$ = $1;
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

// IsSingleChar --
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
  return strchr("(){};,@:?=.<>+-$", c) != NULL
    || c == START_PROGRAM
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
    {.keyword = "interf", .symbol = INTERF},
    {.keyword = "module", .symbol = MODULE},
    {.keyword = "type", .symbol = TYPE},
    {.keyword = "struct", .symbol = STRUCT},
    {.keyword = "union", .symbol = UNION},
    {.keyword = "func", .symbol = FUNC},
    {.keyword = "proc", .symbol = PROC},
    {.keyword = "import", .symbol = IMPORT},
    {.keyword = "priv", .symbol = PRIV},
    {.keyword = "abst", .symbol = ABST}
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

// FbldParseProgram -- see documentation in fbld.h
FbldProgram* FbldParseProgram(FblcArena* arena, const char* filename)
{
  FILE* fin = fopen(filename, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename);
    return NULL;
  }

  // Make a copy of the filename for locations so that the user doesn't have
  // to worry about keeping it alive for the duration of the program lifetime.
  char* source = arena->alloc(arena, sizeof(char) * (strlen(filename) + 1), FBLC_ALLOC_MSG(__FILE__, __LINE__));
  strcpy(source, filename);

  Lex lex = {
    .c = START_PROGRAM,
    .loc = { .source = source, .line = 1, .col = 0 },
    .fin = fin,
    .sin = NULL
  };
  ParseResult result;
  result.prgm = NULL;
  return (0 == yyparse(arena, &lex, &result)) ? result.prgm : NULL;
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
  return (0 == yyparse(arena, &lex, &result)) ? result.value : NULL;
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
  return (0 == yyparse(arena, &lex, &result)) ? result.qref : NULL;
}
