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
  const char* word;
  FbleName name;
  FbleNameV names;
  FbleKind* kind;
  FbleKindV kinds;
  FbleTypeFieldV type_fields;
  FbleExpr* expr;
  FbleExprV exprs;
  FbleField field;
  FbleFieldV fields;
  FbleChoice choice;
  FbleChoiceV choices;
  FbleBindingV bindings;
}

%{
  static bool IsSpaceChar(int c);
  static bool IsPunctuationChar(int c);
  static bool IsNormalChar(int c);
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

%token <word> WORD

%type <name> name
%type <names> path abs_path rel_path
%type <kind> kind
%type <kinds> kind_p
%type <type_fields> type_field_p
%type <expr> expr block stmt
%type <exprs> expr_p expr_s
%type <field> field
%type <fields> field_p field_s
%type <choice> implicit_choice choice
%type <choices> implicit_choice_p implicit_choice_s choice_p
%type <bindings> exec_binding_p let_binding_p

%%

start: stmt { *result = $1; } ;

name:
   WORD {
     $$.name = $1;
     $$.space = FBLE_NORMAL_NAME_SPACE;
     $$.loc = @$;
   }
 | WORD '@' {
     $$.name = $1;
     $$.space = FBLE_TYPE_NAME_SPACE;
     $$.loc = @$;
   }
 ;

rel_path:
   WORD '%' {
     assert(false && "TODO: rel_path");
   }
 | WORD '/' abs_path {
     assert(false && "TODO: rel_path");
   };

abs_path:
   '/' rel_path {
     assert(false && "TODO: abs_path");
   };

path:
   abs_path { $$ = $1; }
 | rel_path { $$ = $1; }

kind:
   '@' {
      FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
      basic_kind->_base.tag = FBLE_BASIC_KIND;
      basic_kind->_base.loc = @$;
      $$ = &basic_kind->_base;
   }
 | '<' kind_p '>' kind {
      FbleKind* kind = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleKind* arg = $2.xs[$2.size - 1 - i];
        FblePolyKind* poly_kind = FbleAlloc(arena, FblePolyKind);
        poly_kind->_base.tag = FBLE_POLY_KIND;
        poly_kind->_base.loc = @$;
        poly_kind->arg = arg;
        poly_kind->rkind = kind;
        kind = &poly_kind->_base;
      }
      FbleFree(arena, $2.xs);
      $$ = kind;
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

type_field_p:
   kind name {
     FbleVectorInit(arena, $$);
     FbleTypeField* type_field = FbleVectorExtend(arena, $$);
     type_field->kind = $1;
     type_field->name = $2;
   }
 | type_field_p ',' kind name {
     $$ = $1;
     FbleTypeField* type_field = FbleVectorExtend(arena, $$);
     type_field->kind = $3;
     type_field->name = $4;
   }
 ;

expr:
   '@' '<' expr '>' {
     FbleTypeofExpr* typeof = FbleAlloc(arena, FbleTypeofExpr);
     typeof->_base.tag = FBLE_TYPEOF_EXPR;
     typeof->_base.loc = @$;
     typeof->expr = $3;
     $$ = &typeof->_base;
   }
 | name {
      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      $$ = &var_expr->_base;
   }
 | path {
      assert(false && "TODO: parse module path");
      $$ = NULL;
   }
 | '*' '(' field_s ')' {
      FbleStructTypeExpr* struct_type = FbleAlloc(arena, FbleStructTypeExpr);
      struct_type->_base.tag = FBLE_STRUCT_TYPE_EXPR;
      struct_type->_base.loc = @$;
      struct_type->fields = $3;
      $$ = &struct_type->_base;
   }
 | expr '(' expr_s ')' {
      FbleMiscApplyExpr* misc_apply_expr = FbleAlloc(arena, FbleMiscApplyExpr);
      misc_apply_expr->_base.tag = FBLE_MISC_APPLY_EXPR;
      misc_apply_expr->_base.loc = @$;
      misc_apply_expr->misc = $1;
      misc_apply_expr->args = $3;
      $$ = &misc_apply_expr->_base;
   }
 | '@' '(' implicit_choice_s ')' {
      FbleStructValueImplicitTypeExpr* expr = FbleAlloc(arena, FbleStructValueImplicitTypeExpr);
      expr->_base.tag = FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR;
      expr->_base.loc = @$;
      expr->args = $3;
      $$ = &expr->_base;
   }
 | expr '.' name {
      FbleMiscAccessExpr* access_expr = FbleAlloc(arena, FbleMiscAccessExpr);
      access_expr->_base.tag = FBLE_MISC_ACCESS_EXPR;
      access_expr->_base.loc = @$;
      access_expr->object = $1;
      access_expr->field = $3;
      $$ = &access_expr->_base;
   }
 | expr '{' stmt '}' {
      FbleStructEvalExpr* expr = FbleAlloc(arena, FbleStructEvalExpr);
      expr->_base.tag = FBLE_STRUCT_EVAL_EXPR;
      expr->_base.loc = @$;
      expr->nspace = $1;
      expr->body = $3;
      $$ = &expr->_base;
   }
 | '+' '(' field_p ')' {
      FbleUnionTypeExpr* union_type = FbleAlloc(arena, FbleUnionTypeExpr);
      union_type->_base.tag = FBLE_UNION_TYPE_EXPR;
      union_type->_base.loc = @$;
      union_type->fields = $3;
      $$ = &union_type->_base;
   }
 | expr '(' name ':' expr ')' {
      FbleUnionValueExpr* union_value_expr = FbleAlloc(arena, FbleUnionValueExpr);
      union_value_expr->_base.tag = FBLE_UNION_VALUE_EXPR;
      union_value_expr->_base.loc = @$;
      union_value_expr->type = $1;
      union_value_expr->field = $3;
      union_value_expr->arg = $5;
      $$ = &union_value_expr->_base;
   }
 | '?' '(' expr ';' choice_p ')' {
      FbleUnionSelectExpr* select_expr = FbleAlloc(arena, FbleUnionSelectExpr);
      select_expr->_base.tag = FBLE_UNION_SELECT_EXPR;
      select_expr->_base.loc = @$;
      select_expr->condition = $3;
      select_expr->choices = $5;
      $$ = &select_expr->_base;
   }
 | expr '!' {
      FbleUnaryTypeExpr* proc_type = FbleAlloc(arena, FbleUnaryTypeExpr);
      proc_type->_base.tag = FBLE_PROC_TYPE_EXPR;
      proc_type->_base.loc = @$;
      proc_type->type = $1;
      $$ = &proc_type->_base;
   }
 | expr '-' {
      FbleUnaryTypeExpr* input_type = FbleAlloc(arena, FbleUnaryTypeExpr);
      input_type->_base.tag = FBLE_INPUT_TYPE_EXPR;
      input_type->_base.loc = @$;
      input_type->type = $1;
      $$ = &input_type->_base;
   }
 | expr '+' {
      FbleUnaryTypeExpr* output_type = FbleAlloc(arena, FbleUnaryTypeExpr);
      output_type->_base.tag = FBLE_OUTPUT_TYPE_EXPR;
      output_type->_base.loc = @$;
      output_type->type = $1;
      $$ = &output_type->_base;
   }
 | '$' '(' expr ')' {
      FbleEvalExpr* eval_expr = FbleAlloc(arena, FbleEvalExpr);
      eval_expr->_base.tag = FBLE_EVAL_EXPR;
      eval_expr->_base.loc = @$;
      eval_expr->expr = $3;
      $$ = &eval_expr->_base;
   }
 | expr '<' expr_p '>' {
      $$ = $1;
      for (size_t i = 0; i < $3.size; ++i) {
        FblePolyApplyExpr* poly_apply_expr = FbleAlloc(arena, FblePolyApplyExpr);
        poly_apply_expr->_base.tag = FBLE_POLY_APPLY_EXPR;
        poly_apply_expr->_base.loc = @$;
        poly_apply_expr->poly = $$;
        poly_apply_expr->arg = $3.xs[i];
        $$ = &poly_apply_expr->_base;
      }
      FbleFree(arena, $3.xs);
   }
 | '[' expr_p ']' {
      FbleListExpr* list_expr = FbleAlloc(arena, FbleListExpr);
      list_expr->_base.tag = FBLE_LIST_EXPR;
      list_expr->_base.loc = @$;
      list_expr->args = $2;
      $$ = &list_expr->_base;
   }
 | expr '|' WORD {
      FbleLiteralExpr* literal_expr = FbleAlloc(arena, FbleLiteralExpr);
      literal_expr->_base.tag = FBLE_LITERAL_EXPR;
      literal_expr->_base.loc = @$;
      literal_expr->type = $1;
      literal_expr->word_loc = @3;
      literal_expr->word = $3;
      $$ = &literal_expr->_base;
   }
 | '&' name {
      if (include_path == NULL) {
        FbleReportError("%s not found for include\n", &(@$), $2);
        YYERROR;
      }

      char new_include_path[strlen(include_path) + strlen($2.name) + 3];
      strcpy(new_include_path, include_path);
      strcat(new_include_path, "/");
      strcat(new_include_path, $2.name);
      if ($2.space == FBLE_TYPE_NAME_SPACE) {
        strcat(new_include_path, "@");
      }

      char new_filename[strlen(new_include_path) + 6];
      strcpy(new_filename, new_include_path);
      strcat(new_filename, ".fble");

      $$ = FbleParse(arena, new_filename, new_include_path);
      if ($$ == NULL) {
        FbleReportError("(included from here)\n", &(@$));
        YYERROR;
      }
   }
 | block {
      $$ = $1;
   }
 ;

block:
   '{' stmt '}' {
      $$ = $2;
   }
 | '(' expr_p ')' block {
      FbleType* type = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleType* arg = $2.xs[$2.size - 1 - i];
        FbleFuncTypeExpr* func_type = FbleAlloc(arena, FbleFuncTypeExpr);
        func_type->_base.tag = FBLE_FUNC_TYPE_EXPR;
        func_type->_base.loc = arg->loc;
        func_type->arg = arg;
        func_type->rtype = type;
        type = &func_type->_base;
      }
      FbleFree(arena, $2.xs);
      $$ = type;
   }
 | '(' field_p ')' block {
      FbleFuncValueExpr* func_value_expr = FbleAlloc(arena, FbleFuncValueExpr);
      func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
      func_value_expr->_base.loc = @$;
      func_value_expr->args = $2;
      func_value_expr->body = $4;
      $$ = &func_value_expr->_base;
   }
 | '<' type_field_p '>' block {
      FbleExpr* expr = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleTypeField* arg = $2.xs + $2.size - 1 - i;
        FblePolyExpr* poly_expr = FbleAlloc(arena, FblePolyExpr);
        poly_expr->_base.tag = FBLE_POLY_EXPR;
        poly_expr->_base.loc = @$;
        poly_expr->arg.kind = arg->kind;
        poly_expr->arg.name = arg->name;
        poly_expr->body = expr;
        expr = &poly_expr->_base;
      }
      FbleFree(arena, $2.xs);
      $$ = expr;
   }
 ;

stmt:
    expr ';' { $$ = $1; }
  | let_binding_p ';' stmt {
      FbleLetExpr* let_expr = FbleAlloc(arena, FbleLetExpr);
      let_expr->_base.tag = FBLE_LET_EXPR;
      let_expr->_base.loc = @$;
      let_expr->bindings = $1;
      let_expr->body = $3;
      $$ = &let_expr->_base;
    }  
  | expr ';' stmt {
      FbleStructImportExpr* expr = FbleAlloc(arena, FbleStructImportExpr);
      expr->_base.tag = FBLE_STRUCT_IMPORT_EXPR;
      expr->_base.loc = @$;
      expr->nspace = $1;
      expr->body = $3;
      $$ = &expr->_base;
    }
  | field_p '<' '-' expr ';' stmt {
      FbleFuncValueExpr* func_value_expr = FbleAlloc(arena, FbleFuncValueExpr);
      func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
      func_value_expr->_base.loc = @$;
      func_value_expr->args = $1;
      func_value_expr->body = $6;

      FbleMiscApplyExpr* apply_expr = FbleAlloc(arena, FbleMiscApplyExpr);
      apply_expr->_base.tag = FBLE_MISC_APPLY_EXPR;
      apply_expr->_base.loc = @$;
      apply_expr->misc = $4;
      FbleVectorInit(arena, apply_expr->args);
      FbleVectorAppend(arena, apply_expr->args, &func_value_expr->_base);
      $$ = &apply_expr->_base;
    }
  | expr '~' name ',' name ';' stmt {
      FbleLinkExpr* link_expr = FbleAlloc(arena, FbleLinkExpr);
      link_expr->_base.tag = FBLE_LINK_EXPR;
      link_expr->_base.loc = @$;
      link_expr->type = $1;
      link_expr->get = $3;
      link_expr->put = $5;
      link_expr->body = $7;
      $$ = &link_expr->_base;
    }
  | exec_binding_p ';' stmt {
      FbleExecExpr* exec_expr = FbleAlloc(arena, FbleExecExpr);
      exec_expr->_base.tag = FBLE_EXEC_EXPR;
      exec_expr->_base.loc = @$;
      exec_expr->bindings = $1;
      exec_expr->body = $3;
      $$ = &exec_expr->_base;
    }
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

expr_s:
   %empty { FbleVectorInit(arena, $$); }
 | expr_p { $$ = $1; }
 ;

field:
   expr name {
     $$.type = $1;
     $$.name = $2;
   }
 ;

field_p:
   field {
     FbleVectorInit(arena, $$);
     FbleField* field = FbleVectorExtend(arena, $$);
     field->type = $1.type;
     field->name = $1.name;
   }
 | field_p ',' field {
     $$ = $1;
     FbleField* field = FbleVectorExtend(arena, $$);
     field->type = $3.type;
     field->name = $3.name;
   }
 ;

field_s:
   %empty { FbleVectorInit(arena, $$); }
 | field_p { $$ = $1; }
 ;

implicit_choice:
    name {
      $$.name = $1;
      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      $$.expr = &var_expr->_base;
    }
  | name ':' expr {
      $$.name = $1;
      $$.expr = $3;
    }
  ;

implicit_choice_p:
  implicit_choice {
      FbleVectorInit(arena, $$);
      FbleVectorAppend(arena, $$, $1);
    }
  | implicit_choice_p ',' implicit_choice {
      $$ = $1;
      FbleVectorAppend(arena, $$, $3);
    }
  ;

implicit_choice_s:
    %empty { FbleVectorInit(arena, $$); }
  | implicit_choice_p { $$ = $1; }
  ;

choice:
    name ':' expr {
      $$.name = $1;
      $$.expr = $3;
    }
  ;

choice_p:
  choice {
      FbleVectorInit(arena, $$);
      FbleVectorAppend(arena, $$, $1);
    }
  | choice_p ',' choice {
      $$ = $1;
      FbleVectorAppend(arena, $$, $3);
    }
  ;

exec_binding_p: 
  expr name ':' '=' expr {
      FbleVectorInit(arena, $$);
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = NULL;
      binding->type = $1;
      binding->name = $2;
      binding->expr = $5;
    }
  | exec_binding_p ',' expr name ':' '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = NULL;
      binding->type = $3;
      binding->name = $4;
      binding->expr = $7;
    }
  ;

let_binding_p: 
  expr name '=' expr {
      FbleVectorInit(arena, $$);
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = NULL;
      binding->type = $1;
      binding->name = $2;
      binding->expr = $4;
    }
  | kind name '=' expr {
      FbleVectorInit(arena, $$);
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = $1;
      binding->type = NULL;
      binding->name = $2;
      binding->expr = $4;
    }
  | let_binding_p ',' expr name '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = NULL;
      binding->type = $3;
      binding->name = $4;
      binding->expr = $6;
    }
  | let_binding_p ',' kind name '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->kind = $3;
      binding->type = NULL;
      binding->name = $4;
      binding->expr = $6;
    }
  ;

%%

// IsSpaceChar --
//   Tests whether a character is whitespace.
//
// Inputs:
//   c - The character to test. This must have a value of an unsigned char or
//       EOF.
//
// Result:
//   The value true if the character is whitespace, false otherwise.
//
// Side effects:
//   None.
static bool IsSpaceChar(int c)
{
  return isspace(c);
}

// IsPunctuationChar --
//   Tests whether a character is one of the single-character punctuation tokens.
//
// Inputs:
//   c - The character to test. This must have a value of an unsigned char or
//       EOF.
//
// Result:
//   The value true if the character is one of the fble punctuation tokens,
//   false otherwise.
//
// Side effects:
//   None.
static bool IsPunctuationChar(int c)
{
  return strchr("(){}[];,:?=.<>+*-!$@~&|%/", c) != NULL;
}

// IsNormalChar --
//   Tests whether a character is a normal character suitable for direct use
//   in fble words.
//
// Inputs:
//   c - The character to test. This must have a value of an unsigned char or
//       EOF.
//
// Result:
//   The value true if the character is a normal character, false otherwise.
//
// Side effects:
//   None.
static bool IsNormalChar(int c)
{
  if (IsSpaceChar(c) || IsPunctuationChar(c) || c == '\'' || c == '#') {
    return false;
  }
  return true;
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

  if (IsPunctuationChar(lex->c)) {
    int c = lex->c;
    ReadNextChar(lex);
    return c;
  };

  struct { size_t size; char* xs; } wordv;
  FbleVectorInit(arena, wordv);

  if (lex->c == '\'') {
    do {
      ReadNextChar(lex);
      while (lex->c != '\'') {
        FbleVectorAppend(arena, wordv, lex->c);
        ReadNextChar(lex);
      }
      ReadNextChar(lex);
    } while (lex->c == '\'');
  } else {
    while (IsNormalChar(lex->c)) {
      FbleVectorAppend(arena, wordv, lex->c);
      ReadNextChar(lex);
    }
  }
  FbleVectorAppend(arena, wordv, '\0');

  lvalp->word = wordv.xs;
  return WORD;
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

// FbleParse -- see documentation in fble-syntax.h
FbleExpr* FbleParse(FbleArena* arena, const char* filename, const char* include_path)
{
  FILE* fin = fopen(filename, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename);
    return NULL;
  }

  // Make a copy of the filename for locations so that the user doesn't have
  // to worry about keeping it alive for the duration of the program lifetime.
  char* source = FbleArrayAlloc(arena, char, strlen(filename) + 1);
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
