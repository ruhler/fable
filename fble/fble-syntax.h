// fble-syntax.h --
//   Header file for the fble abstract syntax.

#ifndef FBLE_SYNTAX_H_
#define FBLE_SYNTAX_H_

#include <stdbool.h>      // for bool
#include <stdio.h>        // for FILE
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"

// FbleString --
//   A reference counted string.
//
// Note: The checksum field is used internally to detect double frees of
// FbleString, which we have had trouble with in the past.
typedef struct {
  size_t refcount;
  const char* str;
  size_t checksum;
} FbleString;

// FbleNewString --
//   Allocate a new FbleString.
//
// Inputs:
//   arena - arena to use for allocations.
//   str - the contents of the string.
//
// Results:
//   A newly allocated string with a reference count that should be released
//   using FbleReleaseString when no longer needed.
//   Does not take ownership of str - makes a copy instead.
FbleString* FbleNewString(FbleArena* arena, const char* str);

// FbleRetainString -- 
//   Increment the refcount on a string.
//
// Inputs:
//   string - the string to increment the reference count on.
// 
// Results:
//   The input string, for convenience.
//
// Side effects:
//   Increments the reference count on the string. The user should arrange for
//   FbleReleaseString to be called on the string when this reference of it is
//   no longer needed.
FbleString* FbleRetainString(FbleString* string);

// FbleReleaseString -- 
//   Decrement the refcount on a string, freeing resources associated with the
//   string if the refcount goes to zero.
//
// Inputs:
//   arena - arena to use for allocations.
//   string - the string to decrement the reference count on.
//
// Side effects:
//   Decrements the reference count of the string, freeing it and its str
//   contents if the reference count goes to zero.
void FbleReleaseString(FbleArena* arena, FbleString* string);

// FbleLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  FbleString* source;
  int line;
  int col;
} FbleLoc;

// FbleCopyLoc --
//   Make a copy of a location. In particular, increments the reference count
//   on the filename.
//
// Inputs:
//   loc - the loc to copy.
//
// Result:
//   A copy of the loc.
//
// Side effects:
//   Increments the reference count on the filename used in the loc.
FbleLoc FbleCopyLoc(FbleLoc loc);

// FbleFreeLoc --
//   Free resources associated with the given loc.
//
// Inputs:
//   arena - arena to use for allocations
//   loc - the location to free resources of.
//
// Side effects
//   Decrements the refcount on the loc's source filename.
void FbleFreeLoc(FbleArena* arena, FbleLoc loc);

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

// FbleNamespace --
//   Enum used to distinguish among different name spaces.
typedef enum {
  FBLE_NORMAL_NAME_SPACE,
  FBLE_TYPE_NAME_SPACE,
  FBLE_MODULE_NAME_SPACE,
} FbleNameSpace;

// FbleName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FbleNameSpace space;
  FbleLoc loc;
} FbleName;

// FbleFreeName --
//   Free resources associated with a name.
//
// Inputs:
//   arena - arena to use for allocations.
//   name - the name to free resources of.
//
// Side effects:
//   Frees resources associated with the name.
void FbleFreeName(FbleArena* arena, FbleName name);

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
// Kinds are non-cyclcically reference counted. Use FbleKindRetain and
// FbleKindRelease to manage the reference count and memory associated with
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

// FbleKindRetain --
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
//   The returned kind must be freed using FbleKindRelease when no longer in
//   use.
FbleKind* FbleKindRetain(FbleArena* arena, FbleKind* kind);

// FbleKindRelease --
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
void FbleKindRelease(FbleArena* arena, FbleKind* kind);

// FbleExprTag --
//   A tag used to dinstinguish among different kinds of expressions.
typedef enum {
  FBLE_TYPEOF_EXPR,
  FBLE_VAR_EXPR,
  FBLE_LET_EXPR,
  FBLE_MODULE_REF_EXPR,

  FBLE_STRUCT_TYPE_EXPR,
//FBLE_STRUCT_VALUE_EXPLICIT_TYPE_EXPR = FBLE_MISC_APPLY_EXPR,
  FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR,
//FBLE_STRUCT_ACCESS_EXPR = FBLE_MISC_ACCESS_EXPR,

  FBLE_UNION_TYPE_EXPR,
  FBLE_UNION_VALUE_EXPR,
//FBLE_UNION_ACCESS_EXPR = FBLE_MISC_ACCESS_EXPR,
  FBLE_UNION_SELECT_EXPR,

  FBLE_FUNC_TYPE_EXPR,
  FBLE_FUNC_VALUE_EXPR,
//FBLE_FUNC_APPLY_EXPR = FBLE_MISC_APPLY_EXPR,

  FBLE_PROC_TYPE_EXPR,
  FBLE_EVAL_EXPR,
  FBLE_LINK_EXPR,
  FBLE_EXEC_EXPR,

  FBLE_POLY_EXPR,
  FBLE_POLY_APPLY_EXPR,

  FBLE_LIST_EXPR,
  FBLE_LITERAL_EXPR,

  FBLE_MISC_ACCESS_EXPR,  // Used for STRUCT_ACCESS, UNION_ACCESS
  FBLE_MISC_APPLY_EXPR,   // Used for STRUCT_VALUE_EXPLICIT_TYPE, FUNC_APPLY
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

// FbleField --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  FbleTypeExpr* type;
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
  FbleTypeExpr _base;
  FbleFieldV fields;
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
  FbleFieldV fields;
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
  FbleFieldV args;
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

// FbleTypeField --
//   A pair of (Kind, Name) used to describe poly arguments.
typedef struct {
  FbleKind* kind;
  FbleName name;
} FbleTypeField;

// FbleTypeFieldV -- 
//   A vector of FbleTypeFields.
typedef struct {
  FbleTypeField* xs;
  size_t size;
} FbleTypeFieldV;

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

// FbleMiscApplyExpr --
//   FBLE_MISC_APPLY_EXPR (misc :: Expr) (args :: [Expr])
//   FBLE_STRUCT_VALUE_EXPR (type :: Type) (args :: [Expr])
//   FBLE_FUNC_APPLY_EXPR (func :: Expr) (args :: [Expr])
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
typedef struct {
  FbleModuleV modules;
  FbleExpr* main;
} FbleProgram;

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
// TODO:
//   Make this function internal.
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
//
// TODO: Make this function internal?
void FbleFreeExpr(FbleArena* arena, FbleExpr* expr);

// FbleLoad --
//   Load an fble program.
//
// Inputs:
//   arena - The arena to use for allocating the parsed program.
//   filename - The name of the file to parse the program from.
//   root - The directory to search for modules in. May be NULL.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
//   Prints an error message to stderr if the program cannot be parsed.
//
// Allocations:
//   The user should call FbleFreeProgram to free resources associated with
//   the given program when it is no longer needed.
//
// Note:
//   A copy of the filename will be made for use in locations. The user need
//   not ensure that filename remains valid for the duration of the lifetime
//   of the program.
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root);

// FbleFreeProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   arena - arena to use for allocations.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeProgram(FbleArena* arena, FbleProgram* program);

#endif // FBLE_SYNTAX_H_
