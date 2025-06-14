/**
 * @file type.h
 *  Header for FbleType.
 */

#ifndef FBLE_INTERNAL_TYPE_H_
#define FBLE_INTERNAL_TYPE_H_

#include <stdint.h>   // for uintptr_t

#include <fble/fble-loc.h>
#include <fble/fble-module-path.h>
#include <fble/fble-name.h>

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
  FBLE_PRIVATE_TYPE,
  FBLE_VAR_TYPE,
  FBLE_TYPE_TYPE,
} FbleTypeTag;

/**
 * @struct[FbleType] FbleType base class.
 *  A tagged union of type types. All types have the same initial layout as
 *  FbleType. The tag can be used to determine what kind of type this is to
 *  get access to additional fields of the type by first casting to that
 *  specific type of type.
 *
 *  @field[FbleTypeTag][tag] The kind of FbleType.
 *  @field[FbleLoc][loc] Source location for error reporting.
 *  @field[bool][visiting] Internal flag. Do not touch.
 */
typedef struct FbleType {
  FbleTypeTag tag;
  FbleLoc loc;
  bool visiting;
} FbleType;

/**
 * @struct[FbleTypeV] Vector of FbleType.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleType**][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleType** xs;
} FbleTypeV;

/**
 * @struct[FbleTaggedType] Type name pair.
 *  Used to describe type and function arguments.
 *
 *  @field[FbleType*][type] The type.
 *  @field[FbleName][name] The name.
 */
typedef struct {
  FbleType* type;
  FbleName name;
} FbleTaggedType;

/**
 * @struct[FbleTaggedTypeV] Vector of FbleTaggedType.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTaggedType*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTaggedType* xs;
} FbleTaggedTypeV;

/**
 * @struct[FbleDataType] A struct or union type.
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleDataTypeTag][datatype] Whether this is for struct or union.
 *  @field[FbleTaggedTypeV][fields] The fields of the data type.
 */
typedef struct {
  FbleType _base;
  FbleDataTypeTag datatype;
  FbleTaggedTypeV fields;
} FbleDataType;

/**
 * @struct[FbleFuncType] A function type.
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleType*][arg] Argument type.
 *  @field[FbleType*][rtype] Return type.
 */
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleType* rtype;
} FbleFuncType;

/**
 * @struct[FblePackageType] A package type.
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleModulePath*][path] The package path.
 */
typedef struct {
  FbleType _base;
  FbleModulePath* path;
} FblePackageType;

/**
 * @struct[FblePrivateType] A private type.
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleType*][arg] The argument type.
 *  @field[FbleModulePath*][path] The package path.
 */
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleModulePath* package;
} FblePrivateType;

/**
 * @struct[FbleVarType] A type variable.
 *  Used for the value of type parameters and recursive type values.
 *
 *  We maintain an invariant when constructing FbleVarTypes that the value is
 *  not an FBLE_TYPE_TYPE. In other words, the kind must have kind level 0.
 *  Construct var types using FbleNewVarType to enforce this invariant.
 *
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleKind*][kind] The kind of value that has this type.
 *  @field[FbleName][name] The name of the type variable.
 *  @field[FbleType*][value] The value of the type variable. May be NULL.
 */
typedef struct FbleVarType {
  FbleType _base;
  FbleKind* kind;
  FbleName name;
  FbleType* value;
} FbleVarType;

/**
 * @struct[FbleVarTypeV] Vector of FbleVarType.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleVarType**][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleVarType** xs;
} FbleVarTypeV;

/**
 * @struct[FblePolyType] A polymorphic type.
 *  We maintain an invariant when constructing FblePolyTypes that the body is
 *  not an FBLE_TYPE_TYPE. For example: @l{\a -> typeof(a)} is constructed as
 *  @l{typeof(\a -> a)}. Construct FblePolyTypes using FbleNewPolyType to
 *  enforce this invariant.
 *
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleType*][arg] Argument to the poly type.
 *  @field[FbleType*][body] The body of the poly type.
 */
typedef struct {
  FbleType _base;
  FbleType* arg;
  FbleType* body;
} FblePolyType;

/**
 * @struct[FblePolyApplyType] A poly applied type.
 *  We maintain an invariant when constructing FblePolyApplyTypes that the
 *  poly is not a FBLE_TYPE_TYPE. For example: (typeof(f) x) is constructed as
 *  typeof(f x). Construct FblePolyApplyTypes using FbleNewPolyApplyType to
 *  enforce this invariant.
 *
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleType*][poly] The poly to apply.
 *  @field[FbleType*][arg] Argument to the poly.
 */
typedef struct {
  FbleType _base;
  FbleType* poly;
  FbleType* arg;
} FblePolyApplyType;

/**
 * @struct[FbleTypeType] The type of a type.
 *  @field[FbleType][_base] FbleType base class.
 *  @field[FbleType*][type] The type to represent the type of.
 */
typedef struct {
  FbleType _base;
  FbleType* type;
} FbleTypeType;

/**
 * @struct[FbleTypeAssignment] A type variable assignment.
 *  @field[FbleType*][var] The type variable.
 *  @field[FbleType*][value] The value to assign to the type variable.
 */
typedef struct {
  FbleType* var;
  FbleType* value;
} FbleTypeAssignment;

/**
 * @struct[FbleTypeAssignmentV] Vector of FbleTypeAssignment.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleTypeAssignment*][xs] The elements.
 */
typedef struct {
  size_t size;
  FbleTypeAssignment* xs;
} FbleTypeAssignmentV;

/**
 * @func[FbleGetKind] Gets the kind of a value with the given type.
 *  @arg[FbleModulePath*][context]
 *   The context to use for resolution of private types. NULL can be used for
 *   cases where private access doesn't matter.
 *  @arg[FbleType*][type] The type of the value to get the kind of.
 *
 *  @returns[FbleKind*]
 *   The kind of the value with the given type.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleFreeKind on the returned
 *   kind when it is no longer needed.
 */
FbleKind* FbleGetKind(FbleModulePath* context, FbleType* type);

/**
 * @func[FbleGetKindLevel]
 * @ Returns the level of the fully applied version of this kind.
 *  @arg[FbleKind*][kind] The kind to get the fully applied level of.
 *
 *  @returns[size_t]
 *   The level of the kind after it has been fully applied.
 *
 *  @sideeffects
 *   None.
 */
size_t FbleGetKindLevel(FbleKind* kind);

/**
 * @func[FbleKindsEqual] Tests whether the two given compiled kinds are equal.
 *  @arg[FbleKind*][a] The first kind
 *  @arg[FbleKind*][b] The second kind
 *
 *  @returns[bool]
 *   True if the first kind equals the second kind, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
bool FbleKindsEqual(FbleKind* a, FbleKind* b);

/**
 * @func[FblePrintKind] Prints an FbleKind in human readable form to stderr.
 *  @arg[FbleKind*][kind] The kind to print.
 *
 *  @sideeffects
 *   Prints the given kind in human readable form to stderr.
 */
void FblePrintKind(FbleKind* kind);

/**
 * Heap of type values managed by the garbage collector.
 *
 * Objects are allocated on a heap. They can have references to other objects
 * on the heap, potentially involving cycles.
 */
typedef struct FbleTypeHeap FbleTypeHeap;

/**
 * @func[FbleNewTypeHeap] Creates a new garbage collected heap.
 *  @returns[FbleTypeHeap*]
 *   The newly allocated heap.
 *
 *  @sideeffects
 *   Allocates a new heap. The caller is resposible for
 *   calling FbleFreeTypeHeap when the heap is no longer needed.
 */
FbleTypeHeap* FbleNewTypeHeap();

/**
 * @func[FbleTypeHeapSetContext] Set the module context for type operations.
 *  Many of the operations involving types depend on the module being compiled
 *  to determine how to treat private types. For convenience, we store the
 *  context along with the type heap so you only have to pass one thing
 *  around everywhere all the time.
 *
 *  @arg[FbleTypeHeap*][heap] The type heap.
 *  @arg[FbleModulePath*][context] The module being compiled.
 *  @sideeffects
 *   Subsequent type operations will use the provided module to determine
 *   access to private types.
 */
void FbleTypeHeapSetContext(FbleTypeHeap* heap, FbleModulePath* context);

/**
 * @func[FbleTypeHeapGetContext] Gets the most recently set context.
 *  @arg[FbleTypeHeap*][heap] The type heap.
 *  @returns[FbleModulePath*]
 *   The context previously passed to FbleTypeHeapSetContext, owned by the
 *   type heap. Make a copy of the returned path if needed.
 *  @sideeffects None.
 */
FbleModulePath* FbleTypeHeapGetContext(FbleTypeHeap* heap);

/**
 * @func[FbleFreeTypeHeap] Frees a heap that is no longer in use.
 *  @arg[FbleTypeHeap*][heap] The heap to free.
 *
 *  @sideeffects
 *   @item
 *    Does a full GC to reclaim all unreachable objects, and frees resources
 *    associated with the given heap.
 *   @item
 *    Does not free objects that are still being retained on the heap. Those
 *    allocations will be leaked.
 */
void FbleFreeTypeHeap(FbleTypeHeap* heap);

/**
 * @func[FbleNewType] Allocates a new type.
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations
 *  @arg[<type>][T] The type of the type to allocate.
 *  @arg[FbleTypeTag][tag] The tag of the type.
 *  @arg[FbleLoc][loc] The source loc of the type. Borrowed.
 *
 *  @returns[T*]
 *   The newly allocated type.
 *
 *  @sideeffects
 *   Allocates a new type that should be released when no longer needed.
 */
#define FbleNewType(heap, T, tag, loc) ((T*) FbleNewTypeRaw(heap, sizeof(T), tag, loc))

/**
 * @func[FbleNewTypeRaw] Allocates a new type.
 *  This function is not type safe.
 *
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations
 *  @arg[size_t][size] The number of bytes to allocate.
 *  @arg[FbleTypeTag][tag] The tag of the type.
 *  @arg[FbleLoc][loc] The source loc of the type. Borrowed.
 *
 *  @returns[FbleType*]
 *   The newly allocated type.
 *
 *  @sideeffects
 *   Allocates a new type that should be released when no longer needed.
 */
FbleType* FbleNewTypeRaw(FbleTypeHeap* heap, size_t size, FbleTypeTag tag, FbleLoc loc);

/**
 * @func[FbleRetainType] Takes a reference to an FbleType.
 *  @arg[FbleTypeHeap*][heap] The heap the type was allocated on.
 *  @arg[FbleTypeHeap*][type] The type to take the reference for. May be NULL.
 *
 *  @returns[FbleType*]
 *   The type with incremented strong reference count.
 *
 *  @sideeffects
 *   The returned type must be freed using FbleReleaseType when no longer in
 *   use.
 */
FbleType* FbleRetainType(FbleTypeHeap* heap, FbleType* type);

/**
 * @func[FbleReleaseType] Drops a reference to an FbleType.
 *  @arg[FbleTypeHeap*][heap] The heap the type was allocated on.
 *  @arg[FbleType*][type] The type to drop the refcount for. May be NULL.
 *
 *  @sideeffects
 *   Decrements the strong refcount for the type and frees it if there are no
 *   more references to it.
 */
void FbleReleaseType(FbleTypeHeap* heap, FbleType* type);

/**
 * @func[FbleTypeAddRef] Adds a reference from one type to another.
 *  Notifies the type heap of a new reference from src to dst.
 *
 *  @arg[FbleTypeHeap*][heap] The heap the types are allocated on.
 *  @arg[FbleType*][src] The source of the reference.
 *  @arg[FbleType*][dst] The destination of the reference.
 *
 *  @sideeffects
 *   Causes the dst type to be retained for at least as long as the src type.
 */
void FbleTypeAddRef(FbleTypeHeap* heap, FbleType* src, FbleType* dst);

/**
 * @func[FbleNewVarType] Constructs a VarType.
 *  Maintains the invariant the that a higher kinded var types is constructed
 *  as typeof a lower kinded var type.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to allocate the type on.
 *  @arg[FbleLoc][loc] The location for the type. Borrowed.
 *  @arg[FbleKind*][kind] The kind of a value of this type. Borrowed.
 *  @arg[FbleName][name] The name of the type variable. Borrowed.
 *
 *  @returns[FbleType*]
 *   A type representing an abstract variable type of given kind and name.
 *   This may be a TYPE_TYPE if kind has kind level greater than 0.
 *   The value of the variable type is initialized to NULL. Use
 *   FbleAssignVarType to set the value of the var type if desired.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned
 *   type when it is no longer needed.
 */
FbleType* FbleNewVarType(FbleTypeHeap* heap, FbleLoc loc, FbleKind* kind, FbleName name);

/**
 * @func[FbleAssignVarType] Assigns a value to the given abstract type.
 *  @arg[FbleTypeHeap*][heap] The type heap.
 *  @arg[FbleType*][var]
 *   The type to assign the value of. This type should have been created with
 *   FbleNewVarType.
 *  @arg[FbleType*][value] The value to assign to the type.
 *
 *  @sideeffects
 *   @i Sets the value of the var type to the given value.
 *   @i Updates the kind of the var type based on the kind of the given value.
 *   @i This function does not take ownership of either var or value types. 
 *   @item
 *    Behavior is undefined if var is not a type constructed with
 *    FbleNewVarType or the kind level of value does not match the kind level
 *    of var.
 */
void FbleAssignVarType(FbleTypeHeap* heap, FbleType* var, FbleType* value);

/**
 * @func[FbleNewPolyType] Constructs a PolyType.
 *  Maintains the invariant that poly of a typeof should be constructed as a
 *  typeof a poly.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleLoc][loc] The location for the type.
 *  @arg[FbleType*][arg] The poly arg.
 *  @arg[FbleType*][body] The poly body.
 *
 *  @returns[FbleType*]
 *   A type representing the poly type: arg -> body.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed. This function does not take ownership of the
 *   passed arg or body types.
 */
FbleType* FbleNewPolyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleType* body);

/**
 * @func[FbleNewPolyApplyType] Constructs a PolyApplyType.
 *  Maintains the invariant that poly apply of a typeof should be constructed
 *  as a typeof a poly apply.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleLoc][loc] The location for the type.
 *  @arg[FbleType*][poly] The poly apply poly.
 *  @arg[FbleType*][arg] The poly apply arg.
 *
 *  @returns[FbleType*]
 *   An unevaluated type representing the poly apply type: poly<arg>
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed. This function does not take ownership of the
 *   passed poly or arg types.
 */
FbleType* FbleNewPolyApplyType(FbleTypeHeap* heap, FbleLoc loc, FbleType* poly, FbleType* arg);

/**
 * @func[FbleNewPrivateType] Constructs a private type.
 *  We maintain an invariant when constructing FblePrivateTypes that the value
 *  is not an FBLE_TYPE_TYPE. In other words, the kind must have kind level 0.
 *  Construct private types using FbleNewPrivateType to enforce this
 *  invariant.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleLoc][loc] The location for the type.
 *  @arg[FbleType*][arg] The underlying type argument.
 *  @arg[FbleModulePath*][package] The package to restrict the type to.
 *
 *  @returns[FbleType*] A newly allocated private type.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed.
 */
FbleType* FbleNewPrivateType(FbleTypeHeap* heap, FbleLoc loc, FbleType* arg, FbleModulePath* package);

/**
 * @func[FbleTypeIsVacuous] Checks if a type will reduce to normal form.
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations.
 *  @arg[FbleType*][type] The type to check.
 *
 *  @returns[bool]
 *   true if the type will fail to reduce to normal form because it is
 *   vacuous, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
bool FbleTypeIsVacuous(FbleTypeHeap* heap, FbleType* type);

/**
 * @func[FbleNormalType] Reduces an evaluated type to normal form.
 *  Normal form types are struct, union, and func types, but not var types,
 *  for example.
 *
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations.
 *  @arg[FbleType*][type] The type to reduce.
 *
 *  @returns[FbleType*]
 *   The type reduced to normal form.
 *
 *  @sideeffects
 *   The caller is responsible for calling FbleReleaseType on the returned type
 *   when it is no longer needed. The behavior is undefined if the type is
 *   vacuous.
 */
FbleType* FbleNormalType(FbleTypeHeap* heap, FbleType* type);

/**
 * @func[FbleValueOfType]
 * @ Returns the value of a type given the type of the type.
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleType*][typeof] The type of the type to get the value of.
 *
 *  @returns[FbleType*]
 *   The value of the type to get the value of. Or NULL if the value is not a
 *   type.
 *
 *  @sideeffects
 *   The returned type must be released using FbleReleaseType when no longer
 *   needed.
 */
FbleType* FbleValueOfType(FbleTypeHeap* heap, FbleType* typeof);

/**
 * @func[FbleListElementType] Gets the element type for a list.
 *  Returns the element type of a type matching the structure needed for list
 *  literals.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleType*][type] The list type to get the element type of.
 *
 *  @returns[FbleType*]
 *   The element type of the list, or NULL if the type does not match the
 *   structured needed for list listerals.
 *
 *  @sideeffects
 *   The returned type must be released using FbleReleaseType when no longer
 *   needed.
 */
FbleType* FbleListElementType(FbleTypeHeap* heap, FbleType* type);

/**
 * @func[FbleTypesEqual] Tests whether the two given evaluated types are equal.
 *  @arg[FbleTypeHeap*][heap] Heap to use for allocations.
 *  @arg[FbleType*][a] The first type
 *  @arg[FbleType*][b] The second type
 *
 *  @returns[bool]
 *   True if the first type equals the second type, false otherwise.
 *
 *  @sideeffects
 *   None.
 */
bool FbleTypesEqual(FbleTypeHeap* heap, FbleType* a, FbleType* b);

/**
 * @func[FbleInferTypes] Infers type values.
 *  Attempts to infer type values for the given type variables that would make
 *  the abstract type equal to the concrete type. This is a best effort type
 *  inference. It picks in arbitrary assignment in case of ambiguity, and may
 *  not find assignments for everything.
 *
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleTypeAssignmentV][vars]
 *   The list of type variables along with their assignments. Initially
 *   the assigned values can be NULL or non-NULL. If an assignment for a type
 *   variable is non-NULL, the type variable is treated as equal to its
 *   assignment.
 *  @arg[FbleType*][abstract]
 *   The type with occurences of the type variables in it.
 *  @arg[FbleType*][concrete]
 *   The concrete type to infer the values of type variables from.
 *
 *  @sideeffects
 *   Updates vars assignments based on inferred types. The caller is
 *   responsible for freeing any assignements to the type variables added to
 *   vars.
 */
void FbleInferTypes(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* abstract, FbleType* concrete);

/**
 * @func[FbleSpecializeType] Apply a type assignment to a type.
 *  Creates a specialized version of the given type by assigning types as per
 *  the type assignment. For example, specializing @l{Maybe@<T@>} with type
 *  assignment @l{T@ = Int@} will give you a @l{Maybe@<Int@>}.
 * 
 *  @arg[FbleTypeHeap*][heap] The heap to use for allocations.
 *  @arg[FbleTypeAssignmentV][vars] The type assignments to apply.
 *  @arg[FbleType*][type] The type to specialize.
 *
 *  @returns[FbleType*] The specialized type.
 *
 *  @sideeffects
 *   Allocates a new type that should be released when no longer needed.
 */
FbleType* FbleSpecializeType(FbleTypeHeap* heap, FbleTypeAssignmentV vars, FbleType* type);

/**
 * @func[FblePrintType] Prints an FbleType in human readable form to stderr.
 *  @arg[FbleType*][type] The type to print.
 *
 *  @sideeffects
 *   Prints the given type in human readable form to stderr.
 */
void FblePrintType(FbleType* type);

#endif // FBLE_INTERNAL_TYPE_H_
