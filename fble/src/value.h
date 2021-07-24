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

// FbleGenericTypeValue --
//   Used as an instance of an fble type for those types that don't need any
//   extra information at runtime.
extern FbleValue* FbleGenericTypeValue;

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
FbleValue* FbleNewFuncValue(FbleValueHeap* heap, FbleExecutable* executable, size_t profile_base_id);

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
void FbleNewLinkValue(FbleValueHeap* heap, FbleBlockId profile, FbleValue** get, FbleValue** put);

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
