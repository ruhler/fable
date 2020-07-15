// type.h --
//   Header file for the internal fble types API.

#ifndef FBLE_INTERNAL_TYPE_H_
#define FBLE_INTERNAL_TYPE_H_

#include <stdint.h>   // for uintptr_t

#include "syntax.h"
#include "heap.h"

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
//
// Fields:
//   id - a unique id for this type that is preserved across type level
//        substitution. For internal use in type.c.
typedef struct FbleType {
  FbleTypeTag tag;
  FbleLoc loc;
  uintptr_t id;
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

// FbleDataType -- FBLE_STRUCT_TYPE, FBLE_UNION_TYPE
typedef struct {
  FbleType _base;
  FbleTaggedTypeV fields;
} FbleDataType;

// FbleFuncType -- FBLE_FUNC_TYPE
typedef struct {
  FbleType _base;
  FbleTypeV args;
  FbleType* rtype;
} FbleFuncType;

// FbleProcType -- FBLE_PROC_TYPE
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleProcType;

// FbleVarType --
//   FBLE_VAR_TYPE
//
// A variable type whose value may or may not be known. Used for the value of
// type paramaters and recursive type values.
//
// The 'kind' refers to the kind of value that has this type.
//
// We maintain an invariant when constructing FbleVarTypes that the value is
// not an FBLE_TYPE_TYPE. In other words, the kind must have kind level 0.
// Construct var types using FbleNewVarType to enforce this invariant.
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

// FblePolyType -- FBLE_POLY_TYPE
//
// We maintain an invariant when constructing FblePolyTypes that the body is
// not an FBLE_TYPE_TYPE. For example: \a -> typeof(a) is constructed as
// typeof(\a -> a). Construct FblePolyTypes using FbleNewPolyType to enforce
// this invariant.
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleType* body;
} FblePolyType;

// FblePolyApplyType -- FBLE_POLY_APPLY_TYPE
//
// We maintain an invariant when constructing FblePolyApplyTypes that the poly is
// not a FBLE_TYPE_TYPE. For example: (typeof(f) x) is constructed as
// typeof(f x). Construct FblePolyApplyTypes using FbleNewPolyApplyType to
// enforce this invariant.
typedef struct {
  FbleType _base;
  FbleType* poly;
  FbleType* arg;
} FblePolyApplyType;

// FbleTypeType --
//   FBLE_TYPE_TYPE
//
// The type of a type.
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleTypeType;

// FbleGetKind --
//   Get the kind of a value with the given type.
//
// Inputs:
//   arena - arena to use for allocations.
//   type - the type of the value to get the kind of.
//
// Results:
//   The kind of the value with the given type.
//
// Side effects:
//   The caller is responsible for calling FbleFreeKind on the returned
//   kind when it is no longer needed.
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

typedef FbleHeap FbleTypeHeap;

// FbleNewTypeHeap --
//   Creates a new type heap backed by the given arena.
//
// Inputs:
//   arena - the arena to back the type heap.
//
// Results:
//   A newly allocated type heap.
//
// Side effects:
//   Allocates a new type heap that should be freed with FbleFreeTypeHeap
//   when no longer in use.
FbleTypeHeap* FbleNewTypeHeap(FbleArena* arena);

// FbleFreeTypeHeap --
//   Frees resources associated with the given type heap.
//
// Inputs:
//   heap - the heap to free
//
// Results:
//   none.
//
// Side effects:
//   Frees resources associated with the given type heap. The type heap must
//   not be accessed after this call.
void FbleFreeTypeHeap(FbleTypeHeap* heap);

// FbleNewType --
//   Allocate a new type. This function is not type safe.
//
// Inputs:
//   heap - heap to use for allocations
//   T - the type of the type to allocate.
//   tag - the tag of the type
//   loc - the source loc of the type
//
// Results:
//   None.
//
// Side effects:
//   Allocates a new type that should be released when no longer needed.
#define FbleNewType(heap, T, tag, loc) ((T*) FbleNewTypeRaw(heap, sizeof(T), tag, loc))
FbleType* FbleNewTypeRaw(FbleTypeHeap* heap, size_t size, FbleTypeTag tag, FbleLoc loc);

// FbleRetainType --
//   Takes a reference to a compiled type.
//
// Inputs:
//   heap - the heap the type was allocated on.
//   type - the type to take the reference for.
//
// Results:
//   The type with incremented strong reference count.
//
// Side effects:
//   The returned type must be freed using FbleReleaseType when no longer in
//   use.
FbleType* FbleRetainType(FbleTypeHeap* heap, FbleType* type);

// FbleReleaseType --
//   Drop a reference to a compiled type.
//
// Inputs:
//   heap - the heap the type was allocated on.
//   type - the type to drop the refcount for. May be NULL.
//
// Results:
//   None.
//
// Side effects:
//   Decrements the strong refcount for the type and frees it if there are no
//   more references to it.
void FbleReleaseType(FbleTypeHeap* heap, FbleType* type);

// FbleTypeAddRef --
//   Notify the type heap of a new reference from src to dst.
//
// Inputs:
//   heap - the heap the types are allocated on.
//   src - the source of the reference.
//   dst - the destination of the reference.
//
// Side effects:
//   Causes the dst type to be retained for at least as long as the src type.
void FbleTypeAddRef(FbleTypeHeap* heap, FbleType* src, FbleType* dst);

// FbleNewVarType --
//   Construct a VarType. Maintains the invariant the that a higher kinded var
//   types is constructed as typeof a lower kinded var type.
//
// Inputs:
//   heap - the heap to allocate the type on.
//   loc - the location for the type. Borrowed.
//   kind - the kind of a value of this type. Borrowed.
//   name - the name of the type variable. Borrowed.
//
// Results:
//   A type representing an abstract variable type of given kind and name.
//   This may be a TYPE_TYPE if kind has kind level greater than 0.
//   The value of the variable type is initialized to NULL. Use
//   FbleAssignVarType to set the value of the var type if desired.
//
// Side effects:
//   The caller is responsible for calling FbleReleaseType on the returned
//   type when it is no longer needed.
FbleType* FbleNewVarType(FbleTypeHeap* heap, FbleLoc loc, FbleKind* kind, FbleName name);

// FbleAssignVarType --
//   Assign a value to the given abstract type.
//
// Inputs:
//   arena - the heap to use for allocations.
//   var - the type to assign the value of. This type should have been created
//         with FbleNewVarTYpe.
//   value - the value to assign to the type.
//
// Results:
//   none.
//
// Side effects:
//   Sets the value of the var type to the given value.
//   This function does not take ownership of either var or value types. 
//   Behavior is undefined if var is not a type constructed with
//   FbleNewVarType or the kind of value does not match the kind of var.
void FbleAssignVarType(FbleTypeHeap* heap, FbleType* var, FbleType* value);

// FbleNewPolyType --
//   Construct a PolyType. Maintains the invariant that poly of a typeof
//   should be constructed as a typeof a poly.
//
// Inputs:
//   heap - the heap to use for allocations.
//   loc - the location for the type.
//   arg - the poly arg.
//   body - the poly body.
//
// Results:
//   A type representing the poly type: \arg -> body.
//
// Side effects:
//   The caller is responsible for calling FbleReleaseType on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed arg or body types.
FbleType* FbleNewPolyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleType* body);

// FbleNewPolyApplyType --
//   Construct a PolyApplyType. Maintains the invariant that poly apply of a
//   typeof should be constructed as a typeof a poly apply.
//
// Inputs:
//   heap - the heap to use for allocations.
//   loc - the location for the type.
//   poly - the poly apply poly.
//   arg - the poly apply arg.
//
// Results:
//   An unevaluated type representing the poly apply type: poly<arg>
//
// Side effects:
//   The caller is responsible for calling FbleReleaseType on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed poly or arg types.
FbleType* FbleNewPolyApplyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* poly, FbleType* arg);

// FbleNewListType --
//   Convenience function for creating the type of an fble list expression.
//
// Inputs:
//   heap - heap to use for allocations
//   elem_type - the type of the elements of the list. Borrowed.
//
// Results:
//   The type of an fble list of elements.
//
// Side effects:
//   The caller is responsible for calling FbleReleaseType on the returned type
//   when it is no longer needed. This function does not take ownership of the
//   passed elem_type.
FbleType* FbleNewListType(FbleTypeHeap* heap, FbleType* elem_type);

// FbleTypeIsVacuous --
//   Check if a type will reduce to normal form.
//
// Inputs:
//   heap - heap to use for allocations.
//   type - the type to check.
//
// Results:
//   true if the type will fail to reduce to normal form because it is
//   vacuous, false otherwise.
//
// Side effects:
//   None.
bool FbleTypeIsVacuous(FbleTypeHeap* heap, FbleType* type);

// FbleNormalType --
//   Reduce an evaluated type to normal form. Normal form types are struct,
//   union, and func types, but not var types, for example.
//
// Inputs:
//   heap - heap to use for allocations.
//   type - the type to reduce.
//
// Results:
//   The type reduced to normal form.
//
// Side effects:
//   The caller is responsible for calling FbleReleaseType on the returned type
//   when it is no longer needed. The behavior is undefined if the type is
//   vacuous.
FbleType* FbleNormalType(FbleTypeHeap* heap, FbleType* type);

// FbleValueOfType --
//   Returns the value of a type given the type of the type.
//
// Inputs:
//   heap - the heap to use for allocations.
//   typeof - the type of the type to get the value of.
//
// Results:
//   The value of the type to get the value of. Or NULL if the value is not a
//   type.
//
// Side effects:
//   The returned type must be released using FbleReleaseType when no longer
//   needed.
FbleType* FbleValueOfType(FbleTypeHeap* heap, FbleType* typeof);

// FbleTypesEqual --
//   Test whether the two given evaluated types are equal.
//
// Inputs:
//   heap - heap to use for allocations.
//   a - the first type
//   b - the second type
//
// Results:
//   True if the first type equals the second type, false otherwise.
//
// Side effects:
//   None.
bool FbleTypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b);

// FblePrintType --
//   Print the given compiled type in human readable form to stderr.
//
// Inputs:
//   heap - arena to use for internal allocations.
//   type - the type to print.
//
// Result:
//   None.
//
// Side effect:
//   Prints the given type in human readable form to stderr.
void FblePrintType(FbleArena* arena, FbleType* type);

#endif // FBLE_INTERNAL_TYPE_H_
