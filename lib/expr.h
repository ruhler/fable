// expr.h --
//   Header file for FbleExpr, the fble abstract syntax.

#ifndef FBLE_INTERNAL_EXPR_H_
#define FBLE_INTERNAL_EXPR_H_

#include <sys/types.h>    // for size_t

#include <fble/fble-load.h>    // for public typedef of FbleExpr.
#include <fble/fble-name.h>

#include "kind.h"

// FbleExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLE_TYPEOF_EXPR,
  FBLE_VAR_EXPR,
  FBLE_LET_EXPR,

  FBLE_DATA_TYPE_EXPR,    // struct and union types.
  FBLE_DATA_ACCESS_EXPR,  // struct and union field access.

  // FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR = FBLE_MISC_APPLY_EXPR
  FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,

  FBLE_UNION_VALUE_EXPR,
  FBLE_UNION_SELECT_EXPR,

  FBLE_FUNC_TYPE_EXPR,
  FBLE_FUNC_VALUE_EXPR,
  // FBLE_FUNC_APPLY_EXPR = FBLE_MISC_APPLY_EXPR

  FBLE_POLY_VALUE_EXPR,
  FBLE_POLY_APPLY_EXPR,

  FBLE_LIST_EXPR,
  FBLE_LITERAL_EXPR,

  FBLE_MODULE_PATH_EXPR,

  FBLE_PACKAGE_TYPE_EXPR,
  // FBLE_ABSTRACT_TYPE_EXPR = FBLE_POLY_APPLY_EXPR
  FBLE_ABSTRACT_CAST_EXPR,
  FBLE_ABSTRACT_ACCESS_EXPR,

  FBLE_MISC_APPLY_EXPR,
} FbleExprTag;

// FbleExpr --
//   A tagged union of expr types. All exprs have the same initial
//   layout as FbleExpr. The tag can be used to determine what kind of
//   expr this is to get access to additional fields of the expr
//   by first casting to that specific type of expr.
struct FbleExpr {
  FbleExprTag tag;
  FbleLoc loc;
};

// FbleExprV --
//   A vector of FbleExpr.
typedef struct {
  size_t size;
  FbleExpr** xs;
} FbleExprV;

// FbleTypeExpr -- Synonym for FbleExpr when a type is expected
typedef FbleExpr FbleTypeExpr;

// FbleTypeExprV -- Synonym for FbleExprV when types are expected.
typedef FbleExprV FbleTypeExprV;

// FbleTaggedTypeExpr --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  FbleTypeExpr* type;
  FbleName name;
} FbleTaggedTypeExpr;

// FbleTaggedTypeExprV --
//   A vector of FbleTaggedTypeExpr.
typedef struct {
  size_t size;
  FbleTaggedTypeExpr* xs;
} FbleTaggedTypeExprV;

// FbleDataTypeExpr --
//   FBLE_DATA_TYPE_EXPR (fields :: [(Type, Name)])
typedef struct {
  FbleTypeExpr _base;
  FbleDataTypeTag datatype;
  FbleTaggedTypeExprV fields;
} FbleDataTypeExpr;

// FbleTaggedExpr --
//   A pair of (Name, Expr) used in conditional expressions and anonymous
//   struct values.
typedef struct {
  FbleName name;
  FbleExpr* expr;
} FbleTaggedExpr;

// FbleTaggedExprV --
//   A vector of FbleTaggedExpr.
typedef struct {
  size_t size;
  FbleTaggedExpr* xs;
} FbleTaggedExprV;

// FbleStructValueImplicitTypeExpr --
//   FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR (args :: [(Name, Expr)])
typedef struct {
  FbleExpr _base;
  FbleTaggedExprV args;
} FbleStructValueImplicitTypeExpr;

// FbleUnionValueExpr --
//   FBLE_UNION_VALUE_EXPR (type :: Type) (field :: Name) (arg :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTypeExpr* type;
  FbleName field;
  FbleExpr* arg;
} FbleUnionValueExpr;

// FbleUnionSelectExpr --
//   FBLE_UNION_SELECT_EXPR (condition :: Expr) (choices :: [(Name, Expr)]) (default :: Expr)
//
// default_ is NULL if no default is provided.
//
// typecheck inserts tagged expressions into choices with expr set to NULL to
// indicate default branches explicitly.
typedef struct {
  FbleExpr _base;
  FbleExpr* condition;
  FbleTaggedExprV choices;
  FbleExpr* default_;
} FbleUnionSelectExpr;

// FbleFuncTypeExpr --
//   FBLE_FUNC_TYPE_EXPR (arg :: Type) (return :: Type)
typedef struct {
  FbleTypeExpr _base;
  FbleTypeExprV args;
  FbleTypeExpr* rtype;
} FbleFuncTypeExpr;

// FbleFuncValueExpr --
//   FBLE_FUNC_VALUE_EXPR (args :: [(Type, Name)]) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTaggedTypeExprV args;
  FbleExpr* body;
} FbleFuncValueExpr;

// FbleBinding --
//   (Kind, Type, Name, Expr) used in let and exec expressions.
// Exactly one of Kind or Type should be NULL. If the Kind is null, it is
// inferred from the given Type. If the Type is null, it is inferred from the
// given Expr.
typedef struct {
  FbleKind* kind;
  FbleTypeExpr* type;
  FbleName name;
  FbleExpr* expr;
} FbleBinding;

// FbleBindingV --
//   A vector of FbleBinding
typedef struct {
  size_t size;
  FbleBinding* xs;
} FbleBindingV;

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

// FbleModulePathExpr --
//   FBLE_MODULE_PATH_EXPR (path :: ModulePath)
typedef struct {
  FbleExpr _base;
  FbleModulePath* path;
} FbleModulePathExpr;

// FbleTypeofExpr --
//   FBLE_TYPEOF_EXPR (expr :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* expr;
} FbleTypeofExpr;

// FbleTaggedKind --
//   A pair of (Kind, Name) used to describe poly arguments.
typedef struct {
  FbleKind* kind;
  FbleName name;
} FbleTaggedKind;

// FbleTaggedKindV -- 
//   A vector of FbleTaggedKinds.
typedef struct {
  FbleTaggedKind* xs;
  size_t size;
} FbleTaggedKindV;

// FblePolyValueExpr --
//   FBLE_POLY_VALUE_EXPR (arg :: (Kind, Name)) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTaggedKind arg;
  FbleExpr* body;
} FblePolyValueExpr;

// FblePolyApplyExpr --
//   FBLE_POLY_APPLY_EXPR (poly :: Expr) (arg :: Type)
typedef struct {
  FbleExpr _base;
  FbleExpr* poly;
  FbleTypeExpr* arg;
} FblePolyApplyExpr;

// FblePackageTypeExpr --
//   FBLE_PACKAGE_TYPE_EXPR (path :: ModulePath)
typedef struct {
  FbleExpr _base;
  FbleModulePath* path;
} FblePackageTypeExpr;

// FbleAbstractCastExpr --
//   FBLE_ABSTRACT_CAST_EXPR (package :: Type) (target :: Type) (value :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTypeExpr* package;
  FbleTypeExpr* target;
  FbleExpr* value;
} FbleAbstractCastExpr;

// FbleAbstractAccessExpr --
//   FBLE_ABSTRACT_ACCESS_EXPR (value :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* value;
} FbleAbstractAccessExpr;

// FbleListExpr --
//   FBLE_LIST_EXPR (func :: Expr) (args :: [Expr])
typedef struct {
  FbleExpr _base;
  FbleExpr* func;
  FbleExprV args;
} FbleListExpr;

// FbleLiteralExpr --
//   FBLE_LITERAL_EXPR (func :: Expr) (word :: Word)
typedef struct {
  FbleExpr _base;
  FbleExpr* func;
  FbleLoc word_loc;
  const char* word;
} FbleLiteralExpr;

// FbleApplyExpr --
//   FBLE_MISC_APPLY_EXPR (misc :: Expr) (args :: [Expr])
//   FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR (type :: Type) (args :: [Expr])
//   FBLE_FUNC_APPLY_EXPR (func :: Expr) (args :: [Expr])
//
// The 'bind' field is set to true if this application originated from the
// bind syntax. False otherwise.
typedef struct {
  FbleExpr _base;
  FbleExpr* misc;
  FbleExprV args;
  bool bind;
} FbleApplyExpr;

// FbleDataAccessExpr --
//   FBLE_DATA_ACCESS_EXPR (object :: Expr) (field :: Name)
//
// Used for struct and union field access.
typedef struct {
  FbleExpr _base;
  FbleExpr* object;
  FbleName field;
} FbleDataAccessExpr;

// FbleFreeExpr --
//   Free resources associated with an expression.
//
// Inputs:
//   expr - expression to free. May be NULL.
//
// Side effect:
//   Frees resources associated with expr.
void FbleFreeExpr(FbleExpr* expr);

#endif // FBLE_INTERNAL_EXPR_H_