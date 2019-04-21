// fble-syntax.h --
//   Header file for the fble abstract syntax.

#ifndef FBLE_SYNTAX_H_
#define FBLE_SYNTAX_H_

#include <stdbool.h>      // for bool
#include <sys/types.h>    // for size_t

// FbleLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  const char* source;
  int line;
  int col;
} FbleLoc;

// FbleReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
void FbleReportError(const char* format, FbleLoc* loc, ...);

// FbleName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FbleLoc loc;
} FbleName;

// FbleNamesEqual --
//   Test whether two names are equal.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the first name equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbleNamesEqual(const char* a, const char* b);

// FbleKindTag --
//   A tag used to distinguish between the two kinds of kinds.
typedef enum {
  FBLE_BASIC_KIND,
  FBLE_POLY_KIND
} FbleKindTag;

// FbleKind --
//   A tagged union of kind types. All kinds have the same initial
//   layout as FbleKind. The tag can be used to determine what kind of
//   kind this is to get access to additional fields of the kind
//   by first casting to that specific type of kind.
typedef struct {
  FbleKindTag tag;
  FbleLoc loc;
} FbleKind;

// FbleKindV --
//   A vector of FbleKind.
typedef struct {
  size_t size;
  FbleKind** xs;
} FbleKindV;

// FbleBasicKind --
//   FBLE_BASIC_KIND
typedef struct {
  FbleKind _base;
} FbleBasicKind;

// FblePolyKind --
//   FBLE_POLY_KIND (arg :: Kind) (return :: Kind)
typedef struct {
  FbleKind _base;
  FbleKind* arg;
  FbleKind* rkind;
} FblePolyKind;

// FbleExprTag --
//   A tag used to dinstinguish among different kinds of expressions.
typedef enum {
  FBLE_STRUCT_TYPE_EXPR,
//FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR = FBLE_MISC_APPLY_EXPR,
  FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,
//FBLE_STRUCT_ACCESS_EXPR = FBLE_MISC_ACCESS_EXPR,
  FBLE_STRUCT_EVAL_EXPR,
  FBLE_STRUCT_IMPORT_EXPR,

  FBLE_UNION_TYPE_EXPR,
  FBLE_UNION_VALUE_EXPR,
//FBLE_UNION_ACCESS_EXPR = FBLE_MISC_ACCESS_EXPR,
  FBLE_UNION_SELECT_EXPR,

  FBLE_FUNC_TYPE_EXPR,
  FBLE_FUNC_VALUE_EXPR,
  FBLE_FUNC_APPLY_EXPR,

  FBLE_PROC_TYPE_EXPR,
  FBLE_INPUT_TYPE_EXPR,
  FBLE_OUTPUT_TYPE_EXPR,
//FBLE_GET_EXPR = FBLE_MISC_APPLY_EXPR,
//FBLE_PUT_EXPR = FBLE_MISC_APPLY_EXPR,
  FBLE_EVAL_EXPR,
  FBLE_LINK_EXPR,
  FBLE_EXEC_EXPR,

  FBLE_VAR_EXPR,
  FBLE_LET_EXPR,
  FBLE_TYPEOF_EXPR,
  FBLE_POLY_EXPR,
  FBLE_POLY_APPLY_EXPR,

  FBLE_MISC_ACCESS_EXPR,  // Used for STRUCT_ACCESS, UNION_ACCESS
  FBLE_MISC_APPLY_EXPR,   // Used for STRUCT_VALUE_EXPLICIT_TYPE, PUT, GET
} FbleExprTag;

// FbleExpr --
//   A tagged union of expr types. All exprs have the same initial
//   layout as FbleExpr. The tag can be used to determine what kind of
//   expr this is to get access to additional fields of the expr
//   by first casting to that specific type of expr.
typedef struct {
  FbleExprTag tag;
  FbleLoc loc;
} FbleExpr;

// FbleExprV --
//   A vector of FbleExpr.
typedef struct {
  size_t size;
  FbleExpr** xs;
} FbleExprV;

// FbleType -- Synonym for FbleExpr when a type is expected
typedef FbleExpr FbleType;
typedef FbleExprV FbleTypeV;

// FbleField --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  FbleType* type;
  FbleName name;
} FbleField;

// FbleFieldV --
//   A vector of FbleField.
typedef struct {
  size_t size;
  FbleField* xs;
} FbleFieldV;

// FbleStructTypeExpr --
//   FBLE_STRUCT_TYPE_EXPR (fields :: [(Type, Name)])
typedef struct {
  FbleType _base;
  FbleFieldV fields;
} FbleStructTypeExpr;

// FbleChoice --
//   A pair of (Name, Expr) used in conditional expressions and anonymous
//   struct values.
typedef struct {
  FbleName name;
  FbleExpr* expr;
} FbleChoice;

// FbleChoiceV --
//   A vector of FbleChoice.
typedef struct {
  size_t size;
  FbleChoice* xs;
} FbleChoiceV;

// FbleStructValueImplicitTypeExpr --
//   FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR (args :: [(Name, Expr)])
typedef struct {
  FbleExpr _base;
  FbleChoiceV args;
} FbleStructValueImplicitTypeExpr;

// FbleStructEvalExpr --
//   FBLE_STRUCT_EVAL_EXPR (namespace :: Expr) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* nspace;
  FbleExpr* body;
} FbleStructEvalExpr;

// FbleStructImportExpr --
//   FBLE_STRUCT_IMPORT_EXPR (namespace :: Expr) (body :: Expr)
typedef FbleStructEvalExpr FbleStructImportExpr;

// FbleUnionTypeExpr --
//   FBLE_UNION_TYPE_EXPR (fields :: [(Type, Name)])
typedef struct {
  FbleType _base;
  FbleFieldV fields;
} FbleUnionTypeExpr;

// FbleUnionValueExpr --
//   FBLE_UNION_VALUE_EXPR (type :: Type) (field :: Name) (arg :: Expr)
typedef struct {
  FbleExpr _base;
  FbleType* type;
  FbleName field;
  FbleExpr* arg;
} FbleUnionValueExpr;

// FbleUnionSelectExpr --
//   FBLE_UNION_SELECT_EXPR (condition :: Expr) (choices :: [(Name, Expr)])
typedef struct {
  FbleExpr _base;
  FbleExpr* condition;
  FbleChoiceV choices;
} FbleUnionSelectExpr;

// FbleFuncTypeExpr --
//   FBLE_FUNC_TYPE_EXPR (arg :: Type) (return :: Type)
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleType* rtype;
} FbleFuncTypeExpr;

// FbleFuncValueExpr --
//   FBLE_FUNC_VALUE_EXPR (arg :: (Type, Name)) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleField arg;
  FbleExpr* body;
} FbleFuncValueExpr;

// FbleFuncApplyExpr --
//   FBLE_FUNC_APPLY_EXPR (func :: Expr) (arg :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* func;
  FbleExpr* arg;
} FbleFuncApplyExpr;

// FbleUnaryTypeExpr --
//   FBLE_PROC_TYPE_EXPR (type :: Type)
//   FBLE_INPUT_TYPE_EXPR (type :: Type)
//   FBLE_OUTPUT_TYPE_EXPR (type :: Type)
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleUnaryTypeExpr;

// FbleEvalExpr --
//   FBLE_EVAL_EXPR (expr :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* expr;
} FbleEvalExpr;

// FbleLinkExpr --
//   FBLE_LINK_EXPR (type :: Type) (get :: Name) (put :: Name) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleType* type;
  FbleName get;
  FbleName put;
  FbleExpr* body;
} FbleLinkExpr;

// FbleBinding --
//   (Kind, Type, Name, Expr) used in let and exec expressions.
// Exactly one of Kind or Type should be NULL. If the Kind is null, it is
// inferred from the given Type. If the Type is null, it is inferred from the
// given Expr.
typedef struct {
  FbleKind* kind;
  FbleType* type;
  FbleName name;
  FbleExpr* expr;
} FbleBinding;

// FbleBindingV --
//   A vector of FbleBinding
typedef struct {
  size_t size;
  FbleBinding* xs;
} FbleBindingV;

// FbleExecExpr --
//   FBLE_EXEC_EXPR (bindings :: [(Type, Name, Expr)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleBindingV bindings;
  FbleExpr* body;
} FbleExecExpr;

// FbleVarExpr --
//   FBLE_VAR_EXPR (name :: Name)
typedef struct {
  FbleExpr _base;
  FbleName var;
} FbleVarExpr;

// FbleLetExpr --
//   FBLE_LET_EXPR (bindings :: [(Type, Name, Expr)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleBindingV bindings;
  FbleExpr* body;
} FbleLetExpr;

// FbleTypeofExpr --
//   FBLE_TYPEOF_EXPR (expr :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* expr;
} FbleTypeofExpr;

// FbleTypeField --
//   A pair of (Kind, Name) used to describe poly arguments.
typedef struct {
  FbleKind* kind;
  FbleName name;
} FbleTypeField;

// FblePolyExpr --
//   FBLE_POLY_EXPR (arg :: (Kind, Name)) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTypeField arg;
  FbleExpr* body;
} FblePolyExpr;

// FblePolyApplyExpr --
//   FBLE_POLY_APPLY_EXPR (poly :: Expr) (arg :: Type)
typedef struct {
  FbleExpr _base;
  FbleExpr* poly;
  FbleType* arg;
} FblePolyApplyExpr;

// FbleMiscApplyExpr --
//   FBLE_MISC_APPLY_EXPR (misc :: Expr) (args :: [Expr])
//   FBLE_STRUCT_VALUE_EXPR (type :: Type) (args :: [Expr])
//   FBLE_GET_EXPR (port :: Expr)
//   FBLE_PUT_EXPR (port :: Expr) (arg :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* misc;
  FbleExprV args;
} FbleMiscApplyExpr;

// FbleMiscAccessExpr --
//   FBLE_MISC_ACCESS_EXPR (object :: Expr) (field :: Name)
//   FBLE_STRUCT_ACCESS_EXPR (object :: Expr) (field :: Name)
//   FBLE_UNION_ACCESS_EXPR (object :: Expr) (field :: Name)
// Common form used for both struct and union access.
typedef struct {
  FbleExpr _base;
  FbleExpr* object;
  FbleName field;
} FbleMiscAccessExpr;

#endif // FBLE_SYNTAX_H_
