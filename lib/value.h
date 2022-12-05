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

#endif // FBLE_INTERNAL_VALUE_H_
