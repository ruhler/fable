// fble-value.h --
//   Header file for the fble value APIs.

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include <stdbool.h>    // for bool

#include "fble-profile.h"   // for FbleProfile

typedef struct FbleHeap FbleValueHeap;

// FbleValue --
//   Abstract type for fble values.
typedef struct FbleValue FbleValue;

// FbleValueV --
//   A vector of FbleValue*
typedef struct {
  size_t size;
  FbleValue** xs;
} FbleValueV;

// FbleNewValueHeap --
//   Create a new heap for allocation of values.
//
// Results:
//   A heap that can be used to allocate values.
//
// Side effects:
//   Allocates a heap that should be freed using FbleFreeValueHeap.
FbleValueHeap* FbleNewValueHeap();

// FbleFreeValueHeap --
//   Reclaim resources associated with a value heap.
//
// Inputs:
//   heap - the heap to free.
//
// Results:
//   None.
//
// Side effects:
//   The resources associated with the given heap are freed. The heap should
//   not be used after this call.
void FbleFreeValueHeap(FbleValueHeap* heap);

// FbleRetainValue --
//   Keep the given value alive until a corresponding FbleReleaseValue is
//   called.
//
// Inputs:
//   value - The value to retain.
//
// Side effects:
//   Causes the value to be retained until a corresponding FbleReleaseValue
//   calls is made on the value. FbleReleaseValue must be called when the
//   value is no longer needed.
void FbleRetainValue(FbleValueHeap* heap, FbleValue* src);

// FbleReleaseValue --
//
//   Decrement the strong reference count of a value and free the resources
//   associated with that value if it has no more references.
//
// Inputs:
//   heap - The heap the value was allocated with.
//   value - The value to decrement the strong reference count of. The value
//           may be NULL, in which case no action is performed.
//
// Results:
//   None.
//
// Side effect:
//   Decrements the strong reference count of the value and frees resources
//   associated with the value if there are no more references to it.
void FbleReleaseValue(FbleValueHeap* heap, FbleValue* value);

// FbleValueAddRef --
//   Notify the value heap of a new reference from src to dst.
//
// Inputs:
//   heap - the heap the values are allocated on.
//   src - the source of the reference.
//   dst - the destination of the reference. Must not be NULL.
//
// Side effects:
//   Causes the dst value to be retained for at least as long as the src value.
void FbleValueAddRef(FbleValueHeap* heap, FbleValue* src, FbleValue* dst);

// FbleValueFullGc --
//   Perform a full garbage collection on the value heap. Frees any
//   unreachable objects currently on the heap.
//
// This is an expensive operation intended for test and debug purposes.
//
// Inputs:
//   heap - the heap to perform gc on.
//
// Side effects:
//   Frees any unreachable objects currently on the heap.
void FbleValueFullGc(FbleValueHeap* heap);

// FbleNewStructValue --
//   Create a new struct value with given arguments.
//
// Inputs:
//   heap - The heap to allocate the value on.
//   argc - The number of fields in the struct value.
//   args - argc FbleValue arguments to the struct value. Args are borrowed,
//          and may be NULL.
//
// Results:
//   A newly allocated struct value with given args.
//
// Side effects:
//   The returned struct value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

// FbleNewStructValue_ --
//   Create a new struct value with given arguments.
//
// Inputs:
//   heap - The heap to allocate the value on.
//   argc - The number of fields in the struct value.
//   ... - argc FbleValue arguments to the struct value. Args are borrowed,
//         and may be NULL.
//
// Results:
//   A newly allocated struct value with given args.
//
// Side effects:
//   The returned struct value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, ...);

// FbleStructValueAccess --
//   Gets the given field value of a struct value.
//
// Inputs:
//   object - the struct value object to get the field value of.
//   field - the field to access.
//
// Results:
//   The value of the given field of the struct value object. The returned
//   value will stay alive as long as the given struct value. The caller is
//   responsible for calling FbleRetainValue on the returned value to keep it
//   alive longer if necessary.
//
// Side effects:
//   Behavior is undefined if the object is not a struct value or the field
//   is invalid.
FbleValue* FbleStructValueAccess(FbleValue* object, size_t field);

// FbleNewUnionValue --
//   Create a new union value with given tag and argument.
//
// Inputs:
//   heap - The heap to allocate the value on.
//   tag - The tag of the union value.
//   arg - The argument of the union value. Borrowed.
//
// Results:
//   A newly allocated union value with given tag and arg.
//
// Side effects:
//   The returned union value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewUnionValue(FbleValueHeap* heap, size_t tag, FbleValue* arg);

// FbleNewEnumValue --
//   Create a new union value with given tag. Convenience function for
//   creating unions with value of type *().
//
// Inputs:
//   heap - The heap to allocate the value on.
//   tag - The tag of the union value.
//
// Results:
//   A newly allocated union value with given tag and arg.
//
// Side effects:
//   The returned union value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag);

// FbleUnionValueTag --
//   Gets the tag of a union value.
//
// Inputs:
//   object - the union value object to get the tag of.
//
// Results:
//   The tag of the union value object.
//
// Side effects:
//   Behavior is undefined if the object is not a union value.
//   
size_t FbleUnionValueTag(FbleValue* object);

// FbleUnionValueAccess --
//   Gets the argument of a union value.
//
// Inputs:
//   object - the union value object to get the argument of.
//
// Results:
//   The argument of the union value object.
//
// Side effects:
//   Behavior is undefined if the object is not a union value.
//   
FbleValue* FbleUnionValueAccess(FbleValue* object);

// FbleSimpleFunc --
//   Typedef for a C function used to implement a simple fble function.
//
// Note: don't do anything weird in the implementation of the function like
// calling back into fble functions.
//
// Inputs:
//   heap - the heap to allocate values on.
//   args - array of arguments passed to the function. The number of args is
//          specified in FbleNewSimpleFuncValue.
//
// Returns:
//   The result of executing the function, or NULL to abort execution of the
//   program.
//
// See also FbleNewSimpleFuncValue.
typedef FbleValue* (*FbleSimpleFunc)(FbleValueHeap* heap, FbleValue** args);

// FbleNewSimpleFuncValue --
//   Create an fble function value from a C function.
//
// TODO: After removing support for processes, figure out how to make this
// more general. For now we don't allow userdata, static variables, or any of
// that sort of thing.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   argc - the number of function arguments.
//   impl - the implementation of the function.
//   profile - a profile block id to use when the function is called.
//
// Results:
//   A newly allocated function value.
//
// Side effects:
//   The returned function value must be freed using FbleReleaseValue when no
//   longer in use.
FbleValue* FbleNewSimpleFuncValue(FbleValueHeap* heap, size_t argc, FbleSimpleFunc impl, FbleBlockId profile);

// FbleEval --
//   Evaluate a linked program. The program is assumed to be a zero argument
//   function as returned by FbleLink.
//
// Inputs:
//   heap - The heap to use for allocating values.
//   program - The program to evaluate.
//   profile - the profile to update. May be NULL to disable profiling.
//
// Results:
//   The value of the evaluated program, or NULL in case of a runtime error in
//   the program.
//
// Side effects:
// * The returned value must be freed with FbleReleaseValue when no longer in
//   use.
// * Prints an error message to stderr in case of a runtime error.
// * Updates profiling information in profile based on the execution of the
//   program.
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile);

// FbleApply --
//   Apply a function to the given arguments.
//
// Inputs:
//   heap - the heap to use for allocating values.
//   func - the function to apply.
//   args - the arguments to apply the function to. length == func->argc.
//   profile - the profile to update. May be NULL to disable profiling.
//
// Results:
//   The result of applying the function to the given arguments.
//
// Side effects:
// * The returned value must be freed with FbleReleaseValue when no longer in
//   use.
// * Does not take ownership of the func. Does not take ownership of the args.
// * Prints warning messages to stderr.
// * Prints an error message to stderr in case of error.
// * Updates the profile with stats from the evaluation.
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);

#endif // FBLE_VALUE_H_
