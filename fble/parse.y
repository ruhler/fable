// parse.y --
//   This file describes the bison grammar for parsing fble expressions.
%{
  #include <assert.h>   // for assert
  #include <ctype.h>    // for isalnum
  #include <stdbool.h>  // for bool
  #include <stdio.h>    // for FILE, fprintf, stderr
  #include <string.h>   // for strchr, strcpy

  #include "fble.h"

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
    FbleLoc loc;
    FILE* fin;
    const char* sin;
  } Lex;

  #define YYLTYPE FbleLoc

  #define YYLLOC_DEFAULT(Cur, Rhs, N)  \
  do                                   \
    if (N) (Cur) = YYRHSLOC(Rhs, 1);   \
    else   (Cur) = YYRHSLOC(Rhs, 0);   \
  while (0)
%}

%union {
  FbleName name;
  FbleKind* kind;
  FbleKindV kinds;
  FbleType* type;
  FbleTypeV types;
  FbleExpr* expr;
  FbleExprV exprs;
  FbleField field;
  FbleFieldV fields;
  FbleTypeFieldV type_fields;
  FbleChoice choice;
  FbleChoiceV choices;
  FbleBindingV bindings;
  FbleTypeBindingV type_bindings;
}

%{
  static bool IsNameChar(int c);
  static bool IsSingleChar(int c);
  static void ReadNextChar(Lex* lex);
  static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FbleArena* arena, const char* include_path, Lex* lex);
  static void yyerror(YYLTYPE* llocp, FbleArena* arena, const char* include_path, Lex* lex, FbleExpr** result, const char* msg);
%}

%locations
%define api.pure
%define parse.error verbose
%param {FbleArena* arena} {const char* include_path} {Lex* lex}
%parse-param {FbleExpr** result}

%token END 0 "end of file"
%token INVALID "invalid character"

%token <name> NAME

%type <name> type_name
%type <kind> kind
%type <kinds> kind_p
%type <type> type type_stmt 
%type <types> type_p
%type <expr> expr stmt
%type <exprs> expr_s expr_p
%type <field> struct_field
%type <fields> field_p field_s struct_field_p struct_field_s
%type <type_fields> type_field_p
%type <choice> anon_struct_arg
%type <choices> choices anon_struct_arg_s anon_struct_arg_p
%type <bindings> let_bindings exec_bindings
%type <type_bindings> type_let_bindings

%%

start: stmt { *result = $1; } ;

type_name: NAME '@' { $$ = $1; } ;

kind:
   '@' {
      FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
      basic_kind->_base.tag = FBLE_BASIC_KIND;
      basic_kind->_base.loc = @$;
      $$ = &basic_kind->_base;
   }
 | '\\' '<' kind_p ';' kind '>' {
      FblePolyKind* poly_kind = FbleAlloc(arena, FblePolyKind);
      poly_kind->_base.tag = FBLE_POLY_KIND;
      poly_kind->_base.loc = @$;
      poly_kind->args = $3;
      poly_kind->rkind = $5;
      $$ = &poly_kind->_base;
   }
 ;

kind_p:
   kind {
     FbleVectorInit(arena, $$);
     FbleVectorAppend(arena, $$, $1);
   }
 | kind_p ',' kind {
     $$ = $1;
     FbleVectorAppend(arena, $$, $3);
   }
 ;

type:
   '*' '(' struct_field_s ')' {
      FbleStructType* struct_type = FbleAlloc(arena, FbleStructType);
      struct_type->_base.tag = FBLE_STRUCT_TYPE;
      struct_type->_base.loc = @$;
      struct_type->fields = $3;
      $$ = &struct_type->_base;
   }
 | '+' '(' field_p ')' {
      FbleUnionType* union_type = FbleAlloc(arena, FbleUnionType);
      union_type->_base.tag = FBLE_UNION_TYPE;
      union_type->_base.loc = @$;
      union_type->fields = $3;
      $$ = &union_type->_base;
   }
 | '\\' '(' field_s ';' type ')' {
      FbleFuncType* func_type = FbleAlloc(arena, FbleFuncType);
      func_type->_base.tag = FBLE_FUNC_TYPE;
      func_type->_base.loc = @$;
      func_type->args = $3;
      func_type->rtype = $5;
      $$ = &func_type->_base;
   }
 | type '!' {
      FbleProcType* proc_type = FbleAlloc(arena, FbleProcType);
      proc_type->_base.tag = FBLE_PROC_TYPE;
      proc_type->_base.loc = @$;
      proc_type->rtype = $1;
      $$ = &proc_type->_base;
   }
 | type '-' {
      FbleInputType* input_type = FbleAlloc(arena, FbleInputType);
      input_type->_base.tag = FBLE_INPUT_TYPE;
      input_type->_base.loc = @$;
      input_type->type = $1;
      $$ = &input_type->_base;
   }
 | type '+' {
      FbleOutputType* output_type = FbleAlloc(arena, FbleOutputType);
      output_type->_base.tag = FBLE_OUTPUT_TYPE;
      output_type->_base.loc = @$;
      output_type->type = $1;
      $$ = &output_type->_base;
   }
 | type_name {
      FbleVarType* var_type = FbleAlloc(arena, FbleVarType);
      var_type->_base.tag = FBLE_VAR_TYPE;
      var_type->_base.loc = @$;
      var_type->var = $1;
      $$ = &var_type->_base;
   }
 | '\\' '<' type_field_p '>' '{' type_stmt '}' {
      FblePolyType* poly_type = FbleAlloc(arena, FblePolyType);
      poly_type->_base.tag = FBLE_POLY_TYPE;
      poly_type->_base.loc = @$;
      poly_type->args = $3;
      poly_type->body = $6;
      $$ = &poly_type->_base;
   }
 | type '<' type_p '>' {
      FblePolyApplyType* poly_apply_type = FbleAlloc(arena, FblePolyApplyType);
      poly_apply_type->_base.tag = FBLE_POLY_APPLY_TYPE;
      poly_apply_type->_base.loc = @$;
      poly_apply_type->poly = $1;
      poly_apply_type->args = $3;
      $$ = &poly_apply_type->_base;
   }
 | '{' type_stmt '}' {
      $$ = $2;
   }
 | type '.'  type_name {
      assert(false && "TODO: Support type field access?");
      $$ = NULL;
   }
 | expr '.'  type_name {
      assert(false && "TODO: Support type field access");
      $$ = NULL;
   }
 | '&' type_name {
      assert(false && "TODO: Support type include?");
      $$ = NULL;
   }
 ;

type_stmt:
    type ';' { $$ = $1; }
  | type_let_bindings ';' type_stmt {
      FbleLetType* let_type = FbleAlloc(arena, FbleLetType);
      let_type->_base.tag = FBLE_LET_TYPE;
      let_type->_base.loc = @$;
      let_type->bindings = $1;
      let_type->body = $3;
      $$ = &let_type->_base;
    }  
  ;

type_p:
   type {
     FbleVectorInit(arena, $$);
     FbleVectorAppend(arena, $$, $1);
   }
 | type_p ',' type {
     $$ = $1;
     FbleVectorAppend(arena, $$, $3);
   }
 ;

type_field_p:
   kind type_name {
     FbleVectorInit(arena, $$);
     FbleTypeField* field = FbleVectorExtend(arena, $$);
     field->kind = $1;
     field->name = $2;
   }
 | type_field_p ',' kind type_name {
     $$ = $1;
     FbleTypeField* field = FbleVectorExtend(arena, $$);
     field->kind = $3;
     field->name = $4;
   }
 ;

struct_field_s:
   %empty { FbleVectorInit(arena, $$); }
 | struct_field_p { $$ = $1; }
 ;

struct_field_p:
   struct_field {
     FbleVectorInit(arena, $$);
     FbleVectorAppend(arena, $$, $1);
   }
 | struct_field_p ',' struct_field {
     $$ = $1;
     FbleVectorAppend(arena, $$, $3);
   }
 ;

struct_field:
   type NAME {
     $$.type = $1;
     $$.name = $2;
   }
 | kind type_name '=' type {
     assert(false && "TODO: struct type field");
   }
 ;

field_s:
   %empty { FbleVectorInit(arena, $$); }
 | field_p { $$ = $1; }
 ;

field_p:
   type NAME {
     FbleVectorInit(arena, $$);
     FbleField* field = FbleVectorExtend(arena, $$);
     field->type = $1;
     field->name = $2;
   }
 | field_p ',' type NAME {
     $$ = $1;
     FbleField* field = FbleVectorExtend(arena, $$);
     field->type = $3;
     field->name = $4;
   }
 ;

expr:
   type '(' expr_s ')' {
      FbleStructValueExpr* struct_value_expr = FbleAlloc(arena, FbleStructValueExpr);
      struct_value_expr->_base.tag = FBLE_STRUCT_VALUE_EXPR;
      struct_value_expr->_base.loc = @$;
      struct_value_expr->type = $1;
      struct_value_expr->args = $3;
      $$ = &struct_value_expr->_base;
   }
 | '@' '(' anon_struct_arg_s ')' {
      // TODO: Support type args too.
      FbleAnonStructValueExpr* expr = FbleAlloc(arena, FbleAnonStructValueExpr);
      expr->_base.tag = FBLE_ANON_STRUCT_VALUE_EXPR;
      expr->_base.loc = @$;
      expr->args = $3;
      $$ = &expr->_base;
   }
 | type '(' NAME ':' expr ')' {
      FbleUnionValueExpr* union_value_expr = FbleAlloc(arena, FbleUnionValueExpr);
      union_value_expr->_base.tag = FBLE_UNION_VALUE_EXPR;
      union_value_expr->_base.loc = @$;
      union_value_expr->type = $1;
      union_value_expr->field = $3;
      union_value_expr->arg = $5;
      $$ = &union_value_expr->_base;
   }
 | expr '.' NAME {
      FbleAccessExpr* access_expr = FbleAlloc(arena, FbleAccessExpr);
      access_expr->_base.tag = FBLE_ACCESS_EXPR;
      access_expr->_base.loc = @$;
      access_expr->object = $1;
      access_expr->field = $3;
      $$ = &access_expr->_base;
   }
 | '?' '(' expr ';' choices ')' {
      FbleCondExpr* cond_expr = FbleAlloc(arena, FbleCondExpr);
      cond_expr->_base.tag = FBLE_COND_EXPR;
      cond_expr->_base.loc = @$;
      cond_expr->condition = $3;
      cond_expr->choices = $5;
      $$ = &cond_expr->_base;
   }
 | '\\' '(' field_s ')' '{' stmt '}' {
      FbleFuncValueExpr* func_value_expr = FbleAlloc(arena, FbleFuncValueExpr);
      func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
      func_value_expr->_base.loc = @$;
      func_value_expr->args = $3;
      func_value_expr->body = $6;
      $$ = &func_value_expr->_base;
   }
 | expr '(' expr_s ')' {
      FbleApplyExpr* apply_expr = FbleAlloc(arena, FbleApplyExpr);
      apply_expr->_base.tag = FBLE_APPLY_EXPR;
      apply_expr->_base.loc = @$;
      apply_expr->func = $1;
      apply_expr->args = $3;
      $$ = &apply_expr->_base;
   }
 | '$' '(' expr ')' {
      FbleEvalExpr* eval_expr = FbleAlloc(arena, FbleEvalExpr);
      eval_expr->_base.tag = FBLE_EVAL_EXPR;
      eval_expr->_base.loc = @$;
      eval_expr->expr = $3;
      $$ = &eval_expr->_base;
   }
 | NAME {
      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      $$ = &var_expr->_base;
   }
 | '\\' '<' type_field_p '>' '{' stmt '}' {
      FblePolyExpr* poly_expr = FbleAlloc(arena, FblePolyExpr);
      poly_expr->_base.tag = FBLE_POLY_EXPR;
      poly_expr->_base.loc = @$;
      poly_expr->args = $3;
      poly_expr->body = $6;
      $$ = &poly_expr->_base;
   }
 | expr '<' type_p '>' {
      FblePolyApplyExpr* poly_apply_expr = FbleAlloc(arena, FblePolyApplyExpr);
      poly_apply_expr->_base.tag = FBLE_POLY_APPLY_EXPR;
      poly_apply_expr->_base.loc = @$;
      poly_apply_expr->poly = $1;
      poly_apply_expr->args = $3;
      $$ = &poly_apply_expr->_base;
   }
 | '{' stmt '}' {
      $$ = $2;
   }
 | expr '{' stmt '}' {
      FbleNamespaceExpr* expr = FbleAlloc(arena, FbleNamespaceExpr);
      expr->_base.tag = FBLE_NAMESPACE_EVAL_EXPR;
      expr->_base.loc = @$;
      expr->nspace = $1;
      expr->body = $3;
      $$ = &expr->_base;
   }
 | '&' NAME {
      FbleReportError("%s not found for include\n", &(@$), $2);
      YYERROR;
   }
 ;


stmt:
    expr ';' { $$ = $1; }
  | field_p '<' '-' expr ';' stmt {
      FbleFuncValueExpr* func_value_expr = FbleAlloc(arena, FbleFuncValueExpr);
      func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
      func_value_expr->_base.loc = @$;
      func_value_expr->args = $1;
      func_value_expr->body = $6;

      FbleApplyExpr* apply_expr = FbleAlloc(arena, FbleApplyExpr);
      apply_expr->_base.tag = FBLE_APPLY_EXPR;
      apply_expr->_base.loc = @$;
      apply_expr->func = $4;
      FbleVectorInit(arena, apply_expr->args);
      FbleVectorAppend(arena, apply_expr->args, &func_value_expr->_base);
      $$ = &apply_expr->_base;
    }
  | type '~' NAME ',' NAME ';' stmt {
      FbleLinkExpr* link_expr = FbleAlloc(arena, FbleLinkExpr);
      link_expr->_base.tag = FBLE_LINK_EXPR;
      link_expr->_base.loc = @$;
      link_expr->type = $1;
      link_expr->get = $3;
      link_expr->put = $5;
      link_expr->body = $7;
      $$ = &link_expr->_base;
    }
  | exec_bindings ';' stmt {
      FbleExecExpr* exec_expr = FbleAlloc(arena, FbleExecExpr);
      exec_expr->_base.tag = FBLE_EXEC_EXPR;
      exec_expr->_base.loc = @$;
      exec_expr->bindings = $1;
      exec_expr->body = $3;
      $$ = &exec_expr->_base;
    }
  | let_bindings ';' stmt {
      FbleLetExpr* let_expr = FbleAlloc(arena, FbleLetExpr);
      let_expr->_base.tag = FBLE_LET_EXPR;
      let_expr->_base.loc = @$;
      let_expr->bindings = $1;
      let_expr->body = $3;
      $$ = &let_expr->_base;
    }  
  | type_let_bindings ';' stmt {
      FbleTypeLetExpr* let_expr = FbleAlloc(arena, FbleTypeLetExpr);
      let_expr->_base.tag = FBLE_TYPE_LET_EXPR;
      let_expr->_base.loc = @$;
      let_expr->bindings = $1;
      let_expr->body = $3;
      $$ = &let_expr->_base;
    }  
  | expr ';' stmt {
      FbleNamespaceExpr* expr = FbleAlloc(arena, FbleNamespaceExpr);
      expr->_base.tag = FBLE_NAMESPACE_IMPORT_EXPR;
      expr->_base.loc = @$;
      expr->nspace = $1;
      expr->body = $3;
      $$ = &expr->_base;
    }
  ;

expr_s:
   %empty { FbleVectorInit(arena, $$); }
 | expr_p { $$ = $1; }
 ;

expr_p:
   expr {
     FbleVectorInit(arena, $$);
     FbleVectorAppend(arena, $$, $1);
   }
 | expr_p ',' expr {
     $$ = $1;
     FbleVectorAppend(arena, $$, $3);
   }
 ;

choices:
    NAME ':' expr {
      FbleVectorInit(arena, $$);
      FbleChoice* choice = FbleVectorExtend(arena, $$);
      choice->name = $1;
      choice->expr = $3;
    }
  | choices ',' NAME ':' expr {
      $$ = $1;
      FbleChoice* choice = FbleVectorExtend(arena, $$);
      choice->name = $3;
      choice->expr = $5;
    }
  ;

anon_struct_arg:
    NAME {
      $$.name = $1;
      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      $$.expr = &var_expr->_base;
    }
  | NAME ':' expr {
      $$.name = $1;
      $$.expr = $3;
    }
  ;

anon_struct_arg_s:
    %empty { FbleVectorInit(arena, $$); }
  | anon_struct_arg_p { $$ = $1; }
  ;

anon_struct_arg_p:
  anon_struct_arg {
      FbleVectorInit(arena, $$);
      FbleVectorAppend(arena, $$, $1);
    }
  | anon_struct_arg_p ',' anon_struct_arg {
      $$ = $1;
      FbleVectorAppend(arena, $$, $3);
    }
  ;

let_bindings: 
  type NAME '=' expr {
      FbleVectorInit(arena, $$);
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->type = $1;
      binding->name = $2;
      binding->expr = $4;
    }
  | let_bindings ',' type NAME '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->type = $3;
      binding->name = $4;
      binding->expr = $6;
    }
  ;

type_let_bindings: 
  kind type_name '=' type {
      FbleVectorInit(arena, $$);
      FbleTypeBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = $1;
      binding->name = $2;
      binding->type = $4;
    }
  | type_let_bindings ',' kind type_name '=' type {
      $$ = $1;
      FbleTypeBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = $3;
      binding->name = $4;
      binding->type = $6;
    }
  ;

exec_bindings: 
  type NAME ':' '=' expr {
      FbleVectorInit(arena, $$);
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->type = $1;
      binding->name = $2;
      binding->expr = $5;
    }
  | exec_bindings ',' type NAME ':' '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->type = $3;
      binding->name = $4;
      binding->expr = $7;
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
  return strchr("(){};,:?=.<>+*-!$@~&\\", c) != NULL;
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
//   include_path - the include path to search for includes.
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
static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FbleArena* arena, const char* include_path, Lex* lex)
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

  llocp->source = lex->loc.source;
  llocp->line = lex->loc.line;
  llocp->col = lex->loc.col;
  
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
  FbleVectorInit(arena, namev);
  while (IsNameChar(lex->c)) {
    FbleVectorAppend(arena, namev, lex->c);
    ReadNextChar(lex);
  }
  FbleVectorAppend(arena, namev, '\0');
  lvalp->name.name = namev.xs;
  lvalp->name.loc = *llocp;

  return NAME;
}

// yyerror --
//   The error reporting function for the bison generated parser.
// 
// Inputs:
//   llocp - The location of the error.
//   arena - unused.
//   include_path - unused.
//   lex - unused.
//   result - unused.
//   msg - The error message.
//
// Results:
//   None.
//
// Side effects:
//   An error message is printed to stderr.
static void yyerror(YYLTYPE* llocp, FbleArena* arena, const char* include_path, Lex* lex, FbleExpr** result, const char* msg)
{
  FbleReportError("%s\n", llocp, msg);
}

// FbleParse -- see documentation in fble.h
FbleExpr* FbleParse(FbleArena* arena, const char* filename, const char* include_path)
{
  FILE* fin = fopen(filename, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename);
    return NULL;
  }

  // Make a copy of the filename for locations so that the user doesn't have
  // to worry about keeping it alive for the duration of the program lifetime.
  char* source = FbleArenaAlloc(arena, sizeof(char) * (strlen(filename) + 1), FbleAllocMsg(__FILE__, __LINE__));
  strcpy(source, filename);

  Lex lex = {
    .c = ' ',
    .loc = { .source = source, .line = 1, .col = 0 },
    .fin = fin,
    .sin = NULL
  };
  FbleExpr* result = NULL;
  yyparse(arena, include_path, &lex, &result);
  return result;
}
