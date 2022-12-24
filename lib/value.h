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

/**
 * Allocates a new data type value.
 *
 * @param heap  Heap to use for allocations.
 * @param kind  Whether this is a struct or union data type.
 * @param fieldc  The number of fields of the data type.
 * @param fields  The types of the fields of the data type.
 *
 * @returns
 *   A newly allocated data type value.
 *
 * @sideeffects
 *   Allocates a new data type value that should be freed using
 *   FbleReleaseValue when it is no longer needed.
 */
FbleValue* FbleNewDataTypeValue(FbleValueHeap* heap, FbleDataTypeTag kind, size_t fieldc, FbleValue** fields);

/**
 * Allocates a new union type value.
 *
 * @param heap  Heap to use for allocations.
 * @param fieldc  The number of fields of the union type.
 * @param fields  The types of the fields of the union type.
 *
 * @returns A newly allocated union type value.
 *
 * @sideeffects
 *   Allocates a new union type value that should be freed using
 *   FbleReleaseValue when it is no longer needed.
 */
FbleValue* FbleNewUnionTypeValue(FbleValueHeap* heap, size_t fieldc, FbleValue** fields);

#endif // FBLE_INTERNAL_VALUE_H_
