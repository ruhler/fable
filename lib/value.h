/**
 * @file value.h
 * Header for FbleValue.
 */

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

#endif // FBLE_INTERNAL_VALUE_H_
