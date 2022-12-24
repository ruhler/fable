/**
 * @file expr.h
 * Defines FbleExpr, untyped abstract syntax for fble.
 */

#ifndef FBLE_INTERNAL_EXPR_H_
#define FBLE_INTERNAL_EXPR_H_

#include <sys/types.h>    // for size_t

#include <fble/fble-load.h>    // for public typedef of FbleExpr.
#include <fble/fble-name.h>

#include "kind.h"

/**
 * Different kinds of FbleExpr.
 */
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
  // FBLE_ABSTRACT_VALUE_EXPR = FBLE_MISC_APPLY_EXPR

  FBLE_MISC_APPLY_EXPR,
} FbleExprTag;

/**
 * Abstract syntax for an fble expression.
 *
 * A tagged union of expr types. All exprs have the same initial layout as
 * FbleExpr. The tag can be used to determine what kind of expr this is to get
 * access to additional fields of the expr by first casting to that specific
 * type of expr.
 */
struct FbleExpr {
  FbleExprTag tag;  /**< The kind of expression. */
  FbleLoc loc;      /**< Source location for error reporting. */
};

/** Vector of FbleExpr. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleExpr** xs;    /**< The elements. */
} FbleExprV;

/** Synonym for FbleExpr when a type is expected. */
typedef FbleExpr FbleTypeExpr;

/** Synonym for FbleExprV when types are expected. */
typedef FbleExprV FbleTypeExprV;

/**
 * Type name pair used for data types and function arguments.
 */
typedef struct {
  FbleTypeExpr* type;   /**< The type. */
  FbleName name;        /**< The name. */
} FbleTaggedTypeExpr;

/** Vector of FbleTaggedTypeExpr. */
typedef struct {
  size_t size;              /**< Number of elements. */
  FbleTaggedTypeExpr* xs;   /**< The elements. */
} FbleTaggedTypeExprV;

/**
 * FBLE_DATA_TYPE_EXPR: A struct or union type expression.
 *
 * Used for struct_type and union_type abstract syntax.
 */
typedef struct {
  FbleTypeExpr _base;           /**< FbleExpr base class. */
  FbleDataTypeTag datatype;     /**< Whether this is struct or union. */
  FbleTaggedTypeExprV fields;   /**< The fields of the data type. */
} FbleDataTypeExpr;

/**
 * Name expr pair used in conditional expressions and anonymous struct values.
 */
typedef struct {
  FbleName name;    /**< The name. */
  FbleExpr* expr;   /**< The expression. */
} FbleTaggedExpr;

/** Vector of FbleTaggedExpr. */
typedef struct {
  size_t size;          /**< Number of elements. */
  FbleTaggedExpr* xs;   /**< The elements. */
} FbleTaggedExprV;

/**
 * FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR.
 */
typedef struct {
  FbleExpr _base;         /**< FbleExpr base class. */
  FbleTaggedExprV args;   /**< Arguments to the struct value. */
} FbleStructValueImplicitTypeExpr;

/**
 * FBLE_UNION_VALUE_EXPR: A union value expression.
 */
typedef struct {
  FbleExpr _base;       /**< FbleExpr base class. */
  FbleTypeExpr* type;   /**< The type of the union value to construct. */
  FbleName field;       /**< The tag of the union value to construct. */
  FbleExpr* arg;        /**< The argument to the union value to construct. */
} FbleUnionValueExpr;

/**
 * FBLE_UNION_SELECT_EXPR: A union select expression.
 */
typedef struct {
  FbleExpr _base;           /**< FbleExpr base class. */
  FbleExpr* condition;      /**< The object to switch on. */
  FbleTaggedExprV choices;  /**< The branches. */
  FbleExpr* default_;       /**< Default branch, or NULL if no default. */
} FbleUnionSelectExpr;

/**
 * FBLE_FUNC_TYPE_EXPR: A function type expression.
 */
typedef struct {
  FbleTypeExpr _base;     /**< FbleExpr base class. */
  FbleTypeExprV args;     /**< Argument types. */
  FbleTypeExpr* rtype;    /**< Return type. */
} FbleFuncTypeExpr;

/**
 * FBLE_FUNC_VALUE_EXPR: A function value expression.
 */
typedef struct {
  FbleExpr _base;             /**< FbleExpr base class. */
  FbleTaggedTypeExprV args;   /**< Arguments to the function. */
  FbleExpr* body;             /**< The function body. */
} FbleFuncValueExpr;

/**
 * Info about a variable binding.
 *
 * Used in let expressions. Exactly one of Kind or Type should be NULL. If the
 * Kind is NULL, it is inferred from the given Type. If the Type is NULL, it
 * is inferred from the given Expr.
 */
typedef struct {
  FbleKind* kind;       /**< The kind of the variable. May be NULL. */
  FbleTypeExpr* type;   /**< The type of the variable. May be NULL. */
  FbleName name;        /**< The name of the variable. */
  FbleExpr* expr;       /**< The value of the variable. */
} FbleBinding;

/** Vector of FbleBinding. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleBinding* xs;  /**< The elements. */
} FbleBindingV;

/**
 * FBLE_VAR_EXPR: A variable expression.
 */
typedef struct {
  FbleExpr _base;   /**< FbleExpr base class. */
  FbleName var;     /**< The name of the variable. */
} FbleVarExpr;

/**
 * FBLE_LET_EXPR: A let expression.
 */
typedef struct {
  FbleExpr _base;         /**< FbleExpr base class. */
  FbleBindingV bindings;  /**< Variable bindings. */
  FbleExpr* body;         /**< The body of the let. */
} FbleLetExpr;

/**
 * FBLE_MODULE_PATH_EXPR: A module path expression.
 */
typedef struct {
  FbleExpr _base;         /**< FbleExpr base class. */
  FbleModulePath* path;   /**< The module path. */
} FbleModulePathExpr;

/**
 * FBLE_TYPEOF_EXPR: A typeof expression.
 */
typedef struct {
  FbleExpr _base;   /**< FbleExpr base class. */
  FbleExpr* expr;   /**< Argument to get the type of. */
} FbleTypeofExpr;

/**
 * A pair of kind and named used to describe poly arguments.
 */
typedef struct {
  FbleKind* kind;   /**< The kind of the argument. */
  FbleName name;    /**< The name of the argument. */
} FbleTaggedKind;

/** Vector of FbleTaggedKind. */
typedef struct {
  FbleTaggedKind* xs;   /**< Number of elements. */
  size_t size;          /**< The elements. */
} FbleTaggedKindV;

/**
 * FBLE_POLY_VALUE_EXPR: A poly value expression.
 */
typedef struct {
  FbleExpr _base;       /**< FbleExpr base class. */
  FbleTaggedKind arg;   /**< Arguments to the poly. */
  FbleExpr* body;       /**< The body of the poly. */
} FblePolyValueExpr;

/**
 * FBLE_POLY_APPLY_EXPR: A poly application expression.
 *
 * Also used for FBLE_ABSTRACT_TYPE_EXPR.
 */
typedef struct {
  FbleExpr _base;     /**< FbleExpr base class. */
  FbleExpr* poly;     /**< The poly to apply. */
  FbleTypeExpr* arg;  /**< The args to the poly. */
} FblePolyApplyExpr;

/**
 * FBLE_PACKAGE_TYPE_EXPR: A package type expression.
 */
typedef struct {
  FbleTypeExpr _base;     /**< FbleExpr base class. */
  FbleModulePath* path;   /**< The package path. */
} FblePackageTypeExpr;

/**
 * FBLE_ABSTRACT_CAST_EXPR: An abstract cast expression.
 */
typedef struct {
  FbleExpr _base;         /**< FbleExpr base class. */
  FbleTypeExpr* package;  /**< The package type. */
  FbleTypeExpr* target;   /**< The target type. */
  FbleExpr* value;        /**< The value to cast. */
} FbleAbstractCastExpr;

/**
 * FBLE_ABSTRACT_ACCESS_EXPR: An abstract access expression.
 */
typedef struct {
  FbleExpr _base;     /**< FbleExpr base class. */
  FbleExpr* value;    /**< The abstract value to access. */
} FbleAbstractAccessExpr;

/**
 * FBLE_LIST_EXPR: A list expression.
 */
typedef struct {
  FbleExpr _base;   /**< FbleExpr base class. */
  FbleExpr* func;   /**< The function to apply. */
  FbleExprV args;   /**< Elements of the list. */
} FbleListExpr;

/**
 * FBLE_LITERAL_EXPR: A literal expression.
 */
typedef struct {
  FbleExpr _base;     /**< FbleExpr base class. */
  FbleExpr* func;     /**< The function to apply. */
  FbleLoc word_loc;   /**< Location of the literal word for error reporting. */
  const char* word;   /**< The literal word. */
} FbleLiteralExpr;

/**
 * FBLE_MISC_APPLY_EXPR: Application expressions.
 *
 * Used for:
 * * FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR
 * * FBLE_FUNC_APPLY_EXPR
 * * FBLE_ABSTRACT_VALUE_EXPR
 */
typedef struct {
  FbleExpr _base;   /**< FbleExpr base class. */
  FbleExpr* misc;   /**< The function or type to apply. */
  FbleExprV args;   /**< The arguments to the function/type. */
  bool bind;        /**< True if this is from bind syntax, false otherwise. */
} FbleApplyExpr;

/**
 * FBLE_DATA_ACCESS_EXPR: A struct or union field access expression.
 *
 * Used for struct_access and union_access.
 */
typedef struct {
  FbleExpr _base;     /**< FbleExpr base class. */
  FbleExpr* object;   /**< The struct or union value to access. */
  FbleName field;     /**< The field to access. */
} FbleDataAccessExpr;

/**
 * Frees resources associated with an expression.
 *
 * @param expr  Expression to free. May be NULL.
 *
 * @sideeffects
 *   Frees resources associated with expr.
 */
void FbleFreeExpr(FbleExpr* expr);

#endif // FBLE_INTERNAL_EXPR_H_
