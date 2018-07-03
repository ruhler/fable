// parse.y --
//   This file describes the bison grammar for parsing fble expressions.
%{
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
  FbleExpr* expr;
  FbleExprV exprs;
  FbleFieldV fields;
  FbleBindingV bindings;
}

%{
  static bool IsNameChar(int c);
  static bool IsSingleChar(int c);
  static void ReadNextChar(Lex* lex);
  static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FbleArena* arena, Lex* lex);
  static void yyerror(YYLTYPE* llocp, FbleArena* arena, Lex* lex, FbleExpr** result, const char* msg);
%}

%locations
%define api.pure
%define parse.error verbose
%param {FbleArena* arena} {Lex* lex}
%parse-param {FbleExpr** result}

%token END 0 "end of file"
%token INVALID "invalid character"

%token <name> NAME

%type <expr> expr stmt
%type <exprs> exprs
%type <fields> fieldp fields
%type <bindings> let_bindings

%%

start: stmt ;

expr:
   '{' stmt '}' {
      $$ = $2;
   }
 | NAME {
      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      $$ = &var_expr->_base;
   }
 | '@' {
      FbleTypeTypeExpr* type_type_expr = FbleAlloc(arena, FbleTypeTypeExpr);
      type_type_expr->_base.tag = FBLE_TYPE_TYPE_EXPR;
      type_type_expr->_base.loc = @$;
      $$ = &type_type_expr->_base;
   }
 | '\\' '(' fields ';' expr ')' {
      FbleFuncTypeExpr* func_type_expr = FbleAlloc(arena, FbleFuncTypeExpr);
      func_type_expr->_base.tag = FBLE_FUNC_TYPE_EXPR;
      func_type_expr->_base.loc = @$;
      func_type_expr->args = $3;
      func_type_expr->rtype = $5;
      $$ = &func_type_expr->_base;
   }
 | '\\' '(' fields ')' expr {
      FbleFuncValueExpr* func_value_expr = FbleAlloc(arena, FbleFuncValueExpr);
      func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
      func_value_expr->_base.loc = @$;
      func_value_expr->args = $3;
      func_value_expr->body = $5;
      $$ = &func_value_expr->_base;
   }
 | expr '(' exprs ')' {
      FbleApplyExpr* apply_expr = FbleAlloc(arena, FbleApplyExpr);
      apply_expr->_base.tag = FBLE_APPLY_EXPR;
      apply_expr->_base.loc = @$;
      apply_expr->func = $1;
      apply_expr->args = $3;
      $$ = &apply_expr->_base;
   }
 | '*' '(' fields ')' {
      FbleStructTypeExpr* struct_type_expr = FbleAlloc(arena, FbleStructTypeExpr);
      struct_type_expr->_base.tag = FBLE_STRUCT_TYPE_EXPR;
      struct_type_expr->_base.loc = @$;
      struct_type_expr->fields = $3;
      $$ = &struct_type_expr->_base;
   }
 | '+' '(' fieldp ')' {
      FbleUnionTypeExpr* union_type_expr = FbleAlloc(arena, FbleUnionTypeExpr);
      union_type_expr->_base.tag = FBLE_UNION_TYPE_EXPR;
      union_type_expr->_base.loc = @$;
      union_type_expr->fields = $3;
      $$ = &union_type_expr->_base;
   }
 | expr ':' NAME '(' expr ')' {
      FbleUnionValueExpr* union_value_expr = FbleAlloc(arena, FbleUnionValueExpr);
      union_value_expr->_base.tag = FBLE_UNION_VALUE_EXPR;
      union_value_expr->_base.loc = @$;
      union_value_expr->type = $1;
      union_value_expr->field = $3;
      union_value_expr->arg = $5;
      $$ = &union_value_expr->_base;
   }
 ;

stmt:
    expr ';' { $$ = $1; }
  | let_bindings ';' stmt {
      FbleLetExpr* let_expr = FbleAlloc(arena, FbleLetExpr);
      let_expr->_base.tag = FBLE_LET_EXPR;
      let_expr->_base.loc = @$;
      let_expr->bindings = $1;
      let_expr->body = $3;
      $$ = &let_expr->_base;
    }  
  ;

exprs:
   %empty { FbleVectorInit(arena, $$); }
 | expr {
     FbleVectorInit(arena, $$);
     FbleVectorAppend(arena, $$, $1);
   }
 | exprs ',' expr {
     $$ = $1;
     FbleVectorAppend(arena, $$, $3);
   }
 ;

fields:
   %empty { FbleVectorInit(arena, $$); }
 | fieldp { $$ = $1; }
 ;

fieldp:
   expr NAME {
     FbleVectorInit(arena, $$);
     FbleField* field = FbleVectorExtend(arena, $$);
     field->type = $1;
     field->name = $2;
   }
 | fieldp ',' expr NAME {
     $$ = $1;
     FbleField* field = FbleVectorExtend(arena, $$);
     field->type = $3;
     field->name = $4;
   }
 ;

let_bindings: 
  expr NAME '=' expr {
      FbleVectorInit(arena, $$);
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->type = $1;
      binding->name = $2;
      binding->expr = $4;
    }
  | let_bindings ',' expr NAME '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleVectorExtend(arena, $$);
      binding->type = $3;
      binding->name = $4;
      binding->expr = $6;
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
  return strchr("(){};,:?=.<+*-!$@\\", c) != NULL;
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
static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FbleArena* arena, Lex* lex)
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
static void yyerror(YYLTYPE* llocp, FbleArena* arena, Lex* lex, FbleExpr** result, const char* msg)
{
  FbleReportError("%s\n", llocp, msg);
}

// FbleParse -- see documentation in fble.h
FbleExpr* FbleParse(FbleArena* arena, const char* filename)
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
  yyparse(arena, &lex, &result);
  return result;
}
