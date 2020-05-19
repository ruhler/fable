// syntax.h --
//   Header file for the fble abstract syntax.

#ifndef FBLE_INTERNAL_SYNTAX_H_
#define FBLE_INTERNAL_SYNTAX_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"
#include "fble-name.h"

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
void FbleReportWarning(const char* format, FbleLoc* loc, ...);

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
bool FbleNamesEqual(FbleName* a, FbleName* b);

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
void FblePrintName(FILE* stream, FbleName* name);

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
// Kinds are non-cyclcically reference counted. Use FbleRetainKind and
// FbleReleaseKind to manage the reference count and memory associated with
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

// FbleRetainKind --
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
//   The returned kind must be freed using FbleReleaseKind when no longer in
//   use.
FbleKind* FbleRetainKind(FbleArena* arena, FbleKind* kind);

// FbleReleaseKind --
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
void FbleReleaseKind(FbleArena* arena, FbleKind* kind);

// FbleExprTag --
//   A tag used to dinstinguish among different kinds of expressions.
typedef enum {
  FBLE_TYPEOF_EXPR,
  FBLE_VAR_EXPR,
  FBLE_LET_EXPR,
  FBLE_MODULE_REF_EXPR,

  FBLE_STRUCT_TYPE_EXPR,
  FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR,
  FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,
  FBLE_STRUCT_ACCESS_EXPR,

  FBLE_UNION_TYPE_EXPR,
  FBLE_UNION_VALUE_EXPR,
  FBLE_UNION_ACCESS_EXPR,
  FBLE_UNION_SELECT_EXPR,

  FBLE_FUNC_TYPE_EXPR,
  FBLE_FUNC_VALUE_EXPR,
  FBLE_FUNC_APPLY_EXPR,

  FBLE_PROC_TYPE_EXPR,
  FBLE_EVAL_EXPR,
  FBLE_LINK_EXPR,
  FBLE_EXEC_EXPR,

  FBLE_POLY_EXPR,
  FBLE_POLY_APPLY_EXPR,

  FBLE_LIST_EXPR,
  FBLE_LITERAL_EXPR,

  FBLE_MISC_ACCESS_EXPR,
  FBLE_MISC_APPLY_EXPR,
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

// FbleStructTypeExpr --
//   FBLE_STRUCT_TYPE_EXPR (fields :: [(Type, Name)])
typedef struct {
  FbleTypeExpr _base;
  FbleTaggedTypeExprV fields;
} FbleStructTypeExpr;

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

// FbleUnionTypeExpr --
//   FBLE_UNION_TYPE_EXPR (fields :: [(Type, Name)])
typedef struct {
  FbleTypeExpr _base;
  FbleTaggedTypeExprV fields;
} FbleUnionTypeExpr;

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
// Note: default_ is NULL if no default is provided.
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

// FblePolyExpr --
//   FBLE_POLY_EXPR (arg :: (Kind, Name)) (body :: Expr)
typedef struct {
  FbleExpr _base;
  FbleTaggedKind arg;
  FbleExpr* body;
} FblePolyExpr;

// FblePolyApplyExpr --
//   FBLE_POLY_APPLY_EXPR (poly :: Expr) (arg :: Type)
typedef struct {
  FbleExpr _base;
  FbleExpr* poly;
  FbleTypeExpr* arg;
} FblePolyApplyExpr;

// FbleListExpr --
//   FBLE_LIST_EXPR (args :: [Expr])
typedef struct {
  FbleExpr _base;
  FbleExprV args;
} FbleListExpr;

// FbleLiteralExpr --
//   FBLE_LITERAL_EXPR (spec :: Expr) (word :: Word)
typedef struct {
  FbleExpr _base;
  FbleExpr* spec;
  FbleLoc word_loc;
  const char* word;
} FbleLiteralExpr;

// FbleApplyExpr --
//   FBLE_MISC_APPLY_EXPR (misc :: Expr) (args :: [Expr])
//   FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR (type :: Type) (args :: [Expr])
//   FBLE_FUNC_APPLY_EXPR (func :: Expr) (args :: [Expr])
//
// FBLE_MISC_APPLY_EXPR is resolved to FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR or
// FBLE_FUNC_APPLY_EXPR as part of type check.
typedef struct {
  FbleExpr _base;
  FbleExpr* misc;
  FbleExprV args;
} FbleApplyExpr;

// FbleAccessExpr --
//   FBLE_MISC_ACCESS_EXPR (object :: Expr) (field :: Name)
//   FBLE_STRUCT_ACCESS_EXPR (object :: Expr) (field :: Name)
//   FBLE_UNION_ACCESS_EXPR (object :: Expr) (field :: Name)
//
// Common form used for both struct and union access.
// FBLE_MISC_ACCESS_EXPR is resolved to FBLE_STRUCT_ACCESS_EXPR or
// FBLE_UNION_ACCESS_EXPR as part of type check.
typedef struct {
  FbleExpr _base;
  FbleExpr* object;
  FbleName field;
} FbleAccessExpr;

// FbleModule --
//   Represents an individual module.
// 
// Fields:
//   filename - the filename of the module. Used for ownership of all loc
//              references in the module value.
//   name - the canonical name of the module. This is the resolved path to the
//          module with "/" used as a separator. For example, the module Foo/Bar% has
//          name "Foo/Bar" in the MODULE name space.
//   value - the value of the module
typedef struct {
  FbleName name;
  FbleExpr* value;
} FbleModule;

// FbleModuleV -- A vector of modules
typedef struct {
  size_t size;
  FbleModule* xs;
} FbleModuleV;

// FbleProgram --
//   Represents a complete parsed and loaded fble program.
//
// Fields:
//   modules - List of dependant modules in topological dependancy order. Later
//             modules in the list may depend on earlier modules in the list,
//             but not the other way around.
//   filename - The filename of the main program. Used for ownership of all
//              references to the filename in locs in main.
//   main - The value of the program, which may depend on any of the modules.
struct FbleProgram {
  FbleModuleV modules;
  FbleExpr* main;
};

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
