/**
 * @file parse.y --
 *  The bison grammar for parsing fble expressions.
 */
%{

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdbool.h>  // for bool
#include <stdio.h>    // for FILE, fprintf, stderr
#include <string.h>   // for strchr, strcpy

#include <fble/fble-alloc.h>
#include <fble/fble-load.h>
#include <fble/fble-loc.h>
#include <fble/fble-module-path.h>
#include <fble/fble-name.h>
#include <fble/fble-string.h>
#include <fble/fble-vector.h>
#include "expr.h"

/**
 * @struct[Lex] State for the lexer.
 *  @field[int][c]
 *   The next character in the input stream.
 *   EOF is used to indicate the end of the input stream.
 *  @field[FbleLoc][loc] The location corresponding to the character c.
 *  @field[FILE*][fin]
 *   The input stream if reading from a file. NULL otherwise.
 *  @field[const char*][sin]
 *   The input stream if reading from a string. NULL otherwise.
 *  @field[int][start_token]
 *   Token to emit at start of lexing if non-zero. 
 *   Used to switch between different kinds of entities to parse.
 */
typedef struct {
  int c;
  FbleLoc loc;
  FILE* fin;
  const char* sin;
  int start_token;
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
  FbleModulePath* module_path;
  FbleKind* kind;
  FbleKindV kinds;
  FbleTaggedKindV tagged_kinds;
  FbleExpr* expr;
  FbleExprV exprs;
  FbleTaggedTypeExpr tagged_type;
  FbleTaggedTypeExprV tagged_types;
  FbleTaggedExpr tagged_expr;
  FbleTaggedExprV tagged_exprs;
  FbleBindingV bindings;
}

%{
  static FbleString* ToString(const char* str);
  static bool IsSpaceChar(int c);
  static bool IsPunctuationChar(int c);
  static bool IsWordChar(int c);
  static void ReadNextChar(Lex* lex);
  static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, Lex* lex);
  static void yyerror(YYLTYPE* llocp, Lex* lex, FbleExpr** result, FbleModulePathV* deps, const char* msg);
%}

%locations
%define api.pure
%define parse.error verbose
%param {Lex* lex}
%parse-param {FbleExpr** result} {FbleModulePathV* deps}

%token END 0 "end of file"
%token PARSE_PROGRAM
%token PARSE_MODULE_PATH
%token <word> WORD

%type <name> name
%type <module_path> path module_path
%type <kind> tkind nkind kind
%type <kinds> tkind_p
%type <tagged_kinds> tagged_kind_p
%type <expr> expr block stmt
%type <exprs> expr_p expr_s
%type <tagged_type> tagged_type
%type <tagged_types> tagged_type_p tagged_type_s
%type <tagged_expr> implicit_tagged_expr tagged_expr
%type <tagged_exprs> implicit_tagged_expr_p implicit_tagged_expr_s tagged_expr_p
%type <bindings> let_binding_p

%destructor {
  FbleFree((char*)$$);
} <word>

%destructor {
  FbleFreeName($$);
} <name>

%destructor {
  FbleFreeModulePath($$);
} <module_path>

%destructor {
  FbleFreeKind($$);
} <kind>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeKind($$.xs[i]);
  }
  FbleFreeVector($$);
} <kinds>

%destructor { 
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeKind($$.xs[i].kind);
    FbleFreeName($$.xs[i].name);
  }
  FbleFreeVector($$);
} <tagged_kinds>

%destructor {
  FbleFreeExpr($$);
} <expr>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeExpr($$.xs[i]);
  }
  FbleFreeVector($$);
} <exprs>

%destructor { 
  // Note: I believe this code is unreachable because tagged_type only ever
  // occurs as the last item in the definition of a non-terminal. Any errors
  // following parsing of this will result in calling the desctructor for the
  // parent non-terminal, not the tagged_type.
  FbleFreeExpr($$.type);
  FbleFreeName($$.name);
} <tagged_type>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeExpr($$.xs[i].type);
    FbleFreeName($$.xs[i].name);
  }
  FbleFreeVector($$);
} <tagged_types>

%destructor {
  // Note: I believe this code is unreachable because tagged_expr only ever
  // occurs as the last item in the definition of a non-terminal. Any errors
  // following parsing of this will result in calling the desctructor for the
  // parent non-terminal, not the tagged_expr.
  FbleFreeName($$.name);
  FbleFreeExpr($$.expr);
} <tagged_expr>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeName($$.xs[i].name);
    FbleFreeExpr($$.xs[i].expr);
  }
  FbleFreeVector($$);
} <tagged_exprs>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeKind($$.xs[i].kind);
    FbleFreeExpr($$.xs[i].type);
    FbleFreeName($$.xs[i].name);
    FbleFreeExpr($$.xs[i].expr);
  }
  FbleFreeVector($$);
} <bindings>

%%

start:
   PARSE_PROGRAM stmt {
     // Parsed program is stored in result.
     *result = $2;
   }
 | PARSE_MODULE_PATH module_path {
     // Parsed module path is stored in deps.
     FbleAppendToVector(*deps, $2);
   }
 ;

name:
   WORD {
     $$.name = ToString($1);
     $$.space = FBLE_NORMAL_NAME_SPACE;
     $$.loc = FbleCopyLoc(@$);
   }
 | WORD '@' {
     $$.name = ToString($1);
     $$.space = FBLE_TYPE_NAME_SPACE;
     $$.loc = FbleCopyLoc(@$);
   }
 ;

path:
   WORD {
     $$ = FbleNewModulePath(@$);
     FbleName* name = FbleExtendVector($$->path);
     name->name = ToString($1);
     name->space = FBLE_NORMAL_NAME_SPACE;
     name->loc = FbleCopyLoc(@$);
   }
 | path '/' WORD {
     $$ = $1;
     FbleName* name = FbleExtendVector($$->path);
     name->name = ToString($3);
     name->space = FBLE_NORMAL_NAME_SPACE;
     name->loc = FbleCopyLoc(@$);
   };

module_path:
   '/' path '%' {
      $$ = $2;
      FbleFreeLoc($$->loc);
      $$->loc = FbleCopyLoc(@$);
   }
 ;

nkind:
   '%' {
      $$ = FbleNewBasicKind(@$, 0);
   }
 | '<' tkind_p '>' nkind {
      FbleKind* kind = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleKind* arg = $2.xs[$2.size - 1 - i];
        FblePolyKind* poly_kind = FbleAlloc(FblePolyKind);
        poly_kind->_base.tag = FBLE_POLY_KIND;
        poly_kind->_base.loc = FbleCopyLoc(@$);
        poly_kind->_base.refcount = 1;
        poly_kind->arg = arg;
        poly_kind->rkind = kind;
        kind = &poly_kind->_base;
      }
      FbleFreeVector($2);
      $$ = kind;
   }
 ;

tkind:
   '@' {
      $$ = FbleNewBasicKind(@$, 1);
   }
 | '<' tkind_p '>' tkind {
      FbleKind* kind = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleKind* arg = $2.xs[$2.size - 1 - i];
        FblePolyKind* poly_kind = FbleAlloc(FblePolyKind);
        poly_kind->_base.tag = FBLE_POLY_KIND;
        poly_kind->_base.loc = FbleCopyLoc(@$);
        poly_kind->_base.refcount = 1;
        poly_kind->arg = arg;
        poly_kind->rkind = kind;
        kind = &poly_kind->_base;
      }
      FbleFreeVector($2);
      $$ = kind;
   }
 ;

kind: nkind | tkind ;

tkind_p:
   tkind {
     FbleInitVector($$);
     FbleAppendToVector($$, $1);
   }
 | tkind_p ',' tkind {
     $$ = $1;
     FbleAppendToVector($$, $3);
   }
 ;

tagged_kind_p:
   kind name {
     FbleInitVector($$);
     FbleTaggedKind* type_field = FbleExtendVector($$);
     type_field->kind = $1;
     type_field->name = $2;
   }
 | tagged_kind_p ',' kind name {
     $$ = $1;
     FbleTaggedKind* type_field = FbleExtendVector($$);
     type_field->kind = $3;
     type_field->name = $4;
   }
 ;

expr:
   '@' '<' expr '>' {
     FbleTypeofExpr* typeof = FbleAlloc(FbleTypeofExpr);
     typeof->_base.tag = FBLE_TYPEOF_EXPR;
     typeof->_base.loc = FbleCopyLoc(@$);
     typeof->expr = $3;
     $$ = &typeof->_base;
   }
 | name {
      FbleVarExpr* var_expr = FbleAlloc(FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = FbleCopyLoc(@$);
      var_expr->var = $1;
      $$ = &var_expr->_base;
   }
 | module_path {
      FbleModulePathExpr* path_expr = FbleAlloc(FbleModulePathExpr);
      path_expr->_base.tag = FBLE_MODULE_PATH_EXPR;
      path_expr->_base.loc = FbleCopyLoc(@$);
      path_expr->path = $1;
      $$ = &path_expr->_base;

      bool found = false;
      for (size_t i = 0; i < deps->size; ++i) {
        if (FbleModulePathsEqual(deps->xs[i], path_expr->path)) {
          found = true;
          break;
        }
      }
      if (!found) {
        FbleAppendToVector(*deps, FbleCopyModulePath(path_expr->path));
      }
   }
 | '*' '(' tagged_type_s ')' {
      FbleDataTypeExpr* struct_type = FbleAlloc(FbleDataTypeExpr);
      struct_type->_base.tag = FBLE_DATA_TYPE_EXPR;
      struct_type->_base.loc = FbleCopyLoc(@$);
      struct_type->datatype = FBLE_STRUCT_DATATYPE;
      struct_type->fields = $3;
      $$ = &struct_type->_base;
   }
 | expr '(' expr_s ')' {
      FbleApplyExpr* apply_expr = FbleAlloc(FbleApplyExpr);
      apply_expr->_base.tag = FBLE_MISC_APPLY_EXPR;
      apply_expr->_base.loc = FbleCopyLoc(@$);
      apply_expr->misc = $1;
      apply_expr->args = $3;
      apply_expr->bind = false;
      $$ = &apply_expr->_base;
   }
 | '@' '(' implicit_tagged_expr_s ')' {
      FbleStructValueImplicitTypeExpr* expr = FbleAlloc(FbleStructValueImplicitTypeExpr);
      expr->_base.tag = FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR;
      expr->_base.loc = FbleCopyLoc(@$);
      expr->args = $3;
      $$ = &expr->_base;
   }
 | expr '.' name {
      FbleDataAccessExpr* access_expr = FbleAlloc(FbleDataAccessExpr);
      access_expr->_base.tag = FBLE_DATA_ACCESS_EXPR;
      access_expr->_base.loc = FbleCopyLoc(@$);
      access_expr->object = $1;
      access_expr->field = $3;
      $$ = &access_expr->_base;
   }
 | expr '.' '@' '(' implicit_tagged_expr_p ')' {
      FbleStructCopyExpr* expr = FbleAlloc(FbleStructCopyExpr);
      expr->_base.tag = FBLE_STRUCT_COPY_EXPR;
      expr->_base.loc = FbleCopyLoc(@$);
      expr->src = $1;
      expr->args = $5;
      $$ = &expr->_base;
   }
 | '+' '(' tagged_type_p ')' {
      FbleDataTypeExpr* union_type = FbleAlloc(FbleDataTypeExpr);
      union_type->_base.tag = FBLE_DATA_TYPE_EXPR;
      union_type->_base.loc = FbleCopyLoc(@$);
      union_type->datatype = FBLE_UNION_DATATYPE;
      union_type->fields = $3;
      $$ = &union_type->_base;
   }
 | expr '(' name ':' expr ')' {
      FbleUnionValueExpr* union_value_expr = FbleAlloc(FbleUnionValueExpr);
      union_value_expr->_base.tag = FBLE_UNION_VALUE_EXPR;
      union_value_expr->_base.loc = FbleCopyLoc(@$);
      union_value_expr->type = $1;
      union_value_expr->field = $3;
      union_value_expr->arg = $5;
      $$ = &union_value_expr->_base;
   }
 | expr '.' '?' '(' tagged_expr_p ')' {
      FbleUnionSelectExpr* select_expr = FbleAlloc(FbleUnionSelectExpr);
      select_expr->_base.tag = FBLE_UNION_SELECT_EXPR;
      select_expr->_base.loc = FbleCopyLoc(@$);
      select_expr->condition = $1;
      select_expr->choices = $5;
      select_expr->default_ = NULL;
      $$ = &select_expr->_base;
   }
 | expr '.' '?' '(' tagged_expr_p ',' ':' expr ')' {
      FbleUnionSelectExpr* select_expr = FbleAlloc(FbleUnionSelectExpr);
      select_expr->_base.tag = FBLE_UNION_SELECT_EXPR;
      select_expr->_base.loc = FbleCopyLoc(@$);
      select_expr->condition = $1;
      select_expr->choices = $5;
      select_expr->default_ = $8;
      $$ = &select_expr->_base;
   }
 | expr '<' expr_p '>' {
      $$ = $1;
      for (size_t i = 0; i < $3.size; ++i) {
        FblePolyApplyExpr* poly_apply_expr = FbleAlloc(FblePolyApplyExpr);
        poly_apply_expr->_base.tag = FBLE_POLY_APPLY_EXPR;
        poly_apply_expr->_base.loc = FbleCopyLoc(@$);
        poly_apply_expr->poly = $$;
        poly_apply_expr->arg = $3.xs[i];
        $$ = &poly_apply_expr->_base;
      }
      FbleFreeVector($3);
   }
 | expr '.' '%' '(' module_path ')' {
     // TODO: Implement this properly. For now just pass through the
     // expression.
     FbleFreeModulePath($5);
     $$ = $1;
   }
 | expr '[' expr_s ']' {
      FbleListExpr* list_expr = FbleAlloc(FbleListExpr);
      list_expr->_base.tag = FBLE_LIST_EXPR;
      list_expr->_base.loc = FbleCopyLoc(@$);
      list_expr->func = $1;
      list_expr->args = $3;
      $$ = &list_expr->_base;
   }
 | expr '|' WORD {
      FbleLiteralExpr* literal_expr = FbleAlloc(FbleLiteralExpr);
      literal_expr->_base.tag = FBLE_LITERAL_EXPR;
      literal_expr->_base.loc = FbleCopyLoc(@$);
      literal_expr->func = $1;
      literal_expr->word_loc = FbleCopyLoc(@3);
      literal_expr->word = $3;
      $$ = &literal_expr->_base;
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
      FbleExpr* expr = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleFuncTypeExpr* func_type = FbleAlloc(FbleFuncTypeExpr);
        func_type->_base.tag = FBLE_FUNC_TYPE_EXPR;
        func_type->_base.loc = FbleCopyLoc(@$);
        func_type->arg = $2.xs[$2.size - 1 - i];
        func_type->rtype = expr;
        expr = &func_type->_base;
      }
      FbleFreeVector($2);
      $$ = expr;
   }
 | '(' tagged_type_p ')' block {
      FbleExpr* expr = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleFuncValueExpr* func_value_expr = FbleAlloc(FbleFuncValueExpr);
        func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
        func_value_expr->_base.loc = FbleCopyLoc(@$);
        func_value_expr->arg = $2.xs[$2.size - 1 - i];
        func_value_expr->body = expr;
        expr = &func_value_expr->_base;
      }
      FbleFreeVector($2);
      $$ = expr;
   }
 | '<' tagged_kind_p '>' block {
      FbleExpr* expr = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleTaggedKind* arg = $2.xs + $2.size - 1 - i;
        FblePolyValueExpr* poly_expr = FbleAlloc(FblePolyValueExpr);
        poly_expr->_base.tag = FBLE_POLY_VALUE_EXPR;
        poly_expr->_base.loc = FbleCopyLoc(@$);
        poly_expr->arg.kind = arg->kind;
        poly_expr->arg.name = arg->name;
        poly_expr->body = expr;
        expr = &poly_expr->_base;
      }
      FbleFreeVector($2);
      $$ = expr;
   }
 ;

stmt:
    expr ';' { $$ = $1; }
  | let_binding_p ';' stmt {
      FbleLetExpr* let_expr = FbleAlloc(FbleLetExpr);
      let_expr->_base.tag = FBLE_LET_EXPR;
      let_expr->_base.loc = FbleCopyLoc(@$);
      let_expr->bindings = $1;
      let_expr->body = $3;
      $$ = &let_expr->_base;
    }  
  | expr name ';' stmt {
      FbleUndefExpr* undef_expr = FbleAlloc(FbleUndefExpr);
      undef_expr->_base.tag = FBLE_UNDEF_EXPR;
      undef_expr->_base.loc = FbleCopyLoc(@$);
      undef_expr->type = $1;
      undef_expr->name = $2;
      undef_expr->body = $4;
      $$ = &undef_expr->_base;
    }
  | tagged_type_p '<' '-' expr ';' stmt {
      FbleExpr* expr = $6;
      for (size_t i = 0; i < $1.size; ++i) {
        FbleFuncValueExpr* func_value_expr = FbleAlloc(FbleFuncValueExpr);
        func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
        func_value_expr->_base.loc = FbleCopyLoc(@$);
        func_value_expr->arg = $1.xs[$1.size - 1 - i];
        func_value_expr->body = expr;
        expr = &func_value_expr->_base;
      }
      FbleFreeVector($1);

      FbleApplyExpr* apply_expr = FbleAlloc(FbleApplyExpr);
      apply_expr->_base.tag = FBLE_MISC_APPLY_EXPR;
      apply_expr->_base.loc = FbleCopyLoc(@$);
      apply_expr->misc = $4;
      apply_expr->bind = true;
      FbleInitVector(apply_expr->args);
      FbleAppendToVector(apply_expr->args, expr);
      $$ = &apply_expr->_base;
    }
  | expr ';' stmt {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)$1;
      if (select_expr->_base.tag != FBLE_UNION_SELECT_EXPR
           || select_expr->default_ != NULL) {
        FbleReportError("unexpected stmt\n", @3);
        FbleFreeExpr($1);
        FbleFreeExpr($3);
        YYERROR;
      }
  
      select_expr->default_ = $3;
      $$ = &select_expr->_base;
    }
  ;

expr_p:
   expr {
     FbleInitVector($$);
     FbleAppendToVector($$, $1);
   }
 | expr_p ',' expr {
     $$ = $1;
     FbleAppendToVector($$, $3);
   }
 ;

expr_s:
   %empty { FbleInitVector($$); }
 | expr_p { $$ = $1; }
 ;

tagged_type:
   expr name {
     $$.type = $1;
     $$.name = $2;
   }
 ;

tagged_type_p:
   tagged_type {
     FbleInitVector($$);
     FbleTaggedTypeExpr* field = FbleExtendVector($$);
     field->type = $1.type;
     field->name = $1.name;
   }
 | tagged_type_p ',' tagged_type {
     $$ = $1;
     FbleTaggedTypeExpr* field = FbleExtendVector($$);
     field->type = $3.type;
     field->name = $3.name;
   }
 ;

tagged_type_s:
   %empty { FbleInitVector($$); }
 | tagged_type_p { $$ = $1; }
 ;

implicit_tagged_expr:
    name {
      $$.name = $1;

      FbleVarExpr* var_expr = FbleAlloc(FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = FbleCopyLoc(@$);
      var_expr->var = FbleCopyName($1);
      $$.expr = &var_expr->_base;
    }
  | name ':' expr {
      $$.name = $1;
      $$.expr = $3;
    }
  ;

implicit_tagged_expr_p:
  implicit_tagged_expr {
      FbleInitVector($$);
      FbleAppendToVector($$, $1);
    }
  | implicit_tagged_expr_p ',' implicit_tagged_expr {
      $$ = $1;
      FbleAppendToVector($$, $3);
    }
  ;

implicit_tagged_expr_s:
    %empty { FbleInitVector($$); }
  | implicit_tagged_expr_p { $$ = $1; }
  ;

tagged_expr:
    name ':' expr {
      $$.name = $1;
      $$.expr = $3;
    }
  ;

tagged_expr_p:
  tagged_expr {
      FbleInitVector($$);
      FbleAppendToVector($$, $1);
    }
  | tagged_expr_p ',' tagged_expr {
      $$ = $1;
      FbleAppendToVector($$, $3);
    }
  ;

let_binding_p: 
  expr name '=' expr {
      FbleInitVector($$);
      FbleBinding* binding = FbleExtendVector($$);
      binding->kind = NULL;
      binding->type = $1;
      binding->name = $2;
      binding->expr = $4;
    }
  | kind name '=' expr {
      FbleInitVector($$);
      FbleBinding* binding = FbleExtendVector($$);
      binding->kind = $1;
      binding->type = NULL;
      binding->name = $2;
      binding->expr = $4;
    }
  | let_binding_p ',' expr name '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleExtendVector($$);
      binding->kind = NULL;
      binding->type = $3;
      binding->name = $4;
      binding->expr = $6;
    }
  | let_binding_p ',' kind name '=' expr {
      $$ = $1;
      FbleBinding* binding = FbleExtendVector($$);
      binding->kind = $3;
      binding->type = NULL;
      binding->name = $4;
      binding->expr = $6;
    }
  ;

%%
/**
 * @func[ToString] Converts a dynamically allocated char* to FbleString*.
 *  @arg[const char*][str] The string to convert
 * 
 *  @returns[FbleString*]
 *   An FbleString version of str.
 *
 *  @sideeffects
 *   Frees @a[str]. Allocates a new FbleString that should be freed using
 *   FbleFreeString.
 */
static FbleString* ToString(const char* str)
{
  FbleString* string = FbleNewString(str);
  FbleFree((char*)str);
  return string;
}

/**
 * @func[IsSpaceChar] Tests whether a character is whitespace.
 *  @arg[int][c]
 *   The character to test. This must have a value of an unsigned char or EOF.
 *
 *  @returns[bool]
 *   The value true if the character is whitespace, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
static bool IsSpaceChar(int c)
{
  return isspace(c);
}

/**
 * @func[IsPunctiationChar] Tests for single-character punctuation tokens.
 *  @arg[int][c]
 *   The character to test. This must have a value of an unsigned char or EOF.
 *
 *  @returns[bool]
 *   The value true if the character is one of the fble punctuation tokens,
 *   false otherwise.
 *
 *  @sideeffects
 *   None.
 */
static bool IsPunctuationChar(int c)
{
  return c == EOF || strchr("(){}[];,:?=.<>+*-!$@~&|%/^", c) != NULL;
}

/**
 * @func[IsWordChar] Tests for word character.
 *  Tests whether a character is a normal character suitable for direct use in
 *  fble words.
 *
 *  @arg[int][c]
 *   The character to test. This must have a value of an unsigned char or EOF.
 *
 *  @returns[bool]
 *   The value true if the character is a normal character, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
static bool IsWordChar(int c)
{
  if (IsSpaceChar(c) || IsPunctuationChar(c) || c == '\'' || c == '#') {
    return false;
  }
  return true;
}

/**
 * @func[ReadNextChar] Advances to the next character in the input stream.
 *  @arg[Lex*][lex] The context of the lexer to advance.
 *
 *  @sideeffects
 *   Updates the character, location, and input stream of the lex context.
 */
static void ReadNextChar(Lex* lex)
{
  int c = lex->c;

  if (lex->fin != NULL) {
    assert(lex->sin == NULL);
    lex->c = fgetc(lex->fin);
  } else {
    assert(lex->sin != NULL);
    if (*lex->sin == '\0') {
      lex->c = EOF;
    } else {
      lex->c = *lex->sin;
      lex->sin++;
    }
  }

  if (c == '\n') {
    lex->loc.line++;
    lex->loc.col = 1;
  } else if (c != EOF) {
    lex->loc.col++;
  }
}

/**
 * @func[yylex] Lex function for parser.
 *  Returns the next token in the input stream for the given lex context.
 *  This is the lexer for the bison generated parser.
 *
 *  @arg[YYSTYPE*][lvalp] Output parameter for returned token value.
 *  @arg[YYLTYPE*][llocp] Output parameter for the returned token's location.
 *  @arg[Lex*][lex] The lex context to parse the next token from.
 * 
 *  @returns[int]
 *   The id of the next terminal symbol in the input stream.
 *
 *  @sideeffects
 *   Sets lvalp with the semantic value of the next token in the input
 *   stream, sets llocp with the location of the next token in the input
 *   stream, advances the stream to the subsequent token and updates the lex
 *   loc accordingly.
 */
static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, Lex* lex)
{
  if (lex->start_token) {
    int token = lex->start_token;
    lex->start_token = 0;
    return token;
  }

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
  FbleInitVector(wordv);

  if (lex->c == '\'') {
    do {
      ReadNextChar(lex);
      while (lex->c != EOF && lex->c != '\'') {
        FbleAppendToVector(wordv, lex->c);
        ReadNextChar(lex);
      }
      ReadNextChar(lex);
      if (lex->c == '\'') {
        FbleAppendToVector(wordv, '\'');
      }
    } while (lex->c == '\'');
  } else {
    while (IsWordChar(lex->c)) {
      FbleAppendToVector(wordv, lex->c);
      ReadNextChar(lex);
    }
  }
  FbleAppendToVector(wordv, '\0');

  lvalp->word = wordv.xs;
  return WORD;
}

/**
 * @func[yyerror] The error reporting function for the bison generated parser.
 *  @arg[YYLTYPE*][llocp] The location of the error.
 *  @arg[Lex*][lex] Unused.
 *  @arg[FbleExpr**][result] Unused.
 *  @arg[FbleModulePathV*][deps] Unused.
 *  @arg[const char*][msg] The error message.
 *
 *  @sideeffects
 *   An error message is printed to stderr.
 */
static void yyerror(YYLTYPE* llocp, Lex* lex, FbleExpr** result, FbleModulePathV* deps, const char* msg)
{
  (void)lex;
  (void)result;
  (void)deps;

  FbleReportError("%s\n", *llocp, msg);
}

// See documentation in parse.h
bool FbleIsPlainWord(const char* word)
{
  for (const char* c = word; *c != '\0'; c++) {
    if (!IsWordChar(*c)) {
      return false;
    }
  }
  return true;
}

// See documentation in fble-load.h.
FbleExpr* FbleParse(FbleString* filename, FbleModulePathV* deps)
{
  FILE* fin = fopen(filename->str, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open file %s for parsing.\n", filename->str);
    return NULL;
  }

  Lex lex = {
    .c = ' ',
    .loc = { .source = filename, .line = 1, .col = 0 },
    .fin = fin,
    .sin = NULL,
    .start_token = PARSE_PROGRAM
  };
  FbleExpr* result = NULL;
  yyparse(&lex, &result, deps);
  fclose(fin);
  return result;
}

// See documentation in fble-module-path.h.
FbleModulePath* FbleParseModulePath(const char* string)
{
  char source[strlen(string) + 3];
  source[0] = '\0';
  strcat(source, "'");
  strcat(source, string);
  strcat(source, "'");

  Lex lex = {
    .c = ' ',
    .loc = { .source = FbleNewString(source), .line = 1, .col = 0 },
    .fin = NULL,
    .sin = string,
    .start_token = PARSE_MODULE_PATH
  };

  FbleModulePathV path;
  FbleInitVector(path);
  yyparse(&lex, NULL, &path);
  FbleFreeLoc(lex.loc);

  FbleModulePath* result = NULL;
  assert(path.size < 2);
  if (path.size > 0) {
    result = path.xs[0];
  }
  FbleFreeVector(path);
  return result;
}
