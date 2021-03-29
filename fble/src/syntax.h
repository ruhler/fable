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


// FbleNewModulePath --
//   Allocate a new, empty module path.
//
// Inputs:
//   arena - the arena to use for allocations.
//   loc - the location of the module path. Borrowed.
//
// Results:
//   A newly allocated empty module path.
//
// Side effects:
//   Allocates a new module path that the user should free with
//   FbleFreeModulePath when no longer needed.
FbleModulePath* FbleNewModulePath(FbleArena* arena, FbleLoc loc);

// FbleModulePathName --
//   Construct an FbleName describing a module path.
//
// Inputs:
//   arena - arena to use for allocations.
//   path - the path to construct an FbleName for.
//
// Results:
//   An FbleName describing the full module path. For example: "/Foo/Bar%".
//
// Side effects:
//   The caller should call FbleFreeName on the returned name when no longer
//   needed.
FbleName FbleModulePathName(FbleArena* arena, FbleModulePath* path);

// FblePrintModulePath --
//   Print a module path in human readable form to the given stream.
//
// Inputs:
//   stream - the stream to print to
//   path - the path to print
//
// Results:
//   none.
//
// Side effects:
//   Prints the given path to the given stream.
void FblePrintModulePath(FILE* stream, FbleModulePath* path);

// FbleModulePathsEqual --
//   Test whether two paths are equal. Two paths are considered equal if they
//   have the same sequence of module names. Locations are not relevant for
//   this check.
//
// Inputs:
//   a - The first path.
//   b - The second path.
//
// Results:
//   true if the first path equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbleModulePathsEqual(FbleModulePath* a, FbleModulePath* b);

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

  FBLE_MODULE_PATH_EXPR,

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
//   deps - Output param: A list of the modules that the parsed expression
//          references. Modules will appear at most once in the list.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr if the program cannot be parsed.
// * Appends module paths in the parsed expression to deps, which
//   is assumed to be a pre-initialized vector. The caller is responsible for
//   calling FbleFreeModulePath on each path when it is no longer needed.
FbleExpr* FbleParse(FbleArena* arena, FbleString* filename, FbleModulePathV* deps);

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
