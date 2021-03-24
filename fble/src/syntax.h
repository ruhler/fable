// syntax.h --
//   Header file for the fble abstract syntax.

#ifndef FBLE_INTERNAL_SYNTAX_H_
#define FBLE_INTERNAL_SYNTAX_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"
#include "fble-name.h"
#include "fble-syntax.h"

// FbleReportWarning --
//   Report a warning message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the warning message.
//   loc - The location of the warning message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints a warning message to stderr with error location.
void FbleReportWarning(const char* format, FbleLoc loc, ...);

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
void FbleReportError(const char* format, FbleLoc loc, ...);

// FbleNameV --
//   A vector of FbleNames.
typedef struct {
  size_t size;
  FbleName* xs;
} FbleNameV;

// FbleNamesEqual --
//   Test whether two names are equal. Two names are considered equal if they
//   have the same name and belong to the same namespace. Location is not
//   relevant for this check.
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
bool FbleNamesEqual(FbleName a, FbleName b);

// FblePrintName --
//   Print a name in human readable form to the given stream.
//
// Inputs:
//   stream - the stream to print to
//   name - the name to print
//
// Results:
//   none.
//
// Side effects:
//   Prints the given name to the given stream.
void FblePrintName(FILE* stream, FbleName name);

// FbleModuleRef --
//
// Fields:
//   resolved: After the module reference is resolved, 'resolved' will be set
//             to the name of the canonical name of the resolved module.
typedef struct {
  FbleNameV path;
  FbleName resolved;
} FbleModuleRef;

// FbleModuleRefV -- A vector of FbleModuleRef.
typedef struct {
  size_t size;
  FbleModuleRef** xs;
} FbleModuleRefV;

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
//
// Kinds are non-cyclcically reference counted. Use FbleCopyKind and
// FbleFreeKind to manage the reference count and memory associated with
// the kind.
typedef struct {
  FbleKindTag tag;
  FbleLoc loc;
  int refcount;
} FbleKind;

// FbleKindV --
//   A vector of FbleKind.
typedef struct {
  size_t size;
  FbleKind** xs;
} FbleKindV;

// FbleBasicKind --
//   FBLE_BASIC_KIND (level :: size_t)
//
// levels
//   0: A normal, non-type value.
//   1: A normal type. A type of a level 0.
//   2: A type of a type of a value.
//   3: A type of a type of a type of a value.
//   etc.
typedef struct {
  FbleKind _base;
  size_t level;
} FbleBasicKind;

// FblePolyKind --
//   FBLE_POLY_KIND (arg :: Kind) (return :: Kind)
typedef struct {
  FbleKind _base;
  FbleKind* arg;
  FbleKind* rkind;
} FblePolyKind;

// FbleCopyKind --
//   Makes a (refcount) copy of a kind.
//
// Inputs:
//   arena - for allocations.
//   kind - the kind to copy.
//
// Results:
//   The copied kind.
//
// Side effects:
//   The returned kind must be freed using FbleFreeKind when no longer in
//   use.
FbleKind* FbleCopyKind(FbleArena* arena, FbleKind* kind);

// FbleFreeKind --
//   Frees a (refcount) copy of a compiled kind.
//
// Inputs:
//   arena - for deallocations.
//   kind - the kind to free. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the refcount for the kind and frees it if there are no
//   more references to it.
void FbleFreeKind(FbleArena* arena, FbleKind* kind);

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

  FBLE_PROC_TYPE_EXPR,
  FBLE_EVAL_EXPR,
  FBLE_LINK_EXPR,
  FBLE_EXEC_EXPR,

  FBLE_POLY_VALUE_EXPR,
  FBLE_POLY_APPLY_EXPR,

  FBLE_LIST_EXPR,
  FBLE_LITERAL_EXPR,

  FBLE_MODULE_REF_EXPR,

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

// FbleDataTypeTag --
//   Enum used to distinguish between different kinds of data types.
typedef enum {
  FBLE_STRUCT_DATATYPE,
  FBLE_UNION_DATATYPE
} FbleDataTypeTag;

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

// FbleProcTypeExpr --
//   FBLE_PROC_TYPE_EXPR (type :: Type)
typedef struct {
  FbleTypeExpr _base;
  FbleTypeExpr* type;
} FbleProcTypeExpr;

// FbleEvalExpr --
//   FBLE_EVAL_EXPR (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleExpr* body;
} FbleEvalExpr;

// FbleLinkExpr --
//   FBLE_LINK_EXPR (type :: Type) (get :: Name) (put :: Name) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTypeExpr* type;
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

// FbleModuleRefExpr --
//   FBLE_MODULE_REF_EXPR (ref :: ModuleRef)
typedef struct {
  FbleExpr _base;
  FbleModuleRef ref;
} FbleModuleRefExpr;

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
typedef struct {
  FbleExpr _base;
  FbleExpr* misc;
  FbleExprV args;
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


// FbleParse --
//   Parse an expression from a file.
//
// Inputs:
//   arena - The arena to use for allocating the parsed program.
//   filename - The name of the file to parse the program from.
//   module_refs - Output param: A list of the module references in the parsed
//                 expression.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
//   Prints an error message to stderr if the program cannot be parsed.
//   Appends module references in the parsed expression to module_refs, which
//   is assumed to be a pre-initialized vector.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned program if there is no error.
//
// Note:
//   The user should ensure that filename remains valid for the duration of
//   the lifetime of the program, because it is used in locations of the
//   returned expression without additional reference counts.
FbleExpr* FbleParse(FbleArena* arena, FbleString* filename, FbleModuleRefV* module_refs);

// FbleFreeExpr --
//   Free resources associated with an expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   expr - expression to free. May be NULL.
//
// Side effect:
//   Frees resources associated with expr.
void FbleFreeExpr(FbleArena* arena, FbleExpr* expr);

#endif // FBLE_INTERNAL_SYNTAX_H_