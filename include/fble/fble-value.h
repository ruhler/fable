/**
 * @file fble-value.h
 * API for interacting with fble values.
 */

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include <stdbool.h>    // for bool

#include "fble-profile.h"   // for FbleProfile

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
 * Creates a new FbleValueHeap.
 * 
 * @returns
 *   A heap that can be used to allocate values.
 *
 * @sideeffects
 *   Allocates a heap that should be freed using FbleFreeValueHeap.
 */
FbleValueHeap* FbleNewValueHeap();

/**
 * Frees an FbleValueHeap.
 *
 * @param heap  The heap to free.
 *
 * @sideeffects
 *   The resources associated with the given heap are freed. The heap should
 *   not be used after this call.
 */
void FbleFreeValueHeap(FbleValueHeap* heap);

/**
 * Increments refcount on an FbleValue.
 *
 * Keep the given value alive until a corresponding FbleReleaseValue is
 * called.
 *
 * @param heap  The value heap.
 * @param src   The value to retain.
 *
 * @sideeffects
 *   Causes the value to be retained until a corresponding FbleReleaseValue
 *   calls is made on the value. FbleReleaseValue must be called when the
 *   value is no longer needed.
 */
void FbleRetainValue(FbleValueHeap* heap, FbleValue* src);

/**
 * Decrements refcount on an FbleValue.
 *
 * Decrement the strong reference count of a value and free the resources
 * associated with that value if it has no more references.
 *
 * @param heap    The heap the value was allocated with.
 * @param value   The value to decrement the strong reference count of. The
 *                value may be NULL, in which case no action is performed.
 *
 * @sideeffects
 *   Decrements the strong reference count of the value and frees resources
 *   associated with the value if there are no more references to it.
 */
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value);

/**
 * Adds reference from one value to another.
 *
 * Notifies the value heap of a new reference from src to dst.
 *
 * @param heap  The heap the values are allocated on.
 * @param src   The source of the reference.
 * @param dst   The destination of the reference. Must not be NULL.
 *
 * @sideeffects
 *   Causes the dst value to be retained for at least as long as the src value.
 */
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);

/**
 * Performs a full garbage collection.
 *
 * Frees any unreachable objects currently on the heap.
 *
 * This is an expensive operation intended only for test and debug purposes.
 *
 * @param heap  The heap to perform gc on.
 *
 * @sideeffects
 *   Frees any unreachable objects currently on the heap.
 */
void FbleValueFullGc(FbleValueHeap* heap);

/**
 * Creates a new struct value.
 *
 * @param heap   The heap to allocate the value on.
 * @param argc   The number of fields in the struct value.
 * @param args   argc FbleValue arguments to the struct value. Args are
 *               borrowed, and may be NULL.
 *
 * @returns
 *   A newly allocated struct value with given args.
 *
 * @sideeffects
 *   The returned struct value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

/**
 * Creates a new struct value using varargs.
 *
 * @param heap   The heap to allocate the value on.
 * @param argc   The number of fields in the struct value.
 * @param ...    argc FbleValue arguments to the struct value. Args are
 *               borrowed, and may be NULL.
 *
 * @returns
 *   A newly allocated struct value with given args.
 *
 * @sideeffects
 *   The returned struct value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, ...);

/**
 * Gets a field of a struct value.
 *
 * @param object   The struct value object to get the field value of.
 * @param field    The field to access.
 *
 * @returns
 *   The value of the given field of the struct value object. The returned
 *   value will stay alive as long as the given struct value. The caller is
 *   responsible for calling FbleRetainValue on the returned value to keep it
 *   alive longer if necessary.
 *
 * @sideeffects
 *   Behavior is undefined if the object is not a struct value or the field
 *   is invalid.
 */
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field);

/**
 * Creates a new union value.
 *
 * @param heap   The heap to allocate the value on.
 * @param tag    The tag of the union value.
 * @param arg    The argument of the union value. Borrowed.
 *
 * @returns
 *   A newly allocated union value with given tag and arg.
 *
 * @sideeffects
 *   The returned union value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg);

/**
 * Creates a new enum value.
 *
 * Convenience function for creating unions with value of type *().
 *
 * @param heap   The heap to allocate the value on.
 * @param tag    The tag of the union value.
 *
 * @returns
 *   A newly allocated union value with given tag and arg.
 *
 * @sideeffects
 *   The returned union value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag);

/**
 * Gets the tag of a union value.
 *
 * @param object  The union value object to get the tag of.
 *
 * @returns
 *   The tag of the union value object.
 *
 * @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
size_t FbleUnionValueTag(FbleValue* object);

/**
 * Gets the argument of a union value.
 *
 * @param object  The union value object to get the argument of.
 *
 * @returns
 *   The argument of the union value object.
 *
 * @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
FbleValue* FbleUnionValueAccess(FbleValue* object);

/**
 * C interface for simple fble functions.
 *
 * @warning
 *   Don't do anything weird in the implementation of the function like
 *   calling back into fble functions.
 *
 * @param heap   The heap to allocate values on.
 * @param args   Array of arguments passed to the function. The number of args
 *               is specified in FbleNewSimpleFuncValue.
 *
 * @returns
 *   The result of executing the function, or NULL to abort execution of the
 *   program.
 *
 * @see FbleNewSimpleFuncValue.
 */
typedef FbleValue* (*FbleSimpleFunc)(FbleValueHeap* heap, FbleValue** args);

/**
 * Creates an fble function value.
 *
 * TODO: Figure out how to make this more general. For now we don't allow
 * userdata, static variables, or any of that sort of thing.
 *
 * @param heap     The heap to allocate the value on.
 * @param argc     The number of function arguments.
 * @param impl     The implementation of the function.
 * @param profile  A profile block id to use when the function is called.
 *
 * @returns
 *   A newly allocated function value.
 *
 * @sideeffects
 *   The returned function value must be freed using FbleReleaseValue when no
 *   longer in use.
 */
FbleValue* FbleNewSimpleFuncValue(FbleValueHeap* heap, size_t argc, FbleSimpleFunc impl, FbleBlockId profile);

/**
 * Evaluates a linked program.
 * 
 * The program is assumed to be a zero argument function as returned by
 * FbleLink.
 *
 * @param heap     The heap to use for allocating values.
 * @param program  The program to evaluate.
 * @param profile  The profile to update. May be NULL to disable profiling.
 *
 * @returns
 *   The value of the evaluated program, or NULL in case of a runtime error in
 *   the program.
 *
 * @sideeffects
 * * The returned value must be freed with FbleReleaseValue when no longer in
 *   use.
 * * Prints an error message to stderr in case of a runtime error.
 * * Updates profiling information in profile based on the execution of the
 *   program.
 */
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile);

/**
 * Applies an fble function to arguments.
 *
 * @param heap     The heap to use for allocating values.
 * @param func     The function to apply.
 * @param args     The arguments to apply the function to. length == func->argc.
 * @param profile  The profile to update. May be NULL to disable profiling.
 *
 * @returns
 *   The result of applying the function to the given arguments.
 *
 * @sideeffects
 * * The returned value must be freed with FbleReleaseValue when no longer in
 *   use.
 * * Does not take ownership of the func. Does not take ownership of the args.
 * * Prints warning messages to stderr.
 * * Prints an error message to stderr in case of error.
 * * Updates the profile with stats from the evaluation.
 */
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);

#endif // FBLE_VALUE_H_
