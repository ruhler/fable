/**
 * @file data.fble.h
 *  Header for working with fble standard data types, including
 *  @l{Char@}, @l{Int@}, @l{String@}, etc.
 */
#ifndef FBLE_STD_DATA_FBLE_H_
#define FBLE_STD_DATA_FBLE_H_

#include <stdint.h>              // for uint32_t

#include <fble/fble-runtime.h>   // for FbleValue, etc.

/**
 * @func[FbleNewCharValue] Creates an FbleValue of type @l{/Std/Char%.Char@}.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[uint32_t][c] The unicode code point of the character.
 *
 *  @returns[FbleValue*]
 *   The FbleValue c represented as an @l{/Std/Char%.Char@}.
 *
 *  @sideeffects
 *   Allocates a value that must be freed when no longer required.
 */
FbleValue* FbleNewCharValue(FbleRuntime* runtime, uint32_t c);

/**
 * @func[FbleCharValueAccess] Reads a character type @l{/Std/Char%.Char@}.
 *  @arg[FbleValue*][c] The value of the character.
 *
 *  @returns[uint32_t]
 *   The unicode code point associated with the character.
 *
 *  @sideeffects
 *   None
 */
uint32_t FbleCharValueAccess(FbleValue* c);

/**
 * @func[FbleNewIntValue] Creates a new @l{/Std/Int%.Int@} value.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[int64_t][x] The integer value to create.
 *  @returns[FbleValue*] An newly allocated fble integer value.
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewIntValue(FbleRuntime* runtime, int64_t x);

/**
 * @func[FbleIntValueAccess] Read a number of type @l{/Std/Int%.Int@}.
 *  @arg[FbleValue*][x] The FbleValue value of the number.
 *
 *  @returns[int64_t]
 *   The number represented as a C int64_t.
 *
 *  @sideeffects
 *   Behavior is undefined if the int value cannot be represented using the C
 *   int64_t type, for example because it is too large.
 */
int64_t FbleIntValueAccess(FbleValue* x);

/**
 * @func[FbleNewMaybeValue] Construct a @l{/Std/Maybe%.@} value.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[FbleValue*][arg]
 *   The Just value of the maybe, or NULL to return a Nothing value.
 *  @returns[FbleValue*] The constructed Maybe@ value.
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewMaybeValue(FbleRuntime* runtime, FbleValue* arg);

/**
 * @func[FbleNewStringValue] Converts a C string to @l{/Std/String%.String@}.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[const char*][str]
 *   The string to convert, interpreted as a UTF-8 encoded multibyte character
 *   string.
 *
 *  @returns[FbleValue*]
 *   A newly allocated fble @l{/Std/String%.String@} with the contents of str.
 *   Returns NULL if the string is not a valid UTF-8 encoded multibyte
 *   character string.
 *
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewStringValue(FbleRuntime* runtime, const char* str);

/**
 * @func[FbleStringValueAccess] Convert a @l{String@} value to a c string.
 *  @arg[FbleValue*][str] The @l{/Std/String%.String@} to convert.
 *
 *  @returns[char*]
 *   A newly allocated nul terminated c string with the contents of str. The
 *   resulting string is a UTF-8 encoded multibyte character string. Returns
 *   NULL if the string can't be encoded properly, because for example it
 *   contains invalid unicode code points.
 *
 *  @sideeffects
 *   The caller should call FbleFree on the returned string when it is no
 *   longer needed.
 */
char* FbleStringValueAccess(FbleValue* str);

#endif // FBLE_STD_DATA_FBLE_H_

