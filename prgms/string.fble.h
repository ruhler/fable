
#ifndef FBLE_STRING_FBLE_H_
#define FBLE_STRING_FBLE_H_

#include "fble-value.h"

// FbleNewStringValue --
//   Convert a C string to an Fble /String%.String@.
//
// Inputs:
//   heap - the heap to use for allocations.
//   str - the string to convert.
//
// Results:
//   A newly allocated fble /String%.String@ with the contents of str.
//
// Side effects:
//   Allocates an FbleValue that should be freed with FbleReleaseValue when no
//   longer needed.
FbleValue* FbleNewStringValue(FbleValueHeap* heap, const char* str);

// FbleStringValueAccess --
//   Convert an /String%.String@ FbleValue to a c string.
//
// Inputs:
//   str - the /String%.String@ to convert.
//
// Results:
//   A newly allocated nul terminated c string with the contents of str.
//
// Side effects:
//   The caller should call FbleFree on the returned string when it is no
//   longer needed.
char* FbleStringValueAccess(FbleValue* str);

#endif // FBLE_STRING_FBLE_H_

