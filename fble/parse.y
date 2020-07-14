// parse.y --
//   This file describes the bison grammar for parsing fble expressions.
%{
  #include <assert.h>   // for assert
  #include <ctype.h>    // for isalnum
  #include <stdbool.h>  // for bool
  #include <stdio.h>    // for FILE, fprintf, stderr
  #include <string.h>   // for strchr, strcpy

  #include "fble.h"
  #include "syntax.h"

  // Lex --
  //   State for the lexer.
  //
  // Fields:
  //   c - The next character in the input stream. EOF is used to indicate the
  //   end of the input stream.
  //   loc - The location corresponding to the character c.
  //   fin - The input stream.
  typedef struct {
    int c;
    FbleLoc loc;
    FILE* fin;
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
  FbleModuleRef module_ref;
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
  static FbleString* ToString(FbleArena* arena, const char* str);
  static bool IsSpaceChar(int c);
  static bool IsPunctuationChar(int c);
  static bool IsNormalChar(int c);
  static void ReadNextChar(Lex* lex);
  static int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, FbleArena* arena, Lex* lex);
  static void yyerror(YYLTYPE* llocp, FbleArena* arena, Lex* lex, FbleExpr** result, FbleModuleRefV* module_refs, const char* msg);
%}

%locations
%define api.pure
%define parse.error verbose
%param {FbleArena* arena} {Lex* lex}
%parse-param {FbleExpr** result} {FbleModuleRefV* module_refs}

%token END 0 "end of file"
%token INVALID "invalid character"

%token <word> WORD

%type <name> name
%type <names> path
%type <module_ref> module_ref
%type <kind> tkind nkind kind
%type <kinds> tkind_p
%type <tagged_kinds> tagged_kind_p
%type <expr> expr block stmt
%type <exprs> expr_p expr_s
%type <tagged_type> tagged_type
%type <tagged_types> tagged_type_p tagged_type_s
%type <tagged_expr> implicit_tagged_expr tagged_expr
%type <tagged_exprs> implicit_tagged_expr_p implicit_tagged_expr_s tagged_expr_p
%type <bindings> exec_binding_p let_binding_p

%destructor {
  FbleFree(arena, (char*)$$);
} <word>

%destructor {
  FbleFreeName(arena, $$);
} <name>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeName(arena, $$.xs[i]);
  }
  FbleFree(arena, $$.xs);
} <names>

%destructor {
  for (size_t i = 0; i < $$.path.size; ++i) {
    FbleFreeName(arena, $$.path.xs[i]);
  }
  FbleFree(arena, $$.path.xs);
} <module_ref>

%destructor {
  FbleFreeKind(arena, $$);
} <kind>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeKind(arena, $$.xs[i]);
  }
  FbleFree(arena, $$.xs);
} <kinds>

%destructor { 
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeKind(arena, $$.xs[i].kind);
    FbleFreeName(arena, $$.xs[i].name);
  }
  FbleFree(arena, $$.xs);
} <tagged_kinds>

%destructor {
  FbleFreeExpr(arena, $$);
} <expr>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeExpr(arena, $$.xs[i]);
  }
  FbleFree(arena, $$.xs);
} <exprs>

%destructor { 
  FbleFreeExpr(arena, $$.type);
  FbleFreeName(arena, $$.name);
} <tagged_type>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeExpr(arena, $$.xs[i].type);
    FbleFreeName(arena, $$.xs[i].name);
  }
  FbleFree(arena, $$.xs);
} <tagged_types>

%destructor {
  FbleFreeName(arena, $$.name);
  FbleFreeExpr(arena, $$.expr);
} <tagged_expr>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeName(arena, $$.xs[i].name);
    FbleFreeExpr(arena, $$.xs[i].expr);
  }
  FbleFree(arena, $$.xs);
} <tagged_exprs>

%destructor {
  for (size_t i = 0; i < $$.size; ++i) {
    FbleFreeKind(arena, $$.xs[i].kind);
    FbleFreeExpr(arena, $$.xs[i].type);
    FbleFreeName(arena, $$.xs[i].name);
    FbleFreeExpr(arena, $$.xs[i].expr);
  }
  FbleFree(arena, $$.xs);
} <bindings>

%%

start: stmt { *result = $1; } ;

name:
   WORD {
     $$.name = ToString(arena, $1);
     $$.space = FBLE_NORMAL_NAME_SPACE;
     $$.loc = FbleCopyLoc(@$);
   }
 | WORD '@' {
     $$.name = ToString(arena, $1);
     $$.space = FBLE_TYPE_NAME_SPACE;
     $$.loc = FbleCopyLoc(@$);
   }
 ;

path:
   WORD {
     FbleVectorInit(arena, $$);
     FbleName* name = FbleVectorExtend(arena, $$);
     name->name = ToString(arena, $1);
     name->space = FBLE_NORMAL_NAME_SPACE;  // arbitrary choice
     name->loc = FbleCopyLoc(@$);
   }
 | path '/' WORD {
     $$ = $1;
     FbleName* name = FbleVectorExtend(arena, $$);
     name->name = ToString(arena, $3);
     name->space = FBLE_NORMAL_NAME_SPACE;  // arbitrary choice
     name->loc = FbleCopyLoc(@$);
   };

module_ref:
   '/' path '%' {
      $$.path = $2;
      $$.resolved.name = NULL;
   }
 ;

nkind:
   '%' {
      FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
      basic_kind->_base.tag = FBLE_BASIC_KIND;
      basic_kind->_base.loc = FbleCopyLoc(@$);
      basic_kind->_base.refcount = 1;
      basic_kind->level = 0;
      $$ = &basic_kind->_base;
   }
 | '<' tkind_p '>' nkind {
      FbleKind* kind = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleKind* arg = $2.xs[$2.size - 1 - i];
        FblePolyKind* poly_kind = FbleAlloc(arena, FblePolyKind);
        poly_kind->_base.tag = FBLE_POLY_KIND;
        poly_kind->_base.loc = FbleCopyLoc(@$);
        poly_kind->_base.refcount = 1;
        poly_kind->arg = arg;
        poly_kind->rkind = kind;
        kind = &poly_kind->_base;
      }
      FbleFree(arena, $2.xs);
      $$ = kind;
   }
 ;

tkind:
   '@' {
      FbleBasicKind* basic_kind = FbleAlloc(arena, FbleBasicKind);
      basic_kind->_base.tag = FBLE_BASIC_KIND;
      basic_kind->_base.loc = FbleCopyLoc(@$);
      basic_kind->_base.refcount = 1;
      basic_kind->level = 1;
      $$ = &basic_kind->_base;
   }
 | '<' tkind_p '>' tkind {
      FbleKind* kind = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleKind* arg = $2.xs[$2.size - 1 - i];
        FblePolyKind* poly_kind = FbleAlloc(arena, FblePolyKind);
        poly_kind->_base.tag = FBLE_POLY_KIND;
        poly_kind->_base.loc = FbleCopyLoc(@$);
        poly_kind->_base.refcount = 1;
        poly_kind->arg = arg;
        poly_kind->rkind = kind;
        kind = &poly_kind->_base;
      }
      FbleFree(arena, $2.xs);
      $$ = kind;
   }
 ;

kind: nkind | tkind ;

tkind_p:
   tkind {
     FbleVectorInit(arena, $$);
     FbleVectorAppend(arena, $$, $1);
   }
 | tkind_p ',' tkind {
     $$ = $1;
     FbleVectorAppend(arena, $$, $3);
   }
 ;

tagged_kind_p:
   kind name {
     FbleVectorInit(arena, $$);
     FbleTaggedKind* type_field = FbleVectorExtend(arena, $$);
     type_field->kind = $1;
     type_field->name = $2;
   }
 | tagged_kind_p ',' kind name {
     $$ = $1;
     FbleTaggedKind* type_field = FbleVectorExtend(arena, $$);
     type_field->kind = $3;
     type_field->name = $4;
   }
 ;

expr:
   '@' '<' expr '>' {
     FbleTypeofExpr* typeof = FbleAlloc(arena, FbleTypeofExpr);
     typeof->_base.tag = FBLE_TYPEOF_EXPR;
     typeof->_base.loc = FbleCopyLoc(@$);
     typeof->expr = $3;
     $$ = &typeof->_base;
   }
 | name {
      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = FbleCopyLoc(@$);
      var_expr->var = $1;
      $$ = &var_expr->_base;
   }
 | module_ref {
      FbleModuleRefExpr* mref = FbleAlloc(arena, FbleModuleRefExpr);
      mref->_base.tag = FBLE_MODULE_REF_EXPR;
      mref->_base.loc = FbleCopyLoc(@$);
      mref->ref = $1;
      $$ = &mref->_base;
      FbleVectorAppend(arena, *module_refs, &mref->ref);
   }
 | '*' '(' tagged_type_s ')' {
      FbleStructTypeExpr* struct_type = FbleAlloc(arena, FbleStructTypeExpr);
      struct_type->_base.tag = FBLE_STRUCT_TYPE_EXPR;
      struct_type->_base.loc = FbleCopyLoc(@$);
      struct_type->fields = $3;
      $$ = &struct_type->_base;
   }
 | '*' '$' '(' tagged_type_s ')' {
      assert(false && "TODO: parse inline struct type");
      //FbleInlineStructTypeExpr* struct_type = FbleAlloc(arena, FbleInlineStructTypeExpr);
      //struct_type->_base.tag = FBLE_INLINE_STRUCT_TYPE_EXPR;
      FbleStructTypeExpr* struct_type = FbleAlloc(arena, FbleStructTypeExpr);
      struct_type->_base.tag = FBLE_STRUCT_TYPE_EXPR;
      struct_type->_base.loc = FbleCopyLoc(@$);
      struct_type->fields = $4;
      $$ = &struct_type->_base;
   }
 | expr '(' expr_s ')' {
      FbleApplyExpr* apply_expr = FbleAlloc(arena, FbleApplyExpr);
      apply_expr->_base.tag = FBLE_MISC_APPLY_EXPR;
      apply_expr->_base.loc = FbleCopyLoc(@$);
      apply_expr->misc = $1;
      apply_expr->args = $3;
      $$ = &apply_expr->_base;
   }
 | '@' '(' implicit_tagged_expr_s ')' {
      FbleStructValueImplicitTypeExpr* expr = FbleAlloc(arena, FbleStructValueImplicitTypeExpr);
      expr->_base.tag = FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR;
      expr->_base.loc = FbleCopyLoc(@$);
      expr->args = $3;
      $$ = &expr->_base;
   }
 | expr '.' name {
      FbleAccessExpr* access_expr = FbleAlloc(arena, FbleAccessExpr);
      access_expr->_base.tag = FBLE_MISC_ACCESS_EXPR;
      access_expr->_base.loc = FbleCopyLoc(@$);
      access_expr->object = $1;
      access_expr->field = $3;
      $$ = &access_expr->_base;
   }
 | '+' '(' tagged_type_p ')' {
      FbleUnionTypeExpr* union_type = FbleAlloc(arena, FbleUnionTypeExpr);
      union_type->_base.tag = FBLE_UNION_TYPE_EXPR;
      union_type->_base.loc = FbleCopyLoc(@$);
      union_type->fields = $3;
      $$ = &union_type->_base;
   }
 | expr '(' name ':' expr ')' {
      FbleUnionValueExpr* union_value_expr = FbleAlloc(arena, FbleUnionValueExpr);
      union_value_expr->_base.tag = FBLE_UNION_VALUE_EXPR;
      union_value_expr->_base.loc = FbleCopyLoc(@$);
      union_value_expr->type = $1;
      union_value_expr->field = $3;
      union_value_expr->arg = $5;
      $$ = &union_value_expr->_base;
   }
 | expr '.' '?' '(' tagged_expr_p ')' {
      FbleUnionSelectExpr* select_expr = FbleAlloc(arena, FbleUnionSelectExpr);
      select_expr->_base.tag = FBLE_UNION_SELECT_EXPR;
      select_expr->_base.loc = FbleCopyLoc(@$);
      select_expr->condition = $1;
      select_expr->choices = $5;
      select_expr->default_ = NULL;
      $$ = &select_expr->_base;
   }
 | expr '.' '?' '(' tagged_expr_p ',' ':' expr ')' {
      FbleUnionSelectExpr* select_expr = FbleAlloc(arena, FbleUnionSelectExpr);
      select_expr->_base.tag = FBLE_UNION_SELECT_EXPR;
      select_expr->_base.loc = FbleCopyLoc(@$);
      select_expr->condition = $1;
      select_expr->choices = $5;
      select_expr->default_ = $8;
      $$ = &select_expr->_base;
   }
 | expr '!' {
      FbleProcTypeExpr* proc_type = FbleAlloc(arena, FbleProcTypeExpr);
      proc_type->_base.tag = FBLE_PROC_TYPE_EXPR;
      proc_type->_base.loc = FbleCopyLoc(@$);
      proc_type->type = $1;
      $$ = &proc_type->_base;
   }
 | '!' '(' expr ')' {
      FbleEvalExpr* eval_expr = FbleAlloc(arena, FbleEvalExpr);
      eval_expr->_base.tag = FBLE_EVAL_EXPR;
      eval_expr->_base.loc = FbleCopyLoc(@$);
      eval_expr->body = $3;
      $$ = &eval_expr->_base;
   }
 | expr '<' expr_p '>' {
      $$ = $1;
      for (size_t i = 0; i < $3.size; ++i) {
        FblePolyApplyExpr* poly_apply_expr = FbleAlloc(arena, FblePolyApplyExpr);
        poly_apply_expr->_base.tag = FBLE_POLY_APPLY_EXPR;
        poly_apply_expr->_base.loc = FbleCopyLoc(@$);
        poly_apply_expr->poly = $$;
        poly_apply_expr->arg = $3.xs[i];
        $$ = &poly_apply_expr->_base;
      }
      FbleFree(arena, $3.xs);
   }
 | '[' expr_p ']' {
      FbleListExpr* list_expr = FbleAlloc(arena, FbleListExpr);
      list_expr->_base.tag = FBLE_LIST_EXPR;
      list_expr->_base.loc = FbleCopyLoc(@$);
      list_expr->args = $2;
      $$ = &list_expr->_base;
   }
 | expr '|' WORD {
      FbleLiteralExpr* literal_expr = FbleAlloc(arena, FbleLiteralExpr);
      literal_expr->_base.tag = FBLE_LITERAL_EXPR;
      literal_expr->_base.loc = FbleCopyLoc(@$);
      literal_expr->spec = $1;
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
      FbleFuncTypeExpr* func_type = FbleAlloc(arena, FbleFuncTypeExpr);
      func_type->_base.tag = FBLE_FUNC_TYPE_EXPR;
      func_type->_base.loc = FbleCopyLoc(@$);
      func_type->args = $2;
      func_type->rtype = $4;
      $$ = &func_type->_base;
   }
 | '(' tagged_type_p ')' block {
      FbleFuncValueExpr* func_value_expr = FbleAlloc(arena, FbleFuncValueExpr);
      func_value_expr->_base.tag = FBLE_FUNC_VALUE_EXPR;
      func_value_expr->_base.loc = FbleCopyLoc(@$);
      func_value_expr->args = $2;
      func_value_expr->body = $4;
      $$ = &func_value_expr->_base;
   }
 | '<' tagged_kind_p '>' block {
      FbleExpr* expr = $4;
      for (size_t i = 0; i < $2.size; ++i) {
        FbleTaggedKind* arg = $2.xs + $2.size - 1 - i;
        FblePolyValueExpr* poly_expr = FbleAlloc(arena, FblePolyValueExpr);
        poly_expr->_base.tag = FBLE_POLY_VALUE_EXPR;
        poly_expr->_base.loc = FbleCopyLoc(@$);
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
      let_expr->_base.loc = FbleCopyLoc(@$);
      let_expr->bindings = $1;
      let_expr->body = $3;
      $$ = &let_expr->_base;
    }  
  | expr '~' name ',' name ';' stmt {
      FbleLinkExpr* link_expr = FbleAlloc(arena, FbleLinkExpr);
      link_expr->_base.tag = FBLE_LINK_EXPR;
      link_expr->_base.loc = FbleCopyLoc(@$);
      link_expr->type = $1;
      link_expr->get = $3;
      link_expr->put = $5;
      link_expr->body = $7;
      $$ = &link_expr->_base;
    }
  | exec_binding_p ';' stmt {
      FbleExecExpr* exec_expr = FbleAlloc(arena, FbleExecExpr);
      exec_expr->_base.tag = FBLE_EXEC_EXPR;
      exec_expr->_base.loc = FbleCopyLoc(@$);
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

tagged_type:
   expr name {
     $$.type = $1;
     $$.name = $2;
   }
 ;

tagged_type_p:
   tagged_type {
     FbleVectorInit(arena, $$);
     FbleTaggedTypeExpr* field = FbleVectorExtend(arena, $$);
     field->type = $1.type;
     field->name = $1.name;
   }
 | tagged_type_p ',' tagged_type {
     $$ = $1;
     FbleTaggedTypeExpr* field = FbleVectorExtend(arena, $$);
     field->type = $3.type;
     field->name = $3.name;
   }
 ;

tagged_type_s:
   %empty { FbleVectorInit(arena, $$); }
 | tagged_type_p { $$ = $1; }
 ;

implicit_tagged_expr:
    name {
      $$.name = $1;

      FbleVarExpr* var_expr = FbleAlloc(arena, FbleVarExpr);
      var_expr->_base.tag = FBLE_VAR_EXPR;
      var_expr->_base.loc = FbleCopyLoc(@$);
      var_expr->var = FbleCopyName(arena, $1);
      $$.expr = &var_expr->_base;
    }
  | name ':' expr {
      $$.name = $1;
      $$.expr = $3;
    }
  ;

implicit_tagged_expr_p:
  implicit_tagged_expr {
      FbleVectorInit(arena, $$);
      FbleVectorAppend(arena, $$, $1);
    }
  | implicit_tagged_expr_p ',' implicit_tagged_expr {
      $$ = $1;
      FbleVectorAppend(arena, $$, $3);
    }
  ;

implicit_tagged_expr_s:
    %empty { FbleVectorInit(arena, $$); }
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
      FbleVectorInit(arena, $$);
      FbleVectorAppend(arena, $$, $1);
    }
  | tagged_expr_p ',' tagged_expr {
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
// ToString --
//   Convert a dynamically allocated char* to FbleString*.
//
// Inputs:
//   arena - arena to use for allocations
//   str - the string to convert
// 
// Results:
//   An FbleString version of str.
//
// Side effects:
//   Frees 'str'. Allocates a new FbleString that should be freed using
//   FbleFreeString.
static FbleString* ToString(FbleArena* arena, const char* str)
{
  FbleString* string = FbleNewString(arena, str);
  FbleFree(arena, (char*)str);
  return string;
}

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
  lex->c = fgetc(lex->fin);
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
//   lex - unused.
//   result - unused.
//   msg - The error message.
//
// Results:
//   None.
//
// Side effects:
//   An error message is printed to stderr.
static void yyerror(YYLTYPE* llocp, FbleArena* arena, Lex* lex, FbleExpr** result, FbleModuleRefV* module_refs, const char* msg)
{
  FbleReportError("%s\n", *llocp, msg);
}

// FbleParse -- see documentation in syntax.h
FbleExpr* FbleParse(FbleArena* arena, FbleString* filename, FbleModuleRefV* module_refs)
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
  };
  FbleExpr* result = NULL;
  yyparse(arena, &lex, &result, module_refs);
  return result;
}
