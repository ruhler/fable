/**
 * @file expr.h
 *  Defines FbleExpr, untyped abstract syntax for fble.
 */

#ifndef FBLE_INTERNAL_EXPR_H_
#define FBLE_INTERNAL_EXPR_H_

#include <stddef.h>    // for size_t

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
  FBLE_UNDEF_EXPR,

  FBLE_DATA_TYPE_EXPR,    // struct and union types.
  FBLE_DATA_ACCESS_EXPR,  // struct and union field access.

  // FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR = FBLE_MISC_APPLY_EXPR
  FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,
  FBLE_STRUCT_COPY_EXPR,

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
  FBLE_PRIVATE_EXPR,

  FBLE_MISC_APPLY_EXPR,
} FbleExprTag;

/**
 * @struct[FbleExpr] Abstract syntax for an fble expression.
 *  A tagged union of expr types. All exprs have the same initial layout as
 *  FbleExpr. The tag can be used to determine what kind of expr this is to
 *  get access to additional fields of the expr by first casting to that
 *  specific type of expr.
 *
 *  @field[FbleExprTag][tag] The kind of expression.
 *  @field[FbleLoc][loc] Source location for error reporting.
 */
struct FbleExpr {
  FbleExprTag tag;
  FbleLoc loc;
};

/**
 * @struct[FbleExprV] Vector of FbleExpr.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleExpr**][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleExpr** xs;
} FbleExprV;

/** Synonym for FbleExpr when a type is expected. */
typedef FbleExpr FbleTypeExpr;

/** Synonym for FbleExprV when types are expected. */
typedef FbleExprV FbleTypeExprV;

/**
 * @struct[FbleTaggedTypeExpr] Type name pair.
 *  Used for data types and function arguments.
 *
 *  @field[FbleTypeExpr*][type] The type.
 *  @field[FbleName][name] The name.
 */
typedef struct {
  FbleTypeExpr* type;
  FbleName name;
} FbleTaggedTypeExpr;

/**
 * @struct[FbleTaggedTypeExprV] Vector of FbleTaggedTypeExpr.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTaggedTypeExpr*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTaggedTypeExpr* xs;
} FbleTaggedTypeExprV;

/**
 * @struct[FbleDataTypeExpr] FBLE_DATA_TYPE_EXPR
 *  A struct or union type expression. Used for struct_type and union_type
 *  abstract syntax.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleDataTypeTag][datatype] Whether this is struct or union.
 *  @field[FbleTaggedTypeExprV][fields] The fields of the data type.
 */
typedef struct {
  FbleTypeExpr _base;
  FbleDataTypeTag datatype;
  FbleTaggedTypeExprV fields;
} FbleDataTypeExpr;

/**
 * @struct[FbleTaggedExpr] Name expr pair
 *  Used in conditional expressions and anonymous struct values.
 *
 *  @field[FbleName][name] The name.
 *  @field[FbleExpr*][expr] The expression.
 */
typedef struct {
  FbleName name;
  FbleExpr* expr;
} FbleTaggedExpr;

/**
 * @struct[FbleTaggedExprV] Vector of FbleTaggedExpr.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTaggedExpr*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTaggedExpr* xs;
} FbleTaggedExprV;

/**
 * @struct[FbleStructValueImplicitTypeExpr] FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR.
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleTaggedExprV][args] Arguments to the struct value.
 */
typedef struct {
  FbleExpr _base;
  FbleTaggedExprV args;
} FbleStructValueImplicitTypeExpr;

/**
 * @struct[FbleStructCopyExpr] FBLE_STRUCT_COPY_EXPR.
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][src] The struct value to copy from.
 *  @field[FbleTaggedExprV][args] modifications to make during copy.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* src;
  FbleTaggedExprV args;
} FbleStructCopyExpr;

/**
 * @struct[FbleUnionValueExpr] FBLE_UNION_VALUE_EXPR
 *  A union value expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleTypeExpr*][type] The type of the union value to construct.
 *  @field[FbleName][field] The tag of the union value to construct.
 *  @field[FbleExpr*][arg] The argument to the union value to construct.
 */
typedef struct {
  FbleExpr _base;
  FbleTypeExpr* type;
  FbleName field;
  FbleExpr* arg;
} FbleUnionValueExpr;

/**
 * @struct[FbleUnionSelectExpr] FBLE_UNION_SELECT_EXPR
 *  A union select expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][condition] The object to switch on.
 *  @field[FbleTaggedExprV][choices] The branches.
 *  @field[FbleExpr*][default_] Default branch, or NULL if no default.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* condition;
  FbleTaggedExprV choices;
  FbleExpr* default_;
} FbleUnionSelectExpr;

/**
 * @struct[FbleFuncTypeExpr] FBLE_FUNC_TYPE_EXPR
 *  A function type expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleTypeExpr*][arg] Argument type.
 *  @field[FbleTypeExpr*][rtype] Return type.
 */
typedef struct {
  FbleTypeExpr _base;
  FbleTypeExpr* arg;
  FbleTypeExpr* rtype;
} FbleFuncTypeExpr;

/**
 * @struct[FbleFuncValueExpr] FBLE_FUNC_VALUE_EXPR
 *  A function value expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleTaggedTypeExpr][arg] Argument to the function.
 *  @field[FbleExpr*][body] The function body.
 */
typedef struct {
  FbleExpr _base;
  FbleTaggedTypeExpr arg;
  FbleExpr* body;
} FbleFuncValueExpr;

/**
 * @struct[FbleBinding] Info about a variable binding.
 *  Used in let expressions. Exactly one of Kind or Type should be NULL. If
 *  the Kind is NULL, it is inferred from the given Type. If the Type is NULL,
 *  it is inferred from the given Expr.
 *
 *  @field[FbleKind*][kind] The kind of the variable. May be NULL.
 *  @field[FbleTypeExpr*][type] The type of the variable. May be NULL.
 *  @field[FbleName][name] The name of the variable.
 *  @field[FbleExpr*][expr] The value of the variable.
 */
typedef struct {
  FbleKind* kind;
  FbleTypeExpr* type;
  FbleName name;
  FbleExpr* expr;
} FbleBinding;

/**
 * @struct[FbleBindingV] Vector of FbleBinding.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleBinding*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleBinding* xs;
} FbleBindingV;

/**
 * @struct[FbleVarExpr] FBLE_VAR_EXPR
 *  A variable expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleName][var] The name of the variable.
 */
typedef struct {
  FbleExpr _base;
  FbleName var;
} FbleVarExpr;

/**
 * @struct[FbleLetExpr] FBLE_LET_EXPR
 *  A let expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleBindingV][bindings] Variable bindings.
 *  @field[FbleExpr*][body] The body of the let.
 */
typedef struct {
  FbleExpr _base;
  FbleBindingV bindings;
  FbleExpr* body;
} FbleLetExpr;

/**
 * @struct[FbleUndefExpr] FBLE_UNDEF_EXPR
 *  An undef expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleTypeExpr*][type] The variable type.
 *  @field[FbleName][name] The variable name.
 *  @field[FbleExpr*][body] The body of the undef.
 */
typedef struct {
  FbleExpr _base;
  FbleTypeExpr* type;
  FbleName name;
  FbleExpr* body;
} FbleUndefExpr;

/**
 * @struct[FbleModulePathExpr] FBLE_MODULE_PATH_EXPR
 *  A module path expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleModulePath*][path] The module path.
 */
typedef struct {
  FbleExpr _base;
  FbleModulePath* path;
} FbleModulePathExpr;

/**
 * @struct[FbleTypeofExpr] FBLE_TYPEOF_EXPR
 *  A typeof expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][expr] Argument to get the type of.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* expr;
} FbleTypeofExpr;

/**
 * @struct[FbleTaggedKind] A pair of kind and name.
 *  Used to describe poly arguments.
 *
 *  @field[FbleKind*][kind] The kind of the argument.
 *  @field[FbleName][name] The name of the argument.
 */
typedef struct {
  FbleKind* kind;
  FbleName name;
} FbleTaggedKind;

/**
 * @struct[FbleTaggedKindV] Vector of FbleTaggedKind.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTaggedKind*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTaggedKind* xs;
} FbleTaggedKindV;

/**
 * @struct[FblePolyValueExpr] FBLE_POLY_VALUE_EXPR
 *  A poly value expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleTaggedKind][arg] Argument to the poly.
 *  @field[FbleExpr*][body] The body of the poly.
 */
typedef struct {
  FbleExpr _base;
  FbleTaggedKind arg;
  FbleExpr* body;
} FblePolyValueExpr;

/**
 * @struct[FblePolyApplyExpr] FBLE_POLY_APPLY_EXPR
 *  A poly application expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][poly] The poly to apply.
 *  @field[FbleTypeExpr*][arg] The args to the poly.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* poly;
  FbleTypeExpr* arg;
} FblePolyApplyExpr;

/**
 * @struct[FblePackageTypeExpr] FBLE_PACKAGE_TYPE_EXPR
 *  A package type expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleModulePath*][path] The package path.
 */
typedef struct {
  FbleTypeExpr _base;
  FbleModulePath* path;
} FblePackageTypeExpr;

/**
 * @struct[FblePrivateExpr] FBLE_PRIVATE_EXPR
 *  A private type or private value expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][arg] The value or type to make private.
 *  @field[FbleTypeExpr*][package] The package type.
 *
 */
typedef struct {
  FbleTypeExpr _base;
  FbleExpr* arg;
  FbleTypeExpr* package;
} FblePrivateExpr;

/**
 * @struct[FbleListExpr] FBLE_LIST_EXPR
 *  A list expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][func] The function to apply.
 *  @field[FbleExprV][args] Elements of the list.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* func;
  FbleExprV args;
} FbleListExpr;

/**
 * @struct[FbleLiteralExpr] FBLE_LITERAL_EXPR
 *  A literal expression.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][func] The function to apply.
 *  @field[FbleLoc][word_loc]
 *   Location of the literal word for error reporting.
 *  @field[const char*][word] The literal word.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* func;
  FbleLoc word_loc;
  const char* word;
} FbleLiteralExpr;

/**
 * @struct[FbleApplyExpr] FBLE_MISC_APPLY_EXPR
 *  Application expressions. Used for:
 *
 *  @i FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR
 *  @i FBLE_FUNC_APPLY_EXPR
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][misc] The function or type to apply.
 *  @field[FbleExprV][args] The arguments to the function/type.
 *  @field[bool][bind] True if this is from bind syntax, false otherwise.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* misc;
  FbleExprV args;
  bool bind;
} FbleApplyExpr;

/**
 * @struct[FbleDataAccessExpr] FBLE_DATA_ACCESS_EXPR
 *  A struct or union field access expression. Used for struct_access and
 *  union_access.
 *
 *  @field[FbleTypeExpr][_base] FbleExpr base class.
 *  @field[FbleExpr*][object] The struct or union value to access.
 *  @field[FbleName][field] The field to access.
 */
typedef struct {
  FbleExpr _base;
  FbleExpr* object;
  FbleName field;
} FbleDataAccessExpr;

/**
 * @func[FbleFreeExpr] Frees resources associated with an expression.
 *  @arg[FbleExpr*][expr] Expression to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with expr.
 */
void FbleFreeExpr(FbleExpr* expr);

#endif // FBLE_INTERNAL_EXPR_H_
