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

  static bool IsNameChar(int c);
  static bool IsSingleChar(int c);
  static void ReadNextChar(Lex* lex);
  static int yylex(FblcArena* arena, Lex* lex);
  static void yyerror(FblcArena* arena, Lex* lex, ParseResult* result, const char* msg);
%}

%union {
  FbldNameL* name;
  FbldQualifiedName* qname;
  FbldImportDecl* import_decl;
  FbldStructDecl* struct_decl;
  FbldUnionDecl* union_decl;
  FbldFuncDecl* func_decl;
  FbldModule* module;
  FbldDecl* decl;
  FbldDeclV* declv;
  FbldExpr* expr;
  FbldNameV* namev;
  FbldTypedNameV* tnamev;
  FbldExprV* exprv;
  FbldValue* value;
  FbldValueV* valuev;
}

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
%type <decl> decl defn
%type <import_decl> import_decl
%type <struct_decl> struct_decl
%type <union_decl> union_decl
%type <func_decl> func_decl
%type <declv> decl_list defn_list
%type <expr> expr stmt
%type <namev> name_list
%type <tnamev> field_list non_empty_field_list
%type <exprv> expr_list non_empty_expr_list
%type <value> value
%type <valuev> value_list

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
      $$ = arena->alloc(arena, sizeof(FbldImportDecl));
      $$->_base.tag = FBLD_IMPORT_DECL;
      $$->_base.name = $2;
      $$->namev = $4;
    }
    ;

struct_decl: "struct" name '(' field_list ')' {
      $$ = arena->alloc(arena, sizeof(FbldStructDecl));
      $$->_base.tag = FBLD_STRUCT_DECL;
      $$->_base.name = $2;
      $$->fieldv = $4;
    }
    ;

union_decl: "union" name '(' non_empty_field_list ')' {
      $$ = arena->alloc(arena, sizeof(FbldUnionDecl));
      $$->_base.tag = FBLD_UNION_DECL;
      $$->_base.name = $2;
      $$->fieldv = $4;
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
    import_decl ';' {
      $$ = &$1->_base;
    }
  | "type" name ';' {
      assert(false && "TODO: type");
    }
  | struct_decl ';' {
      $$ = &$1->_base;
    }
  | union_decl ';' {
      $$ = &$1->_base;
    }
  | func_decl ';' {
      $$ = &$1->_base;
    }
  | "proc" name '(' name_list ';' field_list ';' qualified_name ')' ';' {
      assert(false && "TODO: proc");
    }
  ;

defn:
    import_decl ';' {
      $$ = &$1->_base;
    }
  | struct_decl ';' {
      $$ = &$1->_base;
    }
  | union_decl ';' {
      $$ = &$1->_base;
    }
  | func_decl expr ';' {
      $1->body = $2;
      $$ = &$1->_base;
    }
  ;

stmt: expr ';' { $$ = $1; } ;

expr:
    '{' stmt '}' {
       $$ = $2;
    }
  | qualified_name '(' expr_list ')' {
      FbldAppExpr* app_expr = arena->alloc(arena, sizeof(FbldAppExpr));
      app_expr->_base.tag = FBLC_APP_EXPR;
      // TODO: Use the location at the beginning of the expression, not the
      // one at the end.
      app_expr->_base.loc = arena->alloc(arena, sizeof(FbldLoc));
      app_expr->_base.loc->source = lex->loc.source;
      app_expr->_base.loc->line = lex->loc.line;
      app_expr->_base.loc->col = lex->loc.col;
      app_expr->func = $1;
      app_expr->argv = $3;
      $$ = &app_expr->_base;
    }
  | qualified_name ':' name '(' expr ')' {
      FbldUnionExpr* union_expr = arena->alloc(arena, sizeof(FbldUnionExpr));
      union_expr->_base.tag = FBLC_UNION_EXPR;
      // TODO: Use the location at the beginning of the expression, not the
      // one at the end.
      union_expr->_base.loc = arena->alloc(arena, sizeof(FbldLoc));
      union_expr->_base.loc->source = lex->loc.source;
      union_expr->_base.loc->line = lex->loc.line;
      union_expr->_base.loc->col = lex->loc.col;
      union_expr->type = $1;
      union_expr->field = $3;
      union_expr->arg = $5;
      $$ = &union_expr->_base;
    }
  | '?' '(' expr ';' non_empty_expr_list ')' {
      FbldCondExpr* cond_expr = arena->alloc(arena, sizeof(FbldCondExpr));
      cond_expr->_base.tag = FBLC_COND_EXPR;
      // TODO: Use the location at the beginning of the expression, not the
      // one at the end.
      cond_expr->_base.loc = arena->alloc(arena, sizeof(FbldLoc));
      cond_expr->_base.loc->source = lex->loc.source;
      cond_expr->_base.loc->line = lex->loc.line;
      cond_expr->_base.loc->col = lex->loc.col;
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
      $$->module = $1;
      $$->name = $3;
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
  | value_list ',' value {
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
  return strchr("(){};,@:?", c) != NULL
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
  if (lex->fin) {
    lex->c = fgetc(lex->fin);
  } else if (lex->sin) {
    if (*lex->sin == '\0') {
      lex->c = EOF;
    } else {
      lex->c = *lex->sin++;
    }
  }

  if (lex->c == '\n') {
    lex->loc.line++;
    lex->loc.col = 1;
  } else if (lex->c != EOF) {
    lex->loc.col++;
  }
}

// yylex -- 
//   Return the next token in the input stream for the given lex context.
//   This is the lexer for the bison generated parser.
//
// Inputs:
//   arena - Arena used for allocating names.
//   lex - The lex context to parse the next token from.
// 
// Results:
//   The id of the next terminal symbol in the input stream.
//
// Side effects:
//   Sets yylval with the semantic value of the next token in the input
//   stream, advances the stream to the subsequent token and updates the lex
//   loc accordingly.
static int yylex(FblcArena* arena, Lex* lex)
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

  yylval.name = arena->alloc(arena, sizeof(FbldNameL));
  yylval.name->loc = arena->alloc(arena, sizeof(FbldLoc));
  yylval.name->loc->source = lex->loc.source;
  yylval.name->loc->line = lex->loc.line;
  yylval.name->loc->col = lex->loc.col;

  struct { size_t size; char* xs; } namev;
  FblcVectorInit(arena, namev);
  while (IsNameChar(lex->c)) {
    FblcVectorAppend(arena, namev, lex->c);
    ReadNextChar(lex);
  }
  FblcVectorAppend(arena, namev, '\0');
  yylval.name->name = namev.xs;

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
//   arena - unused.
//   fin - unused.
//   loc - The location of the next item in the input stream.
//   result - unused.
//   msg - The error message.
//
// Results:
//   None.
//
// Side effects:
//   An error message is printed to stderr.
static void yyerror(FblcArena* arena, Lex* lex, ParseResult* result, const char* msg)
{
  fprintf(stderr, "%s:%d:%d: error: %s\n", lex->loc.source, lex->loc.line, lex->loc.col, msg);
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
