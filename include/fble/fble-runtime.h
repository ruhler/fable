/**
 * @file fble-runtime.h
 *  API for interacting with the fble runtime.
 */

#ifndef FBLE_RUNTIME_H_
#define FBLE_RUNTIME_H_

#include <stdbool.h>    // for bool

#include "fble-function.h"    // for FbleExecutable, FbleFunction
#include "fble-module-path.h" // for FbleModulePath
#include "fble-profile.h"     // for FbleProfile

/**
 * @struct[FbleValue] An fble value.
 *  A union of value types. All values have the same initial layout as
 *  FbleValue. You can access additional fields of the value by first casting
 *  to the specific type of value, which you should know based on context.
 *
 *  Internal access to FbleValue should be limited to the runtime and backend
 *  generated code. The data layout is subject to change without notice.
 *
 *  IMPORTANT: Some fble values are packed directly in the FbleValue* pointer
 *  to save space. An FbleValue* only points to an FbleValue if the least
 *  significant two bits of the pointer are 0.
 *
 *  Struct and union values are packed as follows on a 64-bit pointer
 *  architecture. The type value is packed as a zero argument struct value.
 *
 *  @i Bit 0 is set to 1 to indicate it is a packed value.
 *  @i Bits [6:1] indicate the length of the packed content in bits.
 *  @item
 *   The remaining bits are packed content depending on if it's a struct or
 *   union.
 *  @item
 *   Packed content for a union is binary encoded tag bits, using sufficient
 *   number of bits to represent all possible tags of that particular union
 *   type. That's least significant bits of content, followed by the packed
 *   content of the argument.
 *  @item
 *   Packed content for a struct is a list of N-1 6-bit sizes giving the
 *   number of bits past the end of the struct header to reach the packed
 *   content for the ith field of the struct.
 *  @i The unused most significant bits of the packed value are always 0.
 * 
 *  For example:
 *
 *  @code[fble] @
 *   Octal@ = +(Unit@ 0, Unit@ 1, Unit@ 2, ... Unit@ 7)
 *   Str@ = +(*(Octal@ head, Str@ tail) cons, Unit@ nil)
 *   Str@ x = Str|'162'
 * 
 *  The value x is packed as the following 64 bit value, with more significant
 *  bits on the left and least significant bit on the right:
 *
 *  @code[txt] @
 *   Decimal:  1   2      3 0   6      3 0   1      3 0     31 1
 *   Binary:   1 011 000011 0 110 000011 0 001 000011 0 011111 1
 *   Label:    t ooo OOOOOO t ooo OOOOOO t ooo OOOOOO t LLLLLL P
 * 
 *   o: tag bits for an octal element.
 *   O: number of bits offset to tail field of cons struct.
 *   t: list tag: 0 for cons, 1 for nil.
 *   L: length of packed content
 *   P: pack bit
 * 
 *  A 32-bit pointer architecture uses 5 bit length and offset instead of 6
 *  bit length and offset.
 *
 *  Before recursive values are defined, they are represented as a packed
 *  undefined value with the least two significant bits set to 'b10. Any value
 *  packed with least significant bits set to 'b10 should be considered an
 *  undefined value.
 *
 *  @field[uint32_t][data] The tag of a union. Otherwise don't touch.
 *  @field[uint32_t][flags] Internal flags. Don't touch.
 */
struct FbleValue {
  uint32_t data;
  uint32_t flags;
};

/**
 * @struct[FbleStructValue] An fble struct value.
 *  Struct values may be packed. See above for the packed encoding.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[FbleValue**][fields] Field values. _base.data elements in length.
 */
typedef struct {
  FbleValue _base;
  FbleValue* fields[];
} FbleStructValue;

/**
 * @struct[FbleUnionValue] An fble union value.
 *  Union values may be packed. See above for the packed encoding.
 *
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[FbleValue*][arg] Union argument value.
 */
typedef struct {
  FbleValue _base;
  FbleValue* arg;
} FbleUnionValue;

/**
 * @struct[FbleFuncValue] An fble function value.
 *  @field[FbleValue][_base] FbleValue base class.
 *  @field[FbleFunction][function] Function information.
 *  @field[FbleValue**][statics] Storage location for static variables.
 */
typedef struct {
  FbleValue _base;
  FbleFunction function;
  FbleValue* statics[];
} FbleFuncValue;

/**
 * @struct[FbleRuntime] A context for running fble code.
 *  Parts of the FbleRuntime data structure are exposed for use by backends.
 *  Other parts are not exposed.
 *
 *  To perform a tail call, set tail_call_argc to the number of arguments to
 *  the call. Set tail_call_buffer[0] to the function to call. Add the
 *  arguments to tail_call_buffer starting at index 1. Then return
 *  tail_call_sentinel.
 *
 *  @field[FbleValue*][tail_call_sentinel]
 *   Value to return to indicate a tail call should be performed.
 *  @field[FbleValue**][tail_call_buffer]
 *   Buffer allocated for tail calls. When used, contains the tail call
 *   function at index 0, followed by tail_call_argc worth of arguments.
 *  @field[size_t][tail_call_argc]
 *   Number of tail call arguments, not including the tail call function.
 *
 *  @field[FbleProfile*][profile]
 *   The profile for the code loaded into and run in the runtime instance.
 */
struct FbleRuntime {
  FbleValue* tail_call_sentinel;
  FbleValue** tail_call_buffer;
  size_t tail_call_argc;
  FbleProfile* profile;

  // Additional internal fields follow.
};

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
 * @func[FbleNewRuntime] Creates a new FbleRuntime.
 *  The runtime context tracks allocated values organized as a stack of
 *  frames. New values are allocated to the top frame on the stack. Values
 *  are automatically freed when their frame is popped from the stack. See
 *  FblePushFrame and FblePopFrame for more info.
 *
 *  @returns FbleRuntime*
 *   A context for allocating and evaluating values.
 *
 *  @sideeffects
 *   Allocates a runtime context that should be freed using FbleFreeRuntime.
 */
FbleRuntime* FbleNewRuntime();

/**
 * @func[FbleFreeRuntime] Frees an FbleRuntime.
 *  @arg[FbleRuntime*][runtime] The runtime to free.
 *
 *  @sideeffects
 *   The resources associated with the runtime context are freed. The runtime
 *   should not be used after this call.
 */
void FbleFreeRuntime(FbleRuntime* runtime);

/**
 * @func[FblePushFrame] Adds a new frame to the heap.
 *  Values allocated after this call will be allocated to a new frame, staying
 *  alive until the matching call to FblePopFrame.
 *
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @sideeffects
 *   Pushes a new frame on the runtime that should be freed with a call to
 *   FblePopFrame when no longer needed.
 */
void FblePushFrame(FbleRuntime* runtime);

/**
 * @func[FblePopFrame] Pops a frame from the heap.
 *  All values allocated on the frame are freed. The returned value is moved
 *  to the parent frame.
 *
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[FbleValue*][value] Value to return when popping the frame.
 *  @returns[FbleValue*]
 *   A copy of @a[value] moved to the top frame after the frame is popped.
 *  @sideeffects
 *   Frees all values allocated on the top frame. You must not reference any
 *   values allocated to that frame after this call.
 */
FbleValue* FblePopFrame(FbleRuntime* runtime, FbleValue* value);

/**
 * @func[FbleRegisterForeignValue] Registers a foreign value.
 *  @arg[FbleRuntime*][runtime] The runtime context to register the value with.
 *  @arg[FbleForeign*][foreign]
 *   The foreign value implementation to register. The lifetime of the foreign
 *   value implementation must persist beyond that of the FbleRuntime.
 *  @sideeffects
 *   Registers the foreign value implementation with the runtime.
 */
void FbleRegisterForeignValue(FbleRuntime* runtime, FbleForeign* foreign);

/**
 * @func[FbleLookupForeignValue] Looks up a foreign value.
 *  @arg[FbleRuntime*] runtime
 *   The runtime context.
 *  @arg[FbleModulePath*] path
 *   Module path associated with the foreign value.
 *  @arg[const char*] name
 *   The name of the foreign value.
 *
 *  @returns FbleForeign*
 *   A pointer to the foreign value. NULL if the foreign value could not be
 *   found. The pointer is borrowed and assumed to have a lifetime at least as
 *   long as the lifetime of the FbleRuntime.
 *
 *  @sideeffects
 *   None.
 */
FbleForeign* FbleLookupForeignValue(FbleRuntime* runtime, FbleModulePath* path, const char* name);

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
 *  @arg[FbleRuntime*][runtime] The runtime context.
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
FbleValue* FbleNewStructValue(FbleRuntime* runtime, size_t argc, FbleValue** args);

/**
 * @func[FbleNewStructValue_] Creates a new struct value using varargs.
 *  @arg[FbleRuntime*][runtime] The runtime context.
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
FbleValue* FbleNewStructValue_(FbleRuntime* runtime, size_t argc, ...);

/**
 * @func[FbleStructValueField] Gets a field of a struct value.
 *  @arg[FbleValue*][object] The struct value object to get the field value of.
 *  @arg[size_t    ][fieldc] The number of fields in the type for this struct.
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
FbleValue* FbleStructValueField(FbleValue* object, size_t fieldc, size_t field);

/**
 * @func[FbleNewUnionValue] Creates a new union value.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[size_t        ][tagwidth]
 *   The number of bits needed to store a tag of this union type.
 *  @arg[size_t        ][tag ] The tag of the union value.
 *  @arg[FbleValue*    ][arg ] The argument of the union value. Borrowed.
 *
 *  @returns FbleValue*
 *   A newly allocated union value with given tag and arg.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewUnionValue(FbleRuntime* runtime, size_t tagwidth, size_t tag, FbleValue* arg);

/**
 * @func[FbleNewEnumValue] Creates a new enum value.
 *  Convenience function for creating unions with value of type *().
 *
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[size_t        ][tagwidth ] The number of bits needed for the tag.
 *  @arg[size_t        ][tag ] The tag of the union value.
 *
 *  @returns FbleValue*
 *   A newly allocated union value with given tag and arg.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewEnumValue(FbleRuntime* runtime, size_t tagwidth, size_t tag);

/**
 * @func[FbleUnionValueTag] Gets the tag of a union value.
 *  @arg[FbleValue*][object] The union value object to get the tag of.
 *  @arg[size_t][tagwidth] The number of bits needed for the tag.
 *
 *  @returns size_t
 *   The tag of the union value object. Returns -1 if the union value is
 *   undefined.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
size_t FbleUnionValueTag(FbleValue* object, size_t tagwidth);

/**
 * @func[FbleUnionValueArg] Gets the argument of a union value.
 *  @arg[FbleValue*][object] The union value object to get the argument of.
 *  @arg[size_t][tagwidth] Number of bits for the tag.
 *
 *  @returns FbleValue*
 *   The argument of the union value object. Returns NULL if the union value
 *   is undefined.
 *
 *  @sideeffects
 *   Behavior is undefined if the object is not a union value.
 */
FbleValue* FbleUnionValueArg(FbleValue* object, size_t tagwidth);

/**
 * Sentinel value indicating wrong field in FbleUnionValueField function.
 *
 * We use the value 0x2 to be distinct from NULL and packed values.
 */
#define FbleWrongUnionTag ((FbleValue*) 0x2)

/**
 * @func[FbleUnionValueField] Gets a field of a union value.
 *  @arg[FbleValue*][object] The union value object to get the field of.
 *  @arg[size_t][tagwidth] The number of bits for the union tag.
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
FbleValue* FbleUnionValueField(FbleValue* object, size_t tagwidth, size_t field);

/**
 * @func[FbleNewListValue] Creates an fble list value.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[size_t        ][argc] The number of elements on the list.
 *  @arg[FbleValue**   ][args] The elements to put on the list. Borrowed.
 *
 *  @returns FbleValue*
 *   A newly allocated list value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleNewListValue(FbleRuntime* runtime, size_t argc, FbleValue** args);

/**
 * @func[FbleNewListValue_] Creates an fble list value using varargs.
 *  @arg[FbleRuntime*][runtime] The runtime context.
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
FbleValue* FbleNewListValue_(FbleRuntime* runtime, size_t argc, ...);

/**
 * @func[FbleNewFuncValue] Creates an fble function value.
 *  @arg[FbleRuntime*] runtime
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
FbleValue* FbleNewFuncValue(FbleRuntime* runtime, FbleExecutable* executable, size_t profile_block_id, FbleValue** statics);

/**
 * @func[FbleNewForeignValue] Creates an fble foreign value.
 *  @arg[FbleRuntime*] runtime
 *   Heap to use for allocations.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *  @arg[FbleForeign*] value
 *   The foreign value implementation.
 *  @arg[size_t] profile_block_id
 *   The profile block id to use for the foreign value implementation.
 *
 *  @returns FbleValue*
 *   A newly allocated foreign value. If the foreign value implementation
 *   takes one or more arguments, this returns a function value wrapper around
 *   the foreign value. If the foreign value implementation takes zero
 *   arguments, this returns the result of executing that foreign value
 *   implementation right away.
 *
 *  @sideeffects
 *   Allocates a value on the heap. Anything else a zero-argument foreign
 *   value implementation might do.
 */
FbleValue* FbleNewForeignValue(FbleRuntime* runtime, FbleProfileThread* profile, FbleForeign* foreign, size_t profile_block_id);

/**
 * @func[FbleEval] Evaluates a linked program.
 *  The program is assumed to be a zero argument function as returned by
 *  FbleLink.
 * 
 *  @arg[FbleRuntime*][runtime   ] The runtime context.
 *  @arg[FbleValue*    ][program] The program to evaluate.
 * 
 *  @returns FbleValue*
 *   The value of the evaluated program, or @l[NULL] in case of a runtime
 *   error in the program.
 * 
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @i Prints an error message to stderr in case of a runtime error.
 *   @item
 *    Updates profiling information in the runtime profile based on the
 *    execution of the program.
 */
FbleValue* FbleEval(FbleRuntime* runtime, FbleValue* program);

/**
 * @func[FbleApply] Applies an fble function to arguments.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[FbleValue*][func] The function to apply.
 *  @arg[size_t][argc] The number of args to apply.
 *  @arg[FbleValue**][args] The arguments to apply the function to.
 *
 *  @returns FbleValue*
 *   The result of applying the function to the given arguments.
 *
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @i Prints warning messages to stderr.
 *   @i Prints an error message to stderr in case of error.
 *   @i Updates the runtime profile with stats from the evaluation.
 */
FbleValue* FbleApply(FbleRuntime* runtime, FbleValue* func, size_t argc, FbleValue** args);

/**
 * @func[FbleDeclareRecursiveValues] Declare values intended to be recursive.
 *  Used for recursive let declarations. This allocates and returns a struct
 *  value with a separate field for each recursive value to declare. The caller
 *  can access the field values to get a handle to the value to use in its
 *  recursive definition. The FbleDefineRecursiveValues function should be
 *  called to complete the definition of the recursive values.
 *
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[size_t][n] The number of values to declare.
 *
 *  @returns FbleValue*
 *   A newly allocated struct value with n fields corresponding to the n
 *   declared values.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
FbleValue* FbleDeclareRecursiveValues(FbleRuntime* runtime, size_t n);

/**
 * @func[FbleDefineRecursiveValues] Define values intended to be recursive.
 *  Calls to FbleDeclareRecursiveValues and FbleDefineRecursiveValues should be
 *  done in last in first out order. The @a[decl] argument should be the value
 *  returned from FbleDeclareRecursiveValues. The @a[defn] argument should be
 *  a struct values whose fields hold the definitions of the recursive values.
 *
 *  @arg[FbleRuntime*][runtime  ] The runtime context.
 *  @arg[FbleValue*   ][decl] The declaration of the reference values.
 *  @arg[FbleValue*   ][defn] The definition of the reference values.
 *
 *  @returns size_t
 *   0 on success. i+1 if the ith assignment would produce a vacuous value.
 *
 *  @sideeffects
 *   @i Binds the recursive values into loops.
 *   @i Updates the fields of @a[decl] to have the finalized values.
 *   @i Invalidates all handles to the original declared value standins.
 */
size_t FbleDefineRecursiveValues(FbleRuntime* runtime, FbleValue* decl, FbleValue* defn);

/**
 * @func[FbleNewNativeValue] Creates a GC managed native allocation.
 *  The given on_free function will be called on the user data when the value
 *  is no longer reachable. Native values must not reference other values
 *  through the user data.
 *
 *  @arg[FbleRuntime*][runtime] The runtime context.
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
FbleValue* FbleNewNativeValue(FbleRuntime* runtime,
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
 * @func[FbleReportRuntimeError] Reports a runtime error.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[FbleLoc][loc] Location of the error.
 *  @arg[FbleFunction*][func] Function where the error occured.
 *  @arg[const char*][msg]
 *   Error message. Maybe be NULL to indicate a location in the stack.
 *  @sideeffects Outputs an error message.
 */
void FbleReportRuntimeError(FbleRuntime* runtime, FbleLoc loc, FbleFunction* func, const char* msg);

/**
 * @func[FbleFullGc] Performs a full garbage collection.
 *  Frees any unreachable objects currently on the runtime.
 *
 *  This is an expensive operation intended only for test and debug purposes.
 *
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *
 *  @sideeffects
 *   Frees any unreachable objects currently on the heap.
 */
void FbleFullGc(FbleRuntime* runtime);

#endif // FBLE_RUNTIME_H_
