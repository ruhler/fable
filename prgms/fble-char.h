
#ifndef FBLE_CHAR_H_
#define FBLE_CHAR_H_

#include "fble-value.h"   // for FbleValue, etc.

// FbleCharValueRead --
//   Read a character from an FbleValue of type /Char%.Char@
//
// Inputs:
//   c - the value of the character.
//
// Results:
//   The value x represented as a c char.
//
// Side effects:
//   None
char FbleCharValueRead(FbleValue* c);

// FbleCharValueWrite --
//   Write a character to an FbleValue of type /Char%.Char@.
//   The character '?' is used for any characters not currently supported by
//   the /Char%.Char@ type.
//
// Inputs:
//   heap - the heap to use for allocations.
//   c - the value of the character to write.
//
// Results:
//   The FbleValue c represented as an /Char%.Char@.
//
// Side effects:
//   Allocates a value that must be freed when no longer required.
FbleValue* FbleCharValueWrite(FbleValueHeap* heap, char c);

#endif // FBLE_CHAR_H_
