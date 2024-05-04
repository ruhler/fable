/**
 * @file char.fble.h
 *  Header for @l{Char@} type routines.
 */
#ifndef FBLE_CORE_CHAR_FBLE_H_
#define FBLE_CORE_CHAR_FBLE_H_

#include <fble/fble-value.h>   // for FbleValue, etc.

/**
 * @func[FbleNewCharValue] Creates an FbleValue of type @l{/Core/Char%.Char@}.
 *  The character '?' is used for any characters not currently supported by
 *  the @l{/Core/Char%.Char@ type}.
 *
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[char][c] The value of the character to write.
 *
 *  @returns[FbleValue*]
 *   The FbleValue c represented as an @l{/Core/Char%.Char@}.
 *
 *  @sideeffects
 *   Allocates a value that must be freed when no longer required.
 */
FbleValue* FbleNewCharValue(FbleValueHeap* heap, char c);

/**
 * @func[FbleCharValueAccess] Reads a character type @l{/Core/Char%.Char@}.
 *  @arg[FbleValue*][c] The value of the character.
 *
 *  @returns[char]
 *   The value x represented as a c char.
 *
 *  @sideeffects
 *   None
 */
char FbleCharValueAccess(FbleValue* c);

#endif // FBLE_CORE_CHAR_FBLE_H_
