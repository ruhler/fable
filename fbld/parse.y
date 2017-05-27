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
    FbldMDecl* mdecl;
    FbldMDefn* mdefn;
    FbldValue* value;
  } ParseResult;

  #define YYLTYPE FbldLoc*

  #define YYLLOC_DEFAULT(Cur, Rhs, N)  \
  do                                    \
    if (N)                              \
      {                                 \
        (Cur) = YYRHSLOC(Rhs, 1);       \
      }                                 \
    else                                \
      {                                 \
        (Cur) = YYRHSLOC(Rhs, 0);       \
      }                                 \
  while (0)
%}

%union {
  FbldNameL* name;
  FbldQualifiedName* qname;
  FbldModule* module;
  FbldDecl* decl;
  FbldFuncDecl* func_decl;
  FbldDeclV* declv;
  FbldExpr* expr;
  FbldNameV* namev;
  FbldTypedNameV* tnamev;
  FbldExprV* exprv;
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
%token START_MDECL 1 "mdecl parse start indicator"
%token START_MDEFN 2 "mdefn parse start indicator"
%token START_VALUE 3 "value parse start indicator"

%token <name> NAME

// Keywords. These have type 'name' because in many contexts they are treated
// as normal names rather than keywords.
%token <name> MDECL "mdecl"
%token <name> MDEFN "mdefn"
%token <name> TYPE "type"
%token <name> STRUCT "struct"
%token <name> UNION "union"
%token <name> FUNC "func"
%token <name> PROC "proc"
%token <name> IMPORT "import"

%type <name> keyword name
%type <qname> qualified_name
%type <module> mdecl mdefn
%type <decl> decl defn import_decl struct_decl union_decl
%type <func_decl> func_decl
%type <declv> decl_list defn_list
%type <expr> expr stmt
%type <namev> name_list
%type <tnamev> field_list non_empty_field_list
%type <exprv> expr_list non_empty_expr_list
%type <value> value
%type <valuev> value_list non_empty_value_list

%%

// This grammar is used for parsing both module declarations and
// definitions.  We insert an arbitrary, artificial single-character token
// to indicate which we want to parse.
start:
     START_MDECL mdecl { result->mdecl = $2; }
   | START_MDEFN mdefn { result->mdefn = $2; }
   | START_VALUE value { result->value = $2; }
   ;
 
mdecl: "mdecl" name '(' name_list ')' '{' decl_list '}' ';' {
          $$ = arena->alloc(arena, sizeof(FbldMDecl));
          $$->name = $2;
          $$->deps = $4;
          $$->declv = $7;
        }
     ;

mdefn: "mdefn" name '(' name_list ')' '{' defn_list '}' ';' {
          $$ = arena->alloc(arena, sizeof(FbldMDefn));
          $$->name = $2;
          $$->deps = $4;
          $$->declv = $7;
        }
     ;

name: NAME | keyword ;

keyword: "mdecl" | "mdefn" | "type" | "struct" | "union" | "func" | "proc" | "import" ;

decl_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldDeclV));
      FblcVectorInit(arena, *$$);
    }
  | decl_list decl {
      FblcVectorAppend(arena, *$1, $2);
      $$ = $1;
    }
  ;

expr_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldExprV));
      FblcVectorInit(arena, *$$);
    }
  | non_empty_expr_list
  ;

non_empty_expr_list:
  expr {
      $$ = arena->alloc(arena, sizeof(FbldExprV));
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | non_empty_expr_list ',' expr {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

import_decl: "import" name '(' name_list ')' {
      FbldImportDecl* decl = arena->alloc(arena, sizeof(FbldImportDecl));
      decl->_base.tag = FBLD_IMPORT_DECL;
      decl->_base.name = $2;
      decl->namev = $4;
      $$ = &decl->_base;
    }
    ;

struct_decl: "struct" name '(' field_list ')' {
      FbldStructDecl* decl = arena->alloc(arena, sizeof(FbldStructDecl));
      decl->_base.tag = FBLD_STRUCT_DECL;
      decl->_base.name = $2;
      decl->fieldv = $4;
      $$ = &decl->_base;
    }
    ;

union_decl: "union" name '(' non_empty_field_list ')' {
      FbldUnionDecl* decl = arena->alloc(arena, sizeof(FbldUnionDecl));
      decl->_base.tag = FBLD_UNION_DECL;
      decl->_base.name = $2;
      decl->fieldv = $4;
      $$ = &decl->_base;
    }
    ;

func_decl: "func" name '(' field_list ';' qualified_name ')' {
      $$ = arena->alloc(arena, sizeof(FbldFuncDecl));
      $$->_base.tag = FBLD_FUNC_DECL;
      $$->_base.name = $2;
      $$->argv = $4;
      $$->return_type = $6;
      $$->body = NULL;
    }

decl:
    import_decl ';'
  | "type" name ';' {
      $$ = arena->alloc(arena, sizeof(FbldAbstractTypeDecl));
      $$->tag = FBLD_ABSTRACT_TYPE_DECL;
      $$->name = $2;
    }
  | struct_decl ';'
  | union_decl ';'
  | func_decl ';' {
      $$ = &$1->_base;
    }
  | "proc" name '(' name_list ';' field_list ';' qualified_name ')' ';' {
      assert(false && "TODO: proc");
    }
  ;

defn:
    import_decl ';'
  | struct_decl ';'
  | union_decl ';'
  | func_decl expr ';' {
      $1->body = $2;
      $$ = &$1->_base;
    }
  ;

stmt: expr ';' { $$ = $1; }
    | qualified_name name '=' expr ';' stmt {
        FbldLetExpr* let_expr = arena->alloc(arena, sizeof(FbldLetExpr));
        let_expr->_base.tag = FBLC_LET_EXPR;
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
      FbldVarExpr* var_expr = arena->alloc(arena, sizeof(FbldVarExpr));
      var_expr->_base.tag = FBLC_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      $$ = &var_expr->_base;
    }
  | qualified_name '(' expr_list ')' {
      FbldAppExpr* app_expr = arena->alloc(arena, sizeof(FbldAppExpr));
      app_expr->_base.tag = FBLC_APP_EXPR;
      app_expr->_base.loc = @$;
      app_expr->func = $1;
      app_expr->argv = $3;
      $$ = &app_expr->_base;
    }
  | qualified_name ':' name '(' expr ')' {
      FbldUnionExpr* union_expr = arena->alloc(arena, sizeof(FbldUnionExpr));
      union_expr->_base.tag = FBLC_UNION_EXPR;
      union_expr->_base.loc = @$;
      union_expr->type = $1;
      union_expr->field = $3;
      union_expr->arg = $5;
      $$ = &union_expr->_base;
    }
  | expr '.' name {
      FbldAccessExpr* access_expr = arena->alloc(arena, sizeof(FbldAccessExpr));
      access_expr->_base.tag = FBLC_ACCESS_EXPR;
      access_expr->_base.loc = @$;
      access_expr->obj = $1;
      access_expr->field = $3;
      $$ = &access_expr->_base;
    }
  | '?' '(' expr ';' non_empty_expr_list ')' {
      FbldCondExpr* cond_expr = arena->alloc(arena, sizeof(FbldCondExpr));
      cond_expr->_base.tag = FBLC_COND_EXPR;
      cond_expr->_base.loc = @$;
      cond_expr->select = $3;
      cond_expr->argv = $5;
      $$ = &cond_expr->_base;
  }
   ;
    
defn_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldDeclV));
      FblcVectorInit(arena, *$$);
    }
  | defn_list defn {
      FblcVectorAppend(arena, *$1, $2);
      $$ = $1;
    }
  ;

name_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldNameV));
      FblcVectorInit(arena, *$$);
    }
  | name {
      $$ = arena->alloc(arena, sizeof(FbldNameV));
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | name_list ',' name {
      FblcVectorAppend(arena, *$1, $3);
      $$ = $1;
    }
  ;

field_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldTypedNameV));
      FblcVectorInit(arena, *$$);
    }
  | non_empty_field_list ;

non_empty_field_list:
    qualified_name name {
      $$ = arena->alloc(arena, sizeof(FbldTypedNameV));
      FblcVectorInit(arena, *$$);
      FbldTypedName* tname = arena->alloc(arena, sizeof(FbldTypedName));
      tname->type = $1;
      tname->name = $2;
      FblcVectorAppend(arena, *$$, tname);
    }
  | non_empty_field_list ',' qualified_name name {
      FbldTypedName* tname = arena->alloc(arena, sizeof(FbldTypedName));
      tname->type = $3;
      tname->name = $4;
      FblcVectorAppend(arena, *$1, tname);
      $$ = $1;
    }
  ;

qualified_name:
    name {
      $$ = arena->alloc(arena, sizeof(FbldQualifiedName));
      $$->module = NULL;
      $$->name = $1;
    }
  | name '@' name {
      $$ = arena->alloc(arena, sizeof(FbldQualifiedName));
      $$->name = $1;
      $$->module = $3;
    }
  ;

value:
    qualified_name '(' value_list ')' {
      $$ = arena->alloc(arena, sizeof(FbldValue));
      $$->kind = FBLD_STRUCT_KIND;
      $$->type = $1;
      $$->tag = NULL;
      $$->fieldv = $3;
    }
  | qualified_name ':' name '(' value ')' {
      $$ = arena->alloc(arena, sizeof(FbldValue));
      $$->kind = FBLD_UNION_KIND;
      $$->type = $1;
      $$->tag = $3;
      $$->fieldv = arena->alloc(arena, sizeof(FbldValueV));
      FblcVectorInit(arena, *$$->fieldv);
      FblcVectorAppend(arena, *$$->fieldv, $5);
    }
  ;

value_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldValueV));
      FblcVectorInit(arena, *$$);
    }
  | non_empty_value_list
  ;

non_empty_value_list:
    value {
      $$ = arena->alloc(arena, sizeof(FbldValueV));
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
  return strchr("(){};,@:?=.", c) != NULL
    || c == START_MDECL
    || c == START_MDEFN
    || c == START_VALUE;
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

  FbldLoc* loc = arena->alloc(arena, sizeof(FbldLoc));
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
  lvalp->name = arena->alloc(arena, sizeof(FbldNameL));
  lvalp->name->name = namev.xs;
  lvalp->name->loc = loc;

  struct { char* keyword; int symbol; } keywords[] = {
    {.keyword = "mdecl", .symbol = MDECL},
    {.keyword = "mdefn", .symbol = MDEFN},
    {.keyword = "type", .symbol = TYPE},
    {.keyword = "struct", .symbol = STRUCT},
    {.keyword = "union", .symbol = UNION},
    {.keyword = "func", .symbol = FUNC},
    {.keyword = "proc", .symbol = PROC},
    {.keyword = "import", .symbol = IMPORT}
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
  FbldLoc* loc = *llocp;
  fprintf(stderr, "%s:%d:%d: error: %s\n", loc->source, loc->line, loc->col, msg);
}

// FbldParseMDecl -- see documentation in fbld.h
FbldMDecl* FbldParseMDecl(FblcArena* arena, const char* filename)
{
  FILE* fin = fopen(filename, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename);
    return NULL;
  }

  Lex lex = {
    .c = START_MDECL,
    .loc = { .source = filename, .line = 1, .col = 0 },
    .fin = fin,
    .sin = NULL
  };
  ParseResult result;
  result.mdecl = NULL;
  yyparse(arena, &lex, &result);
  return result.mdecl;
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
