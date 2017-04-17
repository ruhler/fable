// mdecl.y --
//   This file describes the bison grammar for parsing fbld module
//   declarations.
%{
  #include <assert.h>       // for assert
  #include <ctype.h>        // for isalnum, isspace
  #include <stdio.h>        // for FILE*, fgetc
  #include <string.h>       // for strcmp

  #include "fbld.h"

  static bool IsNameChar(int c);
  static int GetNextChar(FILE* fin, FbldLoc* loc);
  static int yylex(FblcArena* arena, FILE* fin, FbldLoc* loc);
  static void yyerror(FblcArena* arena, FILE* fin, FbldLoc* loc, FbldMDecl** mdecl_out, const char* msg);
%}

%union {
  FbldNameL* name;
  FbldQualifiedName* qname;
  FbldMDecl* mdecl;
  FbldDeclItem* item;
  FbldDeclItemV* itemv;
  FbldNameV* namev;
  FbldTypedNameV* tnamev;
}

%define parse.error verbose
%param {FblcArena* arena} {FILE* fin} {FbldLoc* loc}
%parse-param {FbldMDecl** mdecl_out}

%token END 0 "end of file"
%token <name> NAME

// Keywords. These have type 'name' because in many contexts they are treated
// as normal names rather than keywords.
%token <name> MDECL "mdecl"
%token <name> TYPE "type"
%token <name> STRUCT "struct"
%token <name> UNION "union"
%token <name> FUNC "func"
%token <name> PROC "proc"
%token <name> IMPORT "import"

%type <name> keyword name
%type <qname> qualified_name
%type <mdecl> mdecl
%type <item> item
%type <itemv> item_list
%type <namev> name_list
%type <tnamev> field_list non_empty_field_list

%%
 
mdecl: "mdecl" name '(' name_list ')' '{' item_list '}' ';' {
          $$ = arena->alloc(arena, sizeof(FbldMDecl));
          $$->name = $2;
          $$->deps = $4;
          $$->items = $7;
          *mdecl_out = $$;
        }
     ;

name: NAME | keyword ;

keyword: "mdecl" | "type" | "struct" | "union" | "func" | "proc" | "import" ;

item_list:
    %empty {
      $$ = arena->alloc(arena, sizeof(FbldDeclItemV));
      FblcVectorInit(arena, *$$);
    }
  | item_list item {
      FblcVectorAppend(arena, *$1, $2);
      $$ = $1;
    }
  ;

item:
    "import" name '(' name_list ')' ';' {
      assert(false && "TODO: import");
    }
  | "type" name ';' {
      assert(false && "TODO: type");
    }
  | "struct" name '(' field_list ')' ';' {
      FbldStructDeclItem* struct_decl = arena->alloc(arena, sizeof(FbldStructDeclItem));
      struct_decl->_base.tag = FBLD_STRUCT_DECL_ITEM;
      struct_decl->_base.name = $2;
      struct_decl->fieldv = $4;
      $$ = &struct_decl->_base;
    }
  | "union" name '(' non_empty_field_list ')' ';' {
      assert(false && "TODO: union");
    }
  | "func" name '(' field_list ';' qualified_name ')' ';' {
      FbldFuncDeclItem* func_decl = arena->alloc(arena, sizeof(FbldFuncDeclItem));
      func_decl->_base.tag = FBLD_FUNC_DECL_ITEM;
      func_decl->_base.name = $2;
      func_decl->argv = $4;
      func_decl->return_type = $6;
      $$ = &func_decl->_base;
    }
  | "proc" name '(' name_list ';' field_list ';' qualified_name ')' ';' {
      assert(false && "TODO: proc");
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

// GetNextChar --
//   Return the next character from the given file, while keeping track of
//   location information.
//
// Inputs:
//   fin - The file to read from.
//   loc - A pointer to the location to update.
//
// Results:
//   The next character read from the file.
//
// Side effects:
//   Updates loc based on the character and advances the file pointer by one
//   character.
static int GetNextChar(FILE* fin, FbldLoc* loc)
{
  int c = fgetc(fin);
  if (c == '\n') {
    loc->line++;
    loc->col = 1;
  } else if (c != EOF) {
    loc->col++;
  }
  return c;
}

// yylex -- 
//   Return the next token in the given input stream.
//   This is the lexer for the bison generated parser.
//
// Inputs:
//   arena - Arena used for allocating names.
//   fin - The stream to parse the next token from.
//   loc - A pointer to the current parse location.
// 
// Results:
//   The id of the next terminal symbol in the input stream.
//
// Side effects:
//   Sets yylval with the semantic value of the next token in the input
//   stream, advances the stream to the subsequent token and updates loc
//   accordingly.
static int yylex(FblcArena* arena, FILE* fin, FbldLoc* loc)
{
  int c = GetNextChar(fin, loc);

  // Skip past white space and comments.
  bool is_comment_start = (c == '#');
  while (isspace(c) || is_comment_start) {
    c = GetNextChar(fin, loc);
    if (is_comment_start) {
      while (c != EOF && c != '\n') {
        c = GetNextChar(fin, loc);
      }
    }
    is_comment_start = (c == '#');
  }

  if (c == EOF) {
    return END;
  }

  if (!IsNameChar(c)) {
    return c;
  };

  yylval.name = arena->alloc(arena, sizeof(FbldNameL));
  yylval.name->loc = arena->alloc(arena, sizeof(FbldLoc));
  yylval.name->loc->source = loc->source;
  yylval.name->loc->line = loc->line;
  yylval.name->loc->col = loc->col-1;

  struct { size_t size; char* xs; } namev;
  FblcVectorInit(arena, namev);
  while (IsNameChar(c)) {
    FblcVectorAppend(arena, namev, c);
    c = GetNextChar(fin, loc);
  }
  ungetc(c == '\n' ? ' ' : c, fin);
  loc->col--;
  FblcVectorAppend(arena, namev, '\0');
  yylval.name->name = namev.xs;

  struct { char* keyword; int symbol; } keywords[] = {
    {.keyword = "mdecl", .symbol = MDECL},
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
//   mdecl_out - unused.
//   msg - The error message.
//
// Results:
//   None.
//
// Side effects:
//   An error message is printed to stderr.
static void yyerror(FblcArena* arena, FILE* fin, FbldLoc* loc, FbldMDecl** mdecl_out, const char* msg)
{
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
  FbldLoc loc = { .source = filename, .line = 1, .col = 1 };
  FbldMDecl* mdecl = NULL;
  yyparse(arena, fin, &loc, &mdecl);
  return mdecl;
}

