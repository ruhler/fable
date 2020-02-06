
#ifndef FBLE_TYPE_H_
#define FBLE_TYPE_H_

#include "fble-syntax.h"
#include "ref.h"

// FbleTypeTag --
//   A tag used to dinstinguish among different kinds of compiled types.
typedef enum {
  FBLE_STRUCT_TYPE,
  FBLE_UNION_TYPE,
  FBLE_FUNC_TYPE,
  FBLE_PROC_TYPE,
  FBLE_POLY_TYPE,
  FBLE_POLY_APPLY_TYPE,
  FBLE_VAR_TYPE,
  FBLE_TYPE_TYPE,
} FbleTypeTag;

// FbleType --
//   A tagged union of type types. All types have the same initial
//   layout as FbleType. The tag can be used to determine what kind of
//   type this is to get access to additional fields of the type
//   by first casting to that specific type of type.
typedef struct FbleType {
  FbleRef ref;
  FbleTypeTag tag;
  FbleLoc loc;
} FbleType;

// FbleTypeV --
//   A vector of Type.
typedef struct {
  size_t size;
  FbleType** xs;
} FbleTypeV;

// FbleTaggedType --
//   A pair of (Type, Name) used to describe type and function arguments.
typedef struct {
  FbleType* type;
  FbleName name;
} FbleTaggedType;

// FbleTaggedTypeV --
//   A vector of FbleTaggedType.
typedef struct {
  size_t size;
  FbleTaggedType* xs;
} FbleTaggedTypeV;

// FbleStructType -- FBLE_STRUCT_TYPE
typedef struct {
  FbleType _base;
  FbleTaggedTypeV fields;
} FbleStructType;

// FbleUnionType -- FBLE_UNION_TYPE
typedef struct {
  FbleType _base;
  FbleTaggedTypeV fields;
} FbleUnionType;

// FbleFuncType -- FBLE_FUNC_TYPE
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleType* rtype;
} FbleFuncType;

// FbleProcType -- FBLE_PROC_TYPE
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleProcType;

// FblePolyType -- FBLE_POLY_TYPE
//
// We maintain an invariant when constructing FblePolyTypes that the body is
// not an FBLE_TYPE_TYPE. For example: \a -> typeof(a) is constructed as
// typeof(\a -> a)
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleType* body;
} FblePolyType;

// FblePolyApplyType -- FBLE_POLY_APPLY_TYPE
//
// We maintain an invariant when constructing FblePolyApplyTypes that the poly is
// not a FBLE_TYPE_TYPE. For example: (typeof(f) x) is constructed as
// typeof(f x).
typedef struct {
  FbleType _base;
  FbleType* poly;
  FbleType* arg;
} FblePolyApplyType;

// FbleVarType --
//   FBLE_VAR_TYPE
//
// A variable type whose value may or may not be known. Used for the value of
// type paramaters and recursive type values.
typedef struct FbleVarType {
  FbleType _base;
  FbleKind* kind;
  FbleName name;
  FbleType* value;
} FbleVarType;

// FbleVarTypeV - a vector of var types.
typedef struct {
  size_t size;
  FbleVarType** xs;
} FbleVarTypeV;

// FbleTypeType --
//   FBLE_TYPE_TYPE
//
// The type of a type.
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleTypeType;

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
FbleKind* FbleGetKind(FbleArena* arena, FbleType* type);

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
FbleType* FbleTypeRetain(FbleTypeArena* arena, FbleType* type);

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
void FbleTypeRelease(FbleTypeArena* arena, FbleType* type);

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
FbleType* FbleNewPolyType(FbleTypeArena* arena, FbleLoc loc, FbleType* arg, FbleType* body);

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
FbleType* FbleNewPolyApplyType(FbleTypeArena* arena, FbleLoc loc, FbleType* poly, FbleType* arg);

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
//   The caller is responsible for calling FbleTypeRelease on the returned type
//   when it is no longer needed.
FbleType* FbleNormalType(FbleTypeArena* arena, FbleType* type);

// FbleTypesEqual --
//   Test whether the two given evaluated types are equal.
//
// Inputs:
//   arena - arena to use for allocations. TODO: Remove this.
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
bool FbleTypesEqual(FbleTypeArena* arena, FbleType* a, FbleType* b);

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
void FblePrintType(FbleArena* arena, FbleType* type);

#endif // FBLE_TYPE_H_
