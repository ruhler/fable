/**
 * @file fble-value.h
 *  API for interacting with fble values.
 */

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include <stdbool.h>    // for bool

#include "fble-profile.h"   // for FbleProfile

/**
 * Forward reference for FbleExecutable
 *
 * See fble-execute.h for the full definition.
 */
typedef struct FbleExecutable FbleExecutable;

/**
 * Memory heap for allocating fble values.
 */
typedef struct FbleHeap FbleValueHeap;

/** An fble value. */
typedef struct FbleValue FbleValue;

/** Vector of FbleValue* */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleValue** xs;   /**< Elements .*/
} FbleValueV;

/**
 * @func[FbleNewValueHeap] Creates a new FbleValueHeap.
 *  @returns FbleValueHeap*
 *   A heap that can be used to allocate values.
 *
 *  @sideeffects
 *   Allocates a heap that should be freed using FbleFreeValueHeap.
 */
FbleValueHeap* FbleNewValueHeap();

/**
 * @func[FbleFreeValueHeap] Frees an FbleValueHeap.
 *  @arg[FbleValueHeap*][heap] The heap to free.
 *
 *  @sideeffects
 *   The resources associated with the given heap are freed. The heap should
 *   not be used after this call.
 */
void FbleFreeValueHeap(FbleValueHeap* heap);

/**
 * @func[FbleRetainValue] Increments refcount on an FbleValue.
 *  Keep the given value alive until a corresponding FbleReleaseValue is
 *  called.
 *
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*    ][src ] The value to retain. May be NULL.
 *
 *  @sideeffects
 *   Causes the value to be retained until a corresponding FbleReleaseValue
 *   calls is made on the value. FbleReleaseValue must be called when the
 *   value is no longer needed.
 */
void FbleRetainValue(FbleValueHeap* heap, FbleValue* src);

/**
 * @func[FbleReleaseValue] Decrements refcount on an FbleValue.
 *  Decrements the strong reference count of a value and free the resources
 *  associated with that value if it has no more references.
 *
 *  @arg[FbleValueHeap*][heap ] The heap the value was allocated with.
 *  @arg[FbleValue*    ][value]
 *   The value to decrement the strong reference count of. The value may be
 *   NULL, in which case no action is performed.
 *
 *  @sideeffects
 *   Decrements the strong reference count of the value and frees resources
 *   associated with the value if there are no more references to it.
 */
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value);

/**
 * @func[FbleReleaseValues] Releases multiple values at once.
 *  Calls FbleReleaseValue for each provide value.
 *
 *  @arg[FbleValueHeap*][heap] The heap the values were allocated with.
 *  @arg[size_t        ][argc] The number of values to release.
 *  @arg[FbleValue**   ][args] The values to release.
 *
 *  @sideeffects
 *   Calls FbleReleaseValue on all the values.
 */
void FbleReleaseValues(FbleValueHeap* heap, size_t argc, FbleValue** args);

/**
 * @func[FbleReleaseValues_] Releases multiple values at once using varargs.
 *  Calls FbleReleaseValue for each provide value.
 *
 *  @arg[FbleValueHeap*][heap] The heap the values were allocated with.
 *  @arg[size_t        ][argc] The number of values to release.
 *  @arg[...           ][    ] The values to release.
 *
 *  @sideeffects
 *   Calls FbleReleaseValue on all the values.
 */
void FbleReleaseValues_(FbleValueHeap* heap, size_t argc, ...);

/**
 * @func[FbleValueAddRef] Adds reference from one value to another.
 *  Notifies the value heap of a new reference from src to dst.
 *
 *  @arg[FbleValueHeap*][heap] The heap the values are allocated on.
 *  @arg[FbleValue*    ][src ] The source of the reference.
 *  @arg[FbleValue*    ][dst ]
 *   The destination of the reference. Must not be NULL.
 *
 *  @sideeffects
 *   Causes the dst value to be retained for at least as long as the src value.
 */
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);

/**
 * @func[FbleValueFullGc] Performs a full garbage collection.
 *  Frees any unreachable objects currently on the heap.
 *
 *  This is an expensive operation intended only for test and debug purposes.
 *
 *  @arg[FbleValueHeap*][heap] The heap to perform gc on.
 *
 *  @sideeffects
 *   Frees any unreachable objects currently on the heap.
 */
void FbleValueFullGc(FbleValueHeap* heap);

/**
 * FbleValue instance for types.
 *
 * Used as an instance of an fble type for those types that don't need any
 * extra information at runtime.
 */
extern FbleValue* FbleGenericTypeValue;

/**
 * @func[FbleNewStructValue] Creates a new struct value.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of fields in the struct value.
 *  @arg[FbleValue**   ][args]
 *    @a[argc] arguments to the struct value. Args are borrowed, and may be
 *   NULL.
 *
 *  @returns[FbleValue*]
 *   A newly allocated struct value with given args.
 *
 *  @sideeffects
 *   The returned struct value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

/**
 * @func[FbleNewStructValue_] Creates a new struct value using varargs.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of fields in the struct value.
 *  @arg[...           ][    ]
 *    @a[argc] FbleValue arguments to the struct value. Args are borrowed, and
 *   may be NULL.
 *
 *  @returns FbleValue*
 *   A newly allocated struct value with given args.
 *
 *  @sideeffects
 *   The returned struct value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, ...);

/**
 * @func[FbleStructValueAccess] Gets a field of a struct value.
 *  @arg[FbleValue*][object] The struct value object to get the field value of.
 *  @arg[size_t    ][field ] The field to access.
 *
 *  @returns FbleValue*
 *   The value of the given field of the struct value object. The returned
 *   value will stay alive as long as the given struct value. The caller is
 *   responsible for calling FbleRetainValue on the returned value to keep it
 *   alive longer if necessary.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a struct value or the field
 *   is invalid.
 */
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field);

/**
 * @func[FbleNewUnionValue] Creates a new union value.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][tag ] The tag of the union value.
 *  @arg[FbleValue*    ][arg ] The argument of the union value. Borrowed.
 *
 *  @returns FbleValue*
 *   A newly allocated union value with given tag and arg.
 *
 *  @sideeffects
 *   The returned union value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg);

/**
 * @func[FbleNewEnumValue] Creates a new enum value.
 *  Convenience function for creating unions with value of type *().
 *
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][tag ] The tag of the union value.
 *
 *  @returns FbleValue*
 *   A newly allocated union value with given tag and arg.
 *
 *  @sideeffects
 *   The returned union value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag);

/**
 * @func[FbleUnionValueTag] Gets the tag of a union value.
 *  @arg[FbleValue*][object] The union value object to get the tag of.
 *
 *  @returns size_t
 *   The tag of the union value object.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
size_t FbleUnionValueTag(FbleValue* object);

/**
 * @func[FbleUnionValueAccess] Gets the argument of a union value.
 *  @arg[FbleValue*][object] The union value object to get the argument of.
 *
 *  @returns FbleValue*
 *   The argument of the union value object.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
FbleValue* FbleUnionValueAccess(FbleValue* object);

/**
 * @func[FbleNewListValue] Creates an fble list value.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of elements on the list.
 *  @arg[FbleValue**   ][args] The elements to put on the list. Borrowed.
 *
 *  @returns FbleValue*
 *   A newly allocated list value.
 *
 *  @sideeffects
 *   The returned value must be freed using FbleReleaseValue when no longer in
 *   use.
 */
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

/**
 * @func[FbleNewListValue_] Creates an fble list value using varargs.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of elements on the list.
 *  @arg[...           ][    ]
 *    @a[argc] FbleValue elements to put on the list. Borrowed.
 *
 *  @returns FbleValue*
 *   A newly allocated list value.
 *
 *  @sideeffects
 *   The returned value must be freed using FbleReleaseValue when no longer in
 *   use.
 */
FbleValue* FbleNewListValue_(FbleValueHeap* heap, size_t argc, ...);

/**
 * @func[FbleNewLiteralValue] Creates an fble literal value.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of letters in the literal.
 *  @arg[size_t*       ][args] The tags of the letters in the literal.
 *
 *  @returns FbleValue*
 *   A newly allocated literal value.
 *
 *  @sideeffects
 *   The returned value must be freed using FbleReleaseValue when no longer in
 *   use.
 */
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args);

/**
 * @func[FbleNewFuncValue] Creates an fble function value.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleExecutable*] executable
 *   The executable to run. Borrowed.
 *  @arg[size_t] profile_block_offset
 *   The profile block offset to use for the function.
 *  @arg[FbleValue**] statics
 *   The array of static variables for the function. The count should match
 *   executable->statics.
 *
 *  @returns FbleValue*
 *   A newly allocated function value.
 *
 *  @sideeffects
 *   Allocates a new function value that should be freed using
 *   FbleReleaseValue when it is no longer needed.
 */
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_offset, FbleValue** statics);

/**
 * @func[FbleNewFuncValue_] Creates an fble function value using varargs.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleExecutable*] executable
 *   The executable to run. Borrowed.
 *  @arg[size_t] profile_block_offset
 *   The profile block offset to use for the function.
 *  @arg[...][]
 *   Static variables for the function. The count should match
 *   executable->num_statics.
 *
 *  @returns FbleValue*
 *   A newly allocated function value.
 *
 *  @sideeffects
 *   Allocates a new function value that should be freed using
 *   FbleReleaseValue when it is no longer needed.
 */
FbleValue* FbleNewFuncValue_(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_offset, ...);

/**
 * Info associated with an fble function value.
 */
typedef struct {
  /** Executable for running the function. */
  FbleExecutable* executable;

  /** Relative offset for block ids referenced from the function. */
  size_t profile_block_offset;

  /**
   * The function's static variables.
   *
   * The number of elements is executable->num_statics.
   */
  FbleValue** statics;
} FbleFuncInfo;

/**
 * @func[FbleFuncValueInfo] Gets the info associated with a function.
 *  The returned info is owned by the function. It is only valid for as long
 *  as the function is valid, and it will be automatically cleaned up as part
 *  of the function's cleanup.
 *
 *  @arg[FbleValue*][func] The function to get the info for.
 *  @returns[FbleFuncInfo] The info for the function.
 *  @sideeffects None.
 */
FbleFuncInfo FbleFuncValueInfo(FbleValue* func);

/**
 * @func[FbleEval] Evaluates a linked program.
 *  The program is assumed to be a zero argument function as returned by
 *  FbleLink.
 * 
 *  @arg[FbleValueHeap*][heap   ] The heap to use for allocating values.
 *  @arg[FbleValue*    ][program] The program to evaluate.
 *  @arg[FbleProfile*  ][profile]
 *   The profile to update. May be NULL to disable profiling.
 * 
 *  @returns FbleValue*
 *   The value of the evaluated program, or @l[NULL] in case of a runtime
 *   error in the program.
 * 
 *  @sideeffects
 *   @item
 *    The returned value must be freed with @l[FbleReleaseValue] when no
 *    longer in use.
 *   @i Prints an error message to stderr in case of a runtime error.
 *   @item
 *    Updates profiling information in profile based on the execution of the
 *    program.
 */
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile);

/**
 * @func[FbleApply] Applies an fble function to arguments.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocating values.
 *  @arg[FbleValue*    ][func] The function to apply.
 *  @arg[FbleValue**] args
 *   The arguments to apply the function to. length == func->argc.
 *  @arg[FbleProfile*] profile
 *   The profile to update. May be NULL to disable profiling.
 *
 *  @returns FbleValue*
 *   The result of applying the function to the given arguments.
 *
 *  @sideeffects
 *   @item
 *    The returned value must be freed with FbleReleaseValue when no longer
 *    in use.
 *   @item
 *    Does not take ownership of the func. Does not take ownership of the
 *    args.
 *   @i Prints warning messages to stderr.
 *   @i Prints an error message to stderr in case of error.
 *   @i Updates the profile with stats from the evaluation.
 */
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);

/**
 * @func[FbleNewRefValue] Creates a reference value.
 *  Used internally for recursive let declarations. A reference value is a
 *  possibly as-of-yet undefined pointer to another value.
 *
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *
 *  @returns FbleValue*
 *   A newly allocated reference value.
 *
 *  @sideeffects
 *   The returned value must be freed using FbleReleaseValue when no longer in
 *   use.
 */
FbleValue* FbleNewRefValue(FbleValueHeap* heap);

/**
 * @func[FbleAssignRefValue] Sets the value pointed to by a ref value.
 *  @arg[FbleValueHeap*][heap ] The heap to use for allocations
 *  @arg[FbleValue*    ][ref  ] The reference to assign to
 *  @arg[FbleValue*    ][value] The value to assign to the reference.
 *
 *  @returns bool
 *   True on success. False if the assignment would produce a vacuous value.
 *
 *  @sideeffects
 *   Updates ref to point to value.
 */
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue* ref, FbleValue* value);

/**
 * @func[FbleStrictValue] Removes layers of refs from a value.
 *  Gets the strict value associated with the given value, which will either
 *  be the value itself, or the dereferenced value if the value is a
 *  reference.
 *
 *  @arg[FbleValue*][value] The value to get the strict version of.
 *
 *  @returns FbleValue*
 *   The value with all layers of reference indirection removed. NULL if the
 *   value is a reference that has no value yet.
 *
 *  @sideeffects
 *   None.
 */
FbleValue* FbleStrictValue(FbleValue* value);

#endif // FBLE_VALUE_H_
