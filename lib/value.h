// value.h --
//   Header file describing the internal interface for working with fble
//   values.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_VALUE_H_
#define FBLE_INTERNAL_VALUE_H_

#include <fble/fble-value.h>

#include <fble/fble-link.h>    // for typedef of FbleExecutable

#include "heap.h"
#include "kind.h"         // for FbleDataTypeTag

// FbleGenericTypeValue --
//   Used as an instance of an fble type for those types that don't need any
//   extra information at runtime.
extern FbleValue* FbleGenericTypeValue;

// FbleNewDataTypeValue --
//   Allocates a new data type value.
//
// Inputs:
//   heap - heap to use for allocations.
//   kind - whether this is a struct or union data type.
//   fieldc - the number of fields of the data type.
//   fields - the types of the fields of the data type.
//
// Results:
//   A newly allocated data type value.
//
// Side effects:
//   Allocates a new data type value that should be freed using
//   FbleReleaseValue when it is no longer needed.
FbleValue* FbleNewDataTypeValue(FbleValueHeap* heap, FbleDataTypeTag kind, size_t fieldc, FbleValue** fields);

// FbleNewFuncValue --
//   Allocates a new function value that runs the given executable.
//
// Inputs:
//   heap - heap to use for allocations.
//   executable - the executable to run. Borrowed.
//   profile_block_offset - the profile block offset to use for the function.
//   statics - the array of static variables for the function. The count
//             should match executable->statics.
//
// Results:
//   A newly allocated function value.
//
// Side effects:
//   Allocates a new function value that should be freed using
//   FbleReleaseValue when it is no longer needed.
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_offset, FbleValue** statics);

// FbleNewFuncValue_ --
//   Allocates a new function value that runs the given executable.
//
// Inputs:
//   heap - heap to use for allocations.
//   executable - the executable to run. Borrowed.
//   profile_block_offset - the profile block offset to use for the function.
//   ... - static variables for the function. The count should match
//         executable->statics.
//
// Results:
//   A newly allocated function value.
//
// Side effects:
// * Allocates a new function value that should be freed using
// * FbleReleaseValue when it is no longer needed.
FbleValue* FbleNewFuncValue_(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_block_offset, ...);

// FbleNewUnionTypeValue --
//   Allocates a new union type value.
//
// Inputs:
//   heap - heap to use for allocations.
//   fieldc - the number of fields of the union type.
//   fields - the types of the fields of the union type.
//
// Results:
//   A newly allocated union type value.
//
// Side effects:
//   Allocates a new union type value that should be freed using
//   FbleReleaseValue when it is no longer needed.
FbleValue* FbleNewUnionTypeValue(FbleValueHeap* heap, size_t fieldc, FbleValue** fields);

// FbleFuncValueStatics --
//   Gets the array of static variables used by a function.
//
// Inputs:
//   func - the function to get the static variables for.
//
// Results:
//   The pointer to the static variables of the function.
//
// Side effects:
//   The returned pointer is only valid for as long as the function value
//   remains alive.
FbleValue** FbleFuncValueStatics(FbleValue* func);

// FbleFuncValueProfileBaseId --
//   Gets the profile base id associated with a function.
//
// Inputs:
//   func - the function to get the profile base id for.
//
// Results:
//   The profile base id of the function.
size_t FbleFuncValueProfileBaseId(FbleValue* func);

// FbleFuncValueExecutable --
//   Gets the FbleExecutable associated with a function.
//
// Inputs:
//   func - the function to get the FbleExecutable for.
//
// Results:
//   The FbleExecutable for the function.
//
// Side effects:
//   The returned pointer is only valid for as long as the function value
//   remains alive.
FbleExecutable* FbleFuncValueExecutable(FbleValue* func);

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
FbleValue* FbleNewListValue(FbleValueHeap* heap, size_t argc, FbleValue** args);

// FbleNewListValue_ --
//   Create a new list value from the given list of arguments.
//
// Inputs:
//   heap - the heap to allocate the value on.
//   argc - the number of elements on the list.
//   ... - argc FbleValue elements to put on the list. Borrowed.
//
// Results:
//   A newly allocated list value.
//
// Side effects:
//   The returned value must be freed using FbleReleaseValue when no longer in
//   use.
FbleValue* FbleNewListValue_(FbleValueHeap* heap, size_t argc, ...);

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
FbleValue* FbleNewLiteralValue(FbleValueHeap* heap, size_t argc, size_t* args);

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
FbleValue* FbleNewRefValue(FbleValueHeap* heap);

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
bool FbleAssignRefValue(FbleValueHeap* heap, FbleValue* ref, FbleValue* value);

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
FbleValue* FbleStrictValue(FbleValue* value);

#endif // FBLE_INTERNAL_VALUE_H_
