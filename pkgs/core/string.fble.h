
#ifndef FBLE_CORE_STRING_FBLE_H_
#define FBLE_CORE_STRING_FBLE_H_

#include <fble/fble-value.h>

// FbleNewStringValue --
//   Convert a C string to an Fble /Core/String%.String@.
//
// Inputs:
//   heap - the heap to use for allocations.
//   str - the string to convert.
//
// Results:
//   A newly allocated fble /Core/String%.String@ with the contents of str.
//
// Side effects:
//   Allocates an FbleValue on the heap.
FbleValue* FbleNewStringValue(FbleValueHeap* heap, const char* str);

// FbleStringValueAccess --
//   Convert an /Core/String%.String@ FbleValue to a c string.
//
// Inputs:
//   str - the /Core/String%.String@ to convert.
//
// Results:
//   A newly allocated nul terminated c string with the contents of str.
//
// Side effects:
//   The caller should call FbleFree on the returned string when it is no
//   longer needed.
char* FbleStringValueAccess(FbleValue* str);

// FbleDebugTrace --
//   Prints a /Core/String%.String@ FbleValue to stderr.
//
// Inputs:
//   str - the /Core/String%.String@ to print.
//
// Side effects:
//   Prints the string to stderr.
void FbleDebugTrace(FbleValue* str);

#endif // FBLE_STRING_FBLE_H_

