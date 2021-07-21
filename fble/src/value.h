// value.h --
//   Header file describing the internal interface for working with fble
//   values.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_VALUE_H_
#define FBLE_INTERNAL_VALUE_H_

#include "fble-link.h"    // for typedef of FbleExecutable
#include "fble-value.h"
#include "execute.h"      // for FbleRunFunction
#include "heap.h"

// FbleValueTag --
//   A tag used to distinguish among different kinds of FbleUnpackedValue.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_LINK_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
} FbleValueTag;

// FbleUnpackedValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleUnpackedValue. The tag can be used to determine what kind
//   of value this is to get access to additional fields of the value by first
//   casting to that specific type of value.
struct FbleUnpackedValue {
  FbleValueTag tag;
};

// FbleGenericTypeValue --
//   Used as an instance of an fble type for those types that don't need any
//   extra information at runtime.
extern FbleValue FbleGenericTypeValue;

// FbleStructValue -- Internal to value.c
// FbleUnionValue -- Internal to value.c
// FbleLinkValue -- Internal to value.c
// FblePortValue -- Internal to value.c
// FbleRefValue -- Internal to value.c

// FbleFuncValue -- FBLE_FUNC_VALUE
//
// Fields:
//   executable - The code for the function.
//   profile_base_id - An offset to use for profile blocks referenced from this
//                     function.
//   statics - static variables captured by the function.
//             Size is executable->statics
//
// Note: Function values are used for both pure functions and processes. We
// don't distinguish between the two at runtime, except that
// executable->args == 0 suggests this is for a process instead of a function.
struct FbleFuncValue {
  FbleUnpackedValue _base;
  FbleExecutable* executable;
  size_t profile_base_id;
  FbleValue statics[];
};

// FbleProcValue -- FBLE_PROC_VALUE
//   A proc value is represented as a function that takes no arguments.
#define FBLE_PROC_VALUE FBLE_FUNC_VALUE
typedef FbleFuncValue FbleProcValue;

// FbleNewValue --
//   Allocate a new value of the given type.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//
// Results:
//   The newly allocated value. The value does not have its tag initialized.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define FbleNewValue(heap, T) ((T*) FbleNewHeapObject(heap, sizeof(T)))

// FbleNewValueExtra --
//   Allocate a new value of the given type with some extra space.
//
// Inputs:
//   heap - the heap to allocate the value on
//   T - the type of the value
//   size - the number of bytes of extra space to include in the allocated
//   object.
//
// Results:
//   The newly allocated value with extra space.
//
// Side effects:
//   Allocates a value that should be released when it is no longer needed.
#define FbleNewValueExtra(heap, T, size) ((T*) FbleNewHeapObject(heap, sizeof(T) + size))

// FbleValueIsUnpacked --
//   Test whether a value is unpacked.
//
// Inputs:
//   FbleValue value - the value to test.
//
// Results:
//   bool true if the value is unpacked, false otherwise.
//
// Note:
//   This is a macro instead of a function because I assume (without
//   verification) that making it a function would introduce a silly amount of
//   overhead.
#define FbleValueIsUnpacked(value) ((value.packed & 1) == 0)

// FbleWrapUnpackedValue --
//   Wrap an FbleUnpackedValue* as an FbleValue.
FbleValue FbleWrapUnpackedValue(FbleUnpackedValue* value);

// FbleNewFuncValue --
//   Allocates a new FbleFuncValue that runs the given executable.
//
// Inputs:
//   heap - heap to use for allocations.
//   executable - the executable to run. Borrowed.
//   profile_base_id - the profile_base_id to use for the function.
//
// Results:
//   A newly allocated FbleFuncValue that is fully initialized except for
//   statics.
//
// Side effects:
//   Allocates a new FbleFuncValue that should be freed using FbleReleaseValue
//   when it is no longer needed.
FbleFuncValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id);

// FbleNewLinkValue --
//   Create a new link value.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   profile - the id of the first of three consecutive profile block ids
//             refereing to the profile blocks to associate with get, put apply, and put execute.
//   get - output variable set to the allocated get side of the link.
//   put - output variable set to the allocated put side of the link.
//
// Side effects:
// * Allocates new get and put values connected together with a link.
// * The returned get and put values must be freed using FbleReleaseValue when
//   no longer in use.
void FbleNewLinkValue(FbleValueHeap* heap, FbleBlockId profile, FbleValue* get, FbleValue* put);

// FbleNewListValue --
//   Create a new list value from the given list of arguments.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   argc - the number of elements on the list.
//   args - the elements to put on the list. Borrowed.
//
// Results:
//   A newly allocated list value.
//
// Side effects:
//   The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue* args);

// FbleNewLiteralValue --
//   Create a new literal value from the given list of letters.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   argc - the number of letters in the literal.
//   args - the tags of the letters in the literal.
//
// Results:
//   A newly allocated literal value.
//
// Side effects:
//   The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args);

// FbleNewRefValue --
//   Create a new reference value.
//
// Inputs:
//   heap - the heap to allocate the value on.
//
// Results:
//   A newly allocated reference value.
//
// Side effects:
//   The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue FbleNewRefValue(FbleValueHeap* heap);

// FbleAssignRefValue
//   Assign a RefValue to point to another value.
//
// Inputs:
//   heap - the heap to use for allocations
//   ref - the reference to assign to
//   value - the value to assign to the reference.
//
// Results:
//   true on success. false if the assignment would produce a vacuous
//   value.
//
// Side effects:
//   Updates ref to point to value.
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue ref, FbleValue value);

// FbleStrictValue --
//   Get the strict value associated with the given value, which will either
//   be the value itself, or the dereferenced value if the value is a
//   reference.
//
// Inputs:
//   value - the value to get the strict version of.
//
// Results:
//   The value with all layers of reference indirection removed. NULL if the
//   value is a reference that has no value yet.
//
// Side effects:
//   None.
FbleValue FbleStrictValue(FbleValue value);

#endif // FBLE_INTERNAL_VALUE_H_
