/**
 * @file fble-value.h
 *  API for interacting with fble values.
 */

#ifndef FBLE_VALUE_H_
#define FBLE_VALUE_H_

#include <stdbool.h>    // for bool

#include "fble-profile.h"   // for FbleProfile

// Forward references for types defined in fble-function.h
typedef struct FbleExecutable FbleExecutable;
typedef struct FbleFunction FbleFunction;

/**
 * @struct[FbleValueHeap] Memory heap for allocating fble values. @@
 */
typedef struct FbleValueHeap FbleValueHeap;

/**
 * @struct[FbleValue] An fble value. @@
 */
typedef struct FbleValue FbleValue;

/**
 * @struct[FbleValueV] Vector of FbleValue
 *  @field[size_t][size] Number of elements.
 *  @field[FbleValue**][xs] Elements.
 */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleValue** xs;   /**< Elements .*/
} FbleValueV;

/**
 * @func[FbleNewValueHeap] Creates a new FbleValueHeap.
 *  The heap is organized as a stack of frames. New values are allocated to
 *  the top frame on the stack. Values are automatically freed when their
 *  frame is popped from the stack. See FblePushFrame and FblePopFrame for
 *  more info.
 *
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
 * @func[FblePushFrame] Adds a new frame to the heap.
 *  Values allocated after this call will be allocated to a new frame, staying
 *  alive until the matching call to FblePopFrame.
 *
 *  @arg[FbleValueHeap*][heap] The heap.
 *  @sideeffects
 *   Pushes a new frame on the heap that should be freed with a call to
 *   FblePopFrame when no longer needed.
 */
void FblePushFrame(FbleValueHeap* heap);

/**
 * @func[FblePopFrame] Pops a frame from the heap.
 *  All values allocated on the frame are freed. The returned value is moved
 *  to the parent frame.
 *
 *  @arg[FbleValueHeap*][heap] The heap.
 *  @arg[FbleValue*][value] Value to return when popping the frame.
 *  @returns[FbleValue*]
 *   A copy of @a[value] moved to the top frame after the frame is popped.
 *  @sideeffects
 *   Frees all values allocated on the top frame. You must not reference any
 *   values allocated to that frame after this call.
 */
FbleValue* FblePopFrame(FbleValueHeap* heap, FbleValue* value);

/**
 * @value[FbleGenericTypeValue] FbleValue instance for types.
 *  Used as an instance of an fble type for those types that don't need any
 *  extra information at runtime.
 *
 *  @type FbleValue*
 */
extern FbleValue* FbleGenericTypeValue;

/**
 * @func[FbleNewStructValue] Creates a new struct value.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of fields in the struct value.
 *  @arg[FbleValue**   ][args]
 *   @@a[argc] arguments to the struct value. Args are borrowed. They must not
 *   be NULL.
 *
 *  @returns[FbleValue*]
 *   A newly allocated struct value with given args.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewStructValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

/**
 * @func[FbleNewStructValue_] Creates a new struct value using varargs.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of fields in the struct value.
 *  @arg[...           ][    ]
 *   @@a[argc] FbleValue arguments to the struct value. Args are borrowed.
 *   They must not be NULL.
 *
 *  @returns FbleValue*
 *   A newly allocated struct value with given args.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewStructValue_(FbleValueHeap* heap, size_t argc, ...);

/**
 * @func[FbleStructValueField] Gets a field of a struct value.
 *  @arg[FbleValue*][object] The struct value object to get the field value of.
 *  @arg[size_t    ][field ] The field to access.
 *
 *  @returns FbleValue*
 *   The value of the given field of the struct value object. The returned
 *   value will stay alive as long as the given struct value.
 *   Returns NULL if the struct value is undefined.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a struct value or the field
 *   is invalid.
 */
FbleValue* FbleStructValueField(FbleValue* object, size_t field);

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
 *   Allocates a value on the heap.
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
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewEnumValue(FbleValueHeap* heap, size_t tag);

/**
 * @func[FbleUnionValueTag] Gets the tag of a union value.
 *  @arg[FbleValue*][object] The union value object to get the tag of.
 *
 *  @returns size_t
 *   The tag of the union value object. Returns -1 if the union value is
 *   undefined.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
size_t FbleUnionValueTag(FbleValue* object);

/**
 * @func[FbleUnionValueArg] Gets the argument of a union value.
 *  @arg[FbleValue*][object] The union value object to get the argument of.
 *
 *  @returns FbleValue*
 *   The argument of the union value object. Returns NULL if the union value
 *   is undefined.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
FbleValue* FbleUnionValueArg(FbleValue* object);

/**
 * Sentinel value indicating wrong field in FbleUnionValueField function.
 *
 * We use the value 0x2 to be distinct from NULL and packed values.
 */
#define FbleWrongUnionTag ((FbleValue*) 0x2)

/**
 * @func[FbleUnionValueField] Gets a field of a union value.
 *  @arg[FbleValue*][object] The union value object to get the field of.
 *  @arg[size_t][field] The field to get.
 *
 *  @returns FbleValue*
 *   @i The field of the union value object.
 *   @i NULL if the union value is undefined.
 *   @i FbleWrongUnionTag if it is the wrong field.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
FbleValue* FbleUnionValueField(FbleValue* object, size_t field);

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
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

/**
 * @func[FbleNewListValue_] Creates an fble list value using varargs.
 *  @arg[FbleValueHeap*][heap] The heap to allocate the value on.
 *  @arg[size_t        ][argc] The number of elements on the list.
 *  @arg[...           ][    ]
 *   @@a[argc] FbleValue elements to put on the list. Borrowed.
 *
 *  @returns FbleValue*
 *   A newly allocated list value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
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
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args);

/**
 * @func[FbleNewFuncValue] Creates an fble function value.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleExecutable*] executable
 *   The executable to run. Borrowed.
 *  @arg[size_t] profile_block_id
 *   The profile block id to use for the function.
 *  @arg[FbleValue**] statics
 *   The array of static variables for the function. The count should match
 *   executable->statics.
 *
 *  @returns FbleValue*
 *   A newly allocated function value.
 *
 *  @sideeffects
 *   Allocates a function value on the heap.
 */
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_id, FbleValue** statics);

/**
 * @func[FbleFuncValueFunction] Gets the info from a function value.
 *  @arg[FbleValue*][value] The value to get the function info of.
 *
 *  @returns FbleFunction*
 *   The function info, or NULL if the function is undefined. The returned
 *   function info is only valid for as long as the value stays alive.
 *
 *  @sideeffects
 *   Behavior is undefined if the value is not a function.
 */
FbleFunction* FbleFuncValueFunction(FbleValue* value);

/**
 * @func[FbleEval] Evaluates a linked program.
 *  The program is assumed to be a zero argument function as returned by
 *  FbleLink.
 * 
 *  @arg[FbleValueHeap*][heap   ] The heap to use for allocating values.
 *  @arg[FbleValue*    ][program] The program to evaluate.
 *  @arg[FbleProfile*  ][profile] The profile to update. Must not be NULL.
 * 
 *  @returns FbleValue*
 *   The value of the evaluated program, or @l[NULL] in case of a runtime
 *   error in the program.
 * 
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @i Prints an error message to stderr in case of a runtime error.
 *   @item
 *    Updates profiling information in profile based on the execution of the
 *    program.
 */
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile);

/**
 * @func[FbleApply] Applies an fble function to arguments.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocating values.
 *  @arg[FbleValue*][func] The function to apply.
 *  @arg[size_t][argc] The number of args to apply.
 *  @arg[FbleValue**][args] The arguments to apply the function to.
 *  @arg[FbleProfile*][profile] The profile to update. Must not be NULL.
 *
 *  @returns FbleValue*
 *   The result of applying the function to the given arguments.
 *
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @i Prints warning messages to stderr.
 *   @i Prints an error message to stderr in case of error.
 *   @i Updates the profile with stats from the evaluation.
 */
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);

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
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewRefValue(FbleValueHeap* heap);

/**
 * @func[FbleAssignRefValue] Sets the value pointed to by a ref value.
 *  The ref value must be allocated on the top frame of the stack when calling
 *  this function, otherwise behavior is undefined.
 *
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
 * @func[FbleNewNativeValue] Creates a GC managed native allocation.
 *  The given on_free function will be called on the user data when the value
 *  is no longer reachable. Native values must not reference other values
 *  through the user data.
 *
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[void*][data] The user data to store on the native value.
 *  @arg[void (*)(void*)][on_free]
 *   Function called just before freeing the allocated native data. May be
 *   NULL to indicate that nothing should be done on free.
 *
 *  @returns[FbleValue*] The newly allocated native value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewNativeValue(FbleValueHeap* heap,
    void* data, void (*on_free)(void* data));

/**
 * @func[FbleNativeValueData] Gets a native value's user data.
 *  @arg[FbleValue*][value] The value to get the native allocation for.
 *  @returns[void*] The user data associated with the value.
 *  @sideeffects
 *   Behavior is undefined if the value is not a native value.
 */
void* FbleNativeValueData(FbleValue* value);

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

#endif // FBLE_VALUE_H_
