
#ifndef FBLE_TYPE_H_
#define FBLE_TYPE_H_

#include "fble-syntax.h"
#include "ref.h"

// TypeTag --
//   A tag used to dinstinguish among different kinds of compiled types.
typedef enum {
  STRUCT_TYPE,
  UNION_TYPE,
  FUNC_TYPE,
  PROC_TYPE,
  POLY_TYPE,
  POLY_APPLY_TYPE,
  VAR_TYPE,
  TYPE_TYPE,
} TypeTag;

// Type --
//   A tagged union of type types. All types have the same initial
//   layout as Type. The tag can be used to determine what kind of
//   type this is to get access to additional fields of the type
//   by first casting to that specific type of type.
//
//   The evaluating field is set to true if the type is currently being
//   evaluated. See the Eval function for more info.
//
// Design notes on types:
// * Instances of Type represent both unevaluated and evaluated versions of
//   the type. We use the unevaluated versions of the type when printing error
//   messages and as a stable reference to a type before and after evaluation.
// * Cycles are allowed in the Type data structure, to represent recursive
//   types. Every cycle is guaranteed to go through a Var type.
// * Types are evaluated as they are constructed.
// * TYPE_TYPE is handled specially: we propagate TYPE_TYPE up to the top of
//   the type during construction rather than save the unevaluated version of
//   a typeof.
typedef struct Type {
  FbleRef ref;
  TypeTag tag;
  FbleLoc loc;
  bool evaluating;
} Type;

// TypeV --
//   A vector of Type.
typedef struct {
  size_t size;
  Type** xs;
} TypeV;

// Field --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  Type* type;
  FbleName name;
} Field;

// FieldV --
//   A vector of Field.
typedef struct {
  size_t size;
  Field* xs;
} FieldV;

// StructType -- STRUCT_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} StructType;

// UnionType -- UNION_TYPE
typedef struct {
  Type _base;
  FieldV fields;
} UnionType;

// FuncType -- FUNC_TYPE
typedef struct {
  Type _base;
  Type* arg;
  struct Type* rtype;
} FuncType;

// ProcType -- PROC_TYPE
typedef struct {
  Type _base;
  Type* type;
} ProcType;

// PolyType -- POLY_TYPE
//
// We maintain an invariant when constructing PolyTypes that the body is not a
// TYPE_TYPE. For example: \a -> typeof(a) is constructed as typeof(\a -> a)
typedef struct {
  Type _base;
  Type* arg;
  Type* body;
} PolyType;

// PolyApplyType -- POLY_APPLY_TYPE
//   The 'result' field is the result of evaluating the poly apply type, or
//   NULL if it has not yet been evaluated.
//
// We maintain an invariant when constructing PolyApplyTypes that the poly is
// not a TYPE_TYPE. For example: (typeof(f) x) is constructed as typeof(f x).
typedef struct {
  Type _base;
  Type* poly;
  Type* arg;
  Type* result;
} PolyApplyType;

// VarType --
//   VAR_TYPE
//
// A variable type whose value may or may not be known. Used for the value of
// type paramaters and recursive type values.
typedef struct VarType {
  Type _base;
  FbleKind* kind;
  FbleName name;
  Type* value;
} VarType;

// VarTypeV - a vector of var types.
typedef struct {
  size_t size;
  VarType** xs;
} VarTypeV;

// TypeType --
//   TYPE_TYPE
//
// The type of a type.
typedef struct TypeType {
  Type _base;
  Type* type;
} TypeType;

// FbleGetKind --
//   Get the kind of the given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to get the kind of.
//
// Results:
//   The kind of the type.
//
// Side effects:
//   The caller is responsible for calling FbleKindRelease on the returned
//   type when it is no longer needed.
FbleKind* FbleGetKind(FbleArena* arena, Type* type);

// FbleGetKindLevel --
//   Returns the level of the fully applied version of this kind.
//
// Inputs:
//   kind - the kind to get the fully applied level of.
//
// Results:
//   The level of the kind after it has been fully applied.
//
// Side effects:
//   None.
size_t FbleGetKindLevel(FbleKind* kind);

// FbleKindsEqual --
//   Test whether the two given compiled kinds are equal.
//
// Inputs:
//   a - the first kind
//   b - the second kind
//
// Results:
//   True if the first kind equals the second kind, false otherwise.
//
// Side effects:
//   None.
bool FbleKindsEqual(FbleKind* a, FbleKind* b);

// FblePrintKind --
//   Print the given compiled kind in human readable form to stderr.
//
// Inputs:
//   kind - the kind to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given kind in human readable form to stderr.
void FblePrintKind(FbleKind* type);

typedef FbleRefArena FbleTypeArena;

// FbleNewTypeArena --
//   Creates a new type arena backed by the given arena.
//
// Inputs:
//   arena - the arena to back the type arena.
//
// Results:
//   A newly allocated type arena.
//
// Side effects:
//   Allocates a new type arena that should be freed with FbleFreeTypeArena
//   when no longer in use.
FbleTypeArena* FbleNewTypeArena(FbleArena* arena);

// FbleFreeTypeArena --
//   Frees resources associated with the given type arena.
//
// Inputs:
//   arena - the arena to free
//
// Results:
//   none.
//
// Side effects:
//   Frees resources associated with the given type arena. The type arena must
//   not be accessed after this call.
void FbleFreeTypeArena(FbleTypeArena* arena);

// FbleTypeRetain --
//   Takes a reference to a compiled type.
//
// Inputs:
//   arena - the arena the type was allocated with.
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented strong reference count.
//
// Side effects:
//   The returned type must be freed using FbleTypeRelease when no longer in
//   use.
Type* FbleTypeRetain(FbleTypeArena* arena, Type* type);

// FbleTypeRelease --
//   Drop a reference to a compiled type.
//
// Inputs:
//   arena - for deallocations.
//   type - the type to drop the refcount for. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the strong refcount for the type and frees it if there are no
//   more references to it.
void FbleTypeRelease(FbleTypeArena* arena, Type* type);

// FbleNewPolyType --
//   Construct a PolyType. Maintains the invariant that poly of a typeof
//   should be constructed as a typeof a poly.
//
// Inputs:
//   arena - the arena to use for allocations.
//   loc - the location for the type.
//   arg - the poly arg.
//   body - the poly body.
//
// Results:
//   A type representing the poly type: \arg -> body.
//
// Side effects:
//   The caller is responsible for calling FbleTypeRelease on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed arg or body types.
Type* FbleNewPolyType(FbleTypeArena* arena, FbleLoc loc, Type* arg, Type* body);

// FbleNewPolyApplyType --
//   Construct a PolyApplyType. Maintains the invariant that poly apply of a
//   typeof should be constructed as a typeof a poly apply.
//
// Inputs:
//   arena - the arena to use for allocations.
//   loc - the location for the type.
//   poly - the poly apply poly.
//   arg - the poly apply arg.
//
// Results:
//   An unevaluated type representing the poly apply type: poly<arg>
//
// Side effects:
//   The caller is responsible for calling FbleTypeRelease on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed poly or arg types.
Type* FbleNewPolyApplyType(FbleTypeArena* arena, FbleLoc loc, Type* poly, Type* arg);

// FbleNormalType --
//   Reduce an evaluated type to normal form. Normal form types are struct,
//   union, and func types, but not var types, for example.
//
// Inputs:
//   type - the type to reduce.
//
// Results:
//   The type reduced to normal form.
//
// Side effects:
//   The result is only valid for as long as the input type is retained. It is
//   the callers responsibility to take a references to the return typed if
//   they want it to live longer than the given input type.
Type* FbleNormalType(Type* type);

// FbleEvalType --
//   Evaluate the given type in place. After evaluation there are no more
//   unevaluated poly apply types that can be applied.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type to evaluate. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   The type is evaluated in place.
void FbleEvalType(FbleTypeArena* arena, Type* type);

// FbleTypesEqual --
//   Test whether the two given evaluated types are equal.
//
// Inputs:
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
bool FbleTypesEqual(Type* a, Type* b);

// FblePrintType --
//   Print the given compiled type in human readable form to stderr.
//
// Inputs:
//   arena - arena to use for internal allocations.
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
void FblePrintType(FbleArena* arena, Type* type);

#endif // FBLE_TYPE_H_
