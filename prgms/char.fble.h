
#ifndef FBLE_CHAR_FBLE_H_
#define FBLE_CHAR_FBLE_H_

#include "fble-value.h"   // for FbleValue, etc.

// FbleNewCharValue --
//   Create an FbleValue of type /Char%.Char@.
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
FbleValue* FbleNewCharValue(FbleValueHeap* heap, char c);

// FbleCharValueAccess --
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
char FbleCharValueAccess(FbleValue* c);


#endif // FBLE_CHAR_FBLE_H_
