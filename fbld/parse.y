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
    FbldQName* qname;
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
  FbldQName* qname;
  FbldIRef* iref;
  FbldMRef* mref;
  FbldMType* mtype;
  FbldMDefn* mdefn;
  FbldDecl* decl;
  FbldFuncDecl* func_decl;
  FbldDeclV* declv;
  FbldExpr* expr;
  FbldNameV* namev;
  FbldArgV* argv;
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
%token START_MTYPE 1 "mtype parse start indicator"
%token START_MDEFN 2 "mdefn parse start indicator"
%token START_VALUE 3 "value parse start indicator"
%token START_QNAME 4 "qname parse start indicator"

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
%type <qname> qname
%type <iref> iref
%type <mref> mref
%type <mtype> mtype
%type <mdefn> mdefn
%type <decl> decl defn using_decl struct_decl union_decl
%type <func_decl> func_decl
%type <declv> decl_list defn_list
%type <expr> expr stmt
%type <namev> name_list
%type <qnamev> qname_list
%type <mrefv> mref_list
%type <argv> arg_list non_empty_arg_list
%type <exprv> expr_list non_empty_expr_list
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
   | START_QNAME qname { result->qname = $2; }
   ;
 
mtype: "mtype" name '<' name_list '>' '{' decl_list '}' ';' {
          $$ = FBLC_ALLOC(arena, FbldMType);
          $$->name = $2;
          $$->targs = $4;
          $$->declv = $7;
        }
     ;

mdefn: "mdefn" name '<' name_list ';' marg_list ';' iref '>' '{' defn_list '}' ';' {
          $$ = FBLC_ALLOC(arena, FbldMDefn);
          $$->name = $2;
          $$->targs = $4;
          $$->margs = $6;
          $$->iref = $8;
          $$->declv = $11;
        }
     ;

name: NAME | keyword ;

keyword: "mtype" | "mdefn" | "type" | "struct" | "union" | "func" | "proc" | "using" ;

decl_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *$$);
    }
  | decl_list decl {
      FblcVectorAppend(arena, *$1, $2);
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

using_decl: "using" mref '{' using_item_list '}' {
      FbldUsingDecl* decl = FBLC_ALLOC(arena, FbldUsingDecl);
      decl->_base.tag = FBLD_USING_DECL;
      decl->_base.name = NULL;  // TODO: What to put here?
      decl->mref = $2;
      decl->itemv = $4;
      $$ = &decl->_base;
    }
    ;

struct_decl: "struct" name '(' arg_list ')' {
      FbldStructDecl* decl = FBLC_ALLOC(arena, FbldStructDecl);
      decl->_base.tag = FBLD_STRUCT_DECL;
      decl->_base.name = $2;
      decl->fieldv = $4;
      $$ = &decl->_base;
    }
    ;

union_decl: "union" name '(' non_empty_arg_list ')' {
      FbldUnionDecl* decl = FBLC_ALLOC(arena, FbldUnionDecl);
      decl->_base.tag = FBLD_UNION_DECL;
      decl->_base.name = $2;
      decl->fieldv = $4;
      $$ = &decl->_base;
    }
    ;

func_decl: "func" name '(' arg_list ';' qname ')' {
      $$ = FBLC_ALLOC(arena, FbldFuncDecl);
      $$->_base.tag = FBLD_FUNC_DECL;
      $$->_base.name = $2;
      $$->argv = $4;
      $$->return_type = $6;
      $$->body = NULL;
    }

decl:
    using_decl ';'
  | "type" name ';' {
      $$ = FBLC_ALLOC(arena, FbldAbstractTypeDecl);
      $$->tag = FBLD_ABSTRACT_TYPE_DECL;
      $$->name = $2;
    }
  | struct_decl ';'
  | union_decl ';'
  | func_decl ';' {
      $$ = &$1->_base;
    }
  | "proc" name '(' name_list ';' arg_list ';' qname ')' ';' {
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
    | qname name '=' expr ';' stmt {
        FbldLetExpr* let_expr = FBLC_ALLOC(arena, FbldLetExpr);
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
      FbldVarExpr* var_expr = FBLC_ALLOC(arena, FbldVarExpr);
      var_expr->_base.tag = FBLC_VAR_EXPR;
      var_expr->_base.loc = @$;
      var_expr->var = $1;
      var_expr->id = FBLC_NULL_ID;
      $$ = &var_expr->_base;
    }
  | qname '(' expr_list ')' {
      FbldAppExpr* app_expr = FBLC_ALLOC(arena, FbldAppExpr);
      app_expr->_base.tag = FBLC_APP_EXPR;
      app_expr->_base.loc = @$;
      app_expr->func = $1;
      app_expr->argv = $3;
      $$ = &app_expr->_base;
    }
  | qname ':' name '(' expr ')' {
      FbldUnionExpr* union_expr = FBLC_ALLOC(arena, FbldUnionExpr);
      union_expr->_base.tag = FBLC_UNION_EXPR;
      union_expr->_base.loc = @$;
      union_expr->type = $1;
      union_expr->field.name = $3;
      union_expr->field.id = FBLC_NULL_ID;
      union_expr->arg = $5;
      $$ = &union_expr->_base;
    }
  | expr '.' name {
      FbldAccessExpr* access_expr = FBLC_ALLOC(arena, FbldAccessExpr);
      access_expr->_base.tag = FBLC_ACCESS_EXPR;
      access_expr->_base.loc = @$;
      access_expr->obj = $1;
      access_expr->field.name = $3;
      access_expr->field.id = FBLC_NULL_ID;
      $$ = &access_expr->_base;
    }
  | '?' '(' expr ';' non_empty_expr_list ')' {
      FbldCondExpr* cond_expr = FBLC_ALLOC(arena, FbldCondExpr);
      cond_expr->_base.tag = FBLC_COND_EXPR;
      cond_expr->_base.loc = @$;
      cond_expr->select = $3;
      cond_expr->argv = $5;
      $$ = &cond_expr->_base;
  }
   ;
    
defn_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldDeclV);
      FblcVectorInit(arena, *$$);
    }
  | defn_list defn {
      FblcVectorAppend(arena, *$1, $2);
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

qname_list:
    %empty {
      $$ = FBLC_ALLOC(arena, FbldQNameV);
      FblcVectorInit(arena, *$$);
    }
  | qname {
      $$ = FBLC_ALLOC(arena, FbldQNameV);
      FblcVectorInit(arena, *$$);
      FblcVectorAppend(arena, *$$, $1);
    }
  | qname_list ',' qname {
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
      $$ = FBLC_ALLOC(arena, FbldTypedNameV);
      FblcVectorInit(arena, *$$);
    }
  | non_empty_arg_list ;

non_empty_arg_list:
    qname name {
      $$ = FBLC_ALLOC(arena, FbldTypedNameV);
      FblcVectorInit(arena, *$$);
      FbldTypedName* tname = FBLC_ALLOC(arena, FbldTypedName);
      tname->type = $1;
      tname->name = $2;
      FblcVectorAppend(arena, *$$, tname);
    }
  | non_empty_arg_list ',' qname name {
      FbldTypedName* tname = FBLC_ALLOC(arena, FbldTypedName);
      tname->type = $3;
      tname->name = $4;
      FblcVectorAppend(arena, *$1, tname);
      $$ = $1;
    }
  ;

qname:
    name {
      $$ = FBLC_ALLOC(arena, FbldQName);
      $$->name = $1;
      $$->mref = NULL;
    }
  | name '@' mref {
      $$ = FBLC_ALLOC(arena, FbldQName);
      $$->name = $1;
      $$->mref = $3;
    }
  ;

iref: name '<' qname_list '>' {
      $$ = FBLC_ALLOC(arena, FbldIRef);
      $$->name = $1;
      $$->targs = $3;
    }
  ;

mref: name '<' qname_list ';' mref_list '>' {
      $$ = FBLC_ALLOC(arena, FbldMRef);
      $$->name = $1;
      $$->targs = $3;
      $$->margs = $5;
    }
  ;

value:
    qname '(' value_list ')' {
      $$ = FBLC_ALLOC(arena, FbldValue);
      $$->kind = FBLD_STRUCT_KIND;
      $$->type = $1;
      $$->tag = NULL;
      $$->fieldv = $3;
    }
  | qname ':' name '(' value ')' {
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
  lvalp->name = FBLC_ALLOC(arena, FbldNameL);
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
  FbldReportError("%s\n", *llocp, msg);
}

// FbldParseMType -- see documentation in fbld.h
FbldMDecl* FbldParseMType(FblcArena* arena, const char* filename)
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

// FbldParseQNameFromString -- see documentation in fbld.h
FbldQName* FbldParseValueFromString(FblcArena* arena, const char* string)
{
  Lex lex = {
    .c = START_QNAME,
    .loc = { .source = string, .line = 1, .col = 0 },
    .fin = NULL,
    .sin = string
  };
  ParseResult result;
  result.qname = NULL;
  yyparse(arena, &lex, &result);
  return result.qname;
}
