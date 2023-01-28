/**
 * @file type.h
 * Header for FbleType.
 */

#ifndef FBLE_INTERNAL_TYPE_H_
#define FBLE_INTERNAL_TYPE_H_

#include <stdint.h>   // for uintptr_t

#include <fble/fble-loc.h>
#include <fble/fble-module-path.h>
#include <fble/fble-name.h>

#include "heap.h"
#include "kind.h"     // for FbleKind

/**
 * Different kinds of types.
 */
typedef enum {
  FBLE_DATA_TYPE,
  FBLE_FUNC_TYPE,
  FBLE_POLY_TYPE,
  FBLE_POLY_APPLY_TYPE,
  FBLE_PACKAGE_TYPE,
  FBLE_ABSTRACT_TYPE,
  FBLE_VAR_TYPE,
  FBLE_TYPE_TYPE,
} FbleTypeTag;

/**
 * FbleType base class.
 *
 * A tagged union of type types. All types have the same initial layout as
 * FbleType. The tag can be used to determine what kind of type this is to get
 * access to additional fields of the type by first casting to that specific
 * type of type.
 *
 *   id - a unique id for this type that is preserved across type level
 *        substitution. For internal use in type.c.
 */
typedef struct FbleType {
  FbleTypeTag tag;    /**< The kind of FbleType. */
  FbleLoc loc;        /**< Source location for error reporting. */
} FbleType;

/** Vector of FbleType. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleType** xs;    /**< The elements. */
} FbleTypeV;

/**
 * Type name pair used to describe type and function arguments.
 */
typedef struct {
  FbleType* type;   /**< The type. */
  FbleName name;    /**< The name. */
} FbleTaggedType;

/** Vector of FbleTaggedType. */
typedef struct {
  size_t size;          /**< Number of elements. */
  FbleTaggedType* xs;   /**< The elements. */
} FbleTaggedTypeV;

/**
 * A struct or union type.
 */
typedef struct {
  FbleType _base;             /**< FbleType base class. */
  FbleDataTypeTag datatype;   /**< Whether this is for struct or union. */
  FbleTaggedTypeV fields;     /**< The fields of the data type. */
} FbleDataType;

/** A function type. */
typedef struct {
  FbleType _base;     /**< FbleType base class. */
  FbleTypeV args;     /**< Argument types. */
  FbleType* rtype;    /**< Return type. */
} FbleFuncType;

/** A package type. */
typedef struct {
  FbleType _base;         /**< FbleType base class. */
  FbleModulePath* path;   /**< The package path. */

  /**
   * Helper flag. Used to control whether or not an abstract type associated
   * with this package type is considered equal to its underlying type.  It is
   * temporarily set to false while testing type equality for abstract cast
   * expressions.
   */
  bool opaque;
} FblePackageType;

/**
 * An abstract type. A type protected by a package type.
 */
typedef struct {
  FbleType _base;             /**< FbleType base class. */
  FblePackageType* package;   /**< The package with access. */
  FbleType* type;             /**< The underlying type. */
} FbleAbstractType;

/**
 * A type variable.
 *
 * Used for the value of type parameters and recursive type values.
 *
 * We maintain an invariant when constructing FbleVarTypes that the value is
 * not an FBLE_TYPE_TYPE. In other words, the kind must have kind level 0.
 * Construct var types using FbleNewVarType to enforce this invariant.
 */
typedef struct FbleVarType {
  FbleType _base;   /**< FbleType base class. */
  FbleKind* kind;   /**< The kind of value that has this type. */
  FbleName name;    /**< The name of the type variable. */
  FbleType* value;  /**< The value of the type variable. May be NULL. */
} FbleVarType;

/** Vector of FbleVarType. */
typedef struct {
  size_t size;        /**< Number of elements. */
  FbleVarType** xs;   /**< The elements. */
} FbleVarTypeV;

/**
 * A polymorphic type.
 *
 * We maintain an invariant when constructing FblePolyTypes that the body is
 * not an FBLE_TYPE_TYPE. For example: \a -> typeof(a) is constructed as
 * typeof(\a -> a). Construct FblePolyTypes using FbleNewPolyType to enforce
 * this invariant.
 */
typedef struct {
  FbleType _base;   /**< FbleType base class. */
  FbleType* arg;    /**< Argument to the poly type. */
  FbleType* body;   /**< The body of the poly type. */
} FblePolyType;

/**
 * A poly applied type.
 *
 * We maintain an invariant when constructing FblePolyApplyTypes that the poly is
 * not a FBLE_TYPE_TYPE. For example: (typeof(f) x) is constructed as
 * typeof(f x). Construct FblePolyApplyTypes using FbleNewPolyApplyType to
 * enforce this invariant.
 */
typedef struct {
  FbleType _base;     /**< FbleType base class. */
  FbleType* poly;     /**< The poly to apply. */
  FbleType* arg;      /**< Argument to the poly. */
} FblePolyApplyType;

/**
 * The type of a type.
 */
typedef struct {
  FbleType _base;   /**< FbleType base class. */
  FbleType* type;   /**< The type to represent the type of. */
} FbleTypeType;

/**
 * A type variable assignment.
 */
typedef struct {
  FbleType* var;        /**< The type variable. */
  FbleType* value;      /**< The value to assign to the type variable. */
} FbleTypeAssignment;

/** Vector of FbleTypeAssignment. */
typedef struct {
  size_t size;              /**< Number of elements. */
  FbleTypeAssignment* xs;   /**< The elements. */
} FbleTypeAssignmentV;

/**
 * Gets the kind of a value with the given type.
 *
 * @param type  The type of the value to get the kind of.
 *
 * @returns
 *   The kind of the value with the given type.
 *
 * @sideeffects
 *   The caller is responsible for calling FbleFreeKind on the returned
 *   kind when it is no longer needed.
 */
FbleKind* FbleGetKind(FbleType* type);

/**
 * Returns the level of the fully applied version of this kind.
 *
 * @param kind  The kind to get the fully applied level of.
 *
 * @returns
 *   The level of the kind after it has been fully applied.
 *
 * @sideeffects
 *   None.
 */
size_t FbleGetKindLevel(FbleKind* kind);

/**
 * Tests whether the two given compiled kinds are equal.
 *
 * @param a  The first kind
 * @param b  The second kind
 *
 * @returns
 *   True if the first kind equals the second kind, false otherwise.
 *
 * @sideeffects
 *   None.
 */
bool FbleKindsEqual(FbleKind* a, FbleKind* b);

/**
 * Prints an FbleKind in human readable form to stderr.
 *
 * @param kind  The kind to print.
 *
 * @sideeffects
 *   Prints the given kind in human readable form to stderr.
 */
void FblePrintKind(FbleKind* kind);

/** An FbleHeap for FbleType objects. */
typedef FbleHeap FbleTypeHeap;

/**
 * Creates a new type heap.
 *
 * @returns
 *   A newly allocated type heap.
 *
 * @sideeffects
 *   Allocates a new type heap that should be freed with FbleFreeTypeHeap
 *   when no longer in use.
 */
FbleTypeHeap* FbleNewTypeHeap();

/**
 * Frees resources associated with the given type heap.
 *
 * @param heap  The heap to free
 *
 * @sideeffects
 *   Frees resources associated with the given type heap. The type heap must
 *   not be accessed after this call.
 */
void FbleFreeTypeHeap(FbleTypeHeap* heap);

/**
 * Allocates a new type.
 *
 * @param heap  Heap to use for allocations
 * @param T  The type of the type to allocate.
 * @param tag  The tag of the type.
 * @param loc  The source loc of the type. Borrowed.
 *
 * @returns
 *   The newly allocated type.
 *
 * @sideeffects
 *   Allocates a new type that should be released when no longer needed.
 */
#define FbleNewType(heap, T, tag, loc) ((T*) FbleNewTypeRaw(heap, sizeof(T), tag, loc))

/**
 * Allocates a new type. This function is not type safe.
 *
 * @param heap  Heap to use for allocations
 * @param size  The number of bytes to allocate.
 * @param tag  The tag of the type.
 * @param loc  The source loc of the type. Borrowed.
 *
 * @returns
 *   The newly allocated type.
 *
 * @sideeffects
 *   Allocates a new type that should be released when no longer needed.
 */
FbleType* FbleNewTypeRaw(FbleTypeHeap* heap, size_t size, FbleTypeTag tag, FbleLoc loc);

/**
 * Takes a reference to an FbleType.
 *
 * @param heap  The heap the type was allocated on.
 * @param type  The type to take the reference for.
 *
 * @returns
 *   The type with incremented strong reference count.
 *
 * @sideeffects
 *   The returned type must be freed using FbleReleaseType when no longer in
 *   use.
 */
FbleType* FbleRetainType(FbleTypeHeap* heap, FbleType* type);

/**
 * Drops a reference to an FbleType.
 *
 * @param heap  The heap the type was allocated on.
 * @param type  The type to drop the refcount for. May be NULL.
 *
 * @sideeffects
 *   Decrements the strong refcount for the type and frees it if there are no
 *   more references to it.
 */
void FbleReleaseType(FbleTypeHeap* heap, FbleType* type);

/**
 * Adds a reference from one type to another.
 *
 * Notifies the type heap of a new reference from src to dst.
 *
 * @param heap  The heap the types are allocated on.
 * @param src  The source of the reference.
 * @param dst  The destination of the reference.
 *
 * @sideeffects
 *   Causes the dst type to be retained for at least as long as the src type.
 */
void FbleTypeAddRef(FbleTypeHeap* heap, FbleType* src, FbleType* dst);

/**
 * Constructs a VarType.
 *
 * Maintains the invariant the that a higher kinded var types is constructed
 * as typeof a lower kinded var type.
 *
 * @param heap  The heap to allocate the type on.
 * @param loc  The location for the type. Borrowed.
 * @param kind  The kind of a value of this type. Borrowed.
 * @param name  The name of the type variable. Borrowed.
 *
 * @returns
 *   A type representing an abstract variable type of given kind and name.
 *   This may be a TYPE_TYPE if kind has kind level greater than 0.
 *   The value of the variable type is initialized to NULL. Use
 *   FbleAssignVarType to set the value of the var type if desired.
 *
 * @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned
 *   type when it is no longer needed.
 */
FbleType* FbleNewVarType(FbleTypeHeap* heap, FbleLoc loc, FbleKind* kind, FbleName name);

/**
 * Assigns a value to the given abstract type.
 *
 * @param heap  The type heap.
 * @param var  The type to assign the value of. This type should have been
 *   created with FbleNewVarType.
 * @param value  The value to assign to the type.
 *
 * @sideeffects
 * * Sets the value of the var type to the given value.
 * * This function does not take ownership of either var or value types. 
 * * Behavior is undefined if var is not a type constructed with
 *   FbleNewVarType or the kind of value does not match the kind of var.
 */
void FbleAssignVarType(FbleTypeHeap* heap, FbleType* var, FbleType* value);

/**
 * Constructs a PolyType.
 *
 * Maintains the invariant that poly of a typeof should be constructed as a
 * typeof a poly.
 *
 * @param heap  The heap to use for allocations.
 * @param loc  The location for the type.
 * @param arg  The poly arg.
 * @param body  The poly body.
 *
 * @returns
 *   A type representing the poly type: \arg -> body.
 *
 * @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed. This function does not take ownership of the
 *   passed arg or body types.
 */
FbleType* FbleNewPolyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleType* body);

/**
 * Constructs a PolyApplyType.
 *
 * Maintains the invariant that poly apply of a typeof should be constructed
 * as a typeof a poly apply.
 *
 * @param heap  The heap to use for allocations.
 * @param loc  The location for the type.
 * @param poly  The poly apply poly.
 * @param arg  The poly apply arg.
 *
 * @returns
 *   An unevaluated type representing the poly apply type: poly<arg>
 *
 * @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed. This function does not take ownership of the
 *   passed poly or arg types.
 */
FbleType* FbleNewPolyApplyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* poly, FbleType* arg);

/**
 * Checks if a type will reduce to normal form.
 *
 * @param heap  Heap to use for allocations.
 * @param type  The type to check.
 *
 * @returns
 *   true if the type will fail to reduce to normal form because it is
 *   vacuous, false otherwise.
 *
 * @sideeffects
 *   None.
 */
bool FbleTypeIsVacuous(FbleTypeHeap* heap, FbleType* type);

/**
 * Reduces an evaluated type to normal form.
 * Normal form types are struct, union, and func types, but not var types, for
 * example.
 *
 * @param heap  Heap to use for allocations.
 * @param type  The type to reduce.
 *
 * @returns
 *   The type reduced to normal form.
 *
 * @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed. The behavior is undefined if the type is
 *   vacuous.
 */
FbleType* FbleNormalType(FbleTypeHeap* heap, FbleType* type);

/**
 * Returns the value of a type given the type of the type.
 *
 * @param heap  The heap to use for allocations.
 * @param typeof  The type of the type to get the value of.
 *
 * @returns
 *   The value of the type to get the value of. Or NULL if the value is not a
 *   type.
 *
 * @sideeffects
 *   The returned type must be released using FbleReleaseType when no longer
 *   needed.
 */
FbleType* FbleValueOfType(FbleTypeHeap* heap, FbleType* typeof);

/**
 * Gets the element type for a list.
 *
 * Returns the element type of a type matching the structure needed for list
 * literals.
 *
 * @param heap  The heap to use for allocations.
 * @param type  The list type to get the element type of.
 *
 * @returns
 *   The element type of the list, or NULL if the type does not match the
 *   structured needed for list listerals.
 *
 * @sideeffects
 *   The returned type must be released using FbleReleaseType when no longer
 *   needed.
 */
FbleType* FbleListElementType(FbleTypeHeap* heap, FbleType* type);

/**
 * Tests whether the two given evaluated types are equal.
 *
 * @param heap  Heap to use for allocations.
 * @param a  The first type
 * @param b  The second type
 *
 * @returns
 *   True if the first type equals the second type, false otherwise.
 *
 * @sideeffects
 *   None.
 */
bool FbleTypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b);

/**
 * Infers type values.
 *
 * lnfers type values for the given type variables to make the abstract type
 * equal to the concrete type.
 *
 * @param heap  The heap to use for allocations.
 * @param vars  The list of type variables along with their assignments. Initially
 *   the assigned values can be NULL or non-NULL. If an assignment for a type
 *   variable is non-NULL, the type variable is treated as equal to its
 *   assignment.
 * @param abstract  The type with occurences of the type variables in it.
 * @param concrete  The concrete type to infer the values of type variables from.
 *
 * @returns
 *   True if the abstract type can be made equal to the concrete type with
 *   type inference, false otherwise.
 *
 * @sideeffects
 * * Updates vars assignments based on inferred types. The caller is
 *   responsible for freeing any assignements to the type variables added to
 *   vars.
 */
bool FbleTypeInfer(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* abstract, FbleType* concrete);

/**
 * Prints an FbleType in human readable form to stderr.
 *
 * @param type  The type to print.
 *
 * @sideeffects
 *   Prints the given type in human readable form to stderr.
 */
void FblePrintType(FbleType* type);

#endif // FBLE_INTERNAL_TYPE_H_
