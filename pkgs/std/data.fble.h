/**
 * @file data.fble.h
 *  Header for working with fble standard data types, including
 *  @l{Char@}, @l{Int@}, @l{String@}, etc.
 */
#ifndef FBLE_STD_DATA_FBLE_H_
#define FBLE_STD_DATA_FBLE_H_

#include <stddef.h>            // for wchar_t

#include <fble/fble-value.h>   // for FbleValue, etc.

/**
 * @func[FbleNewCharValue] Creates an FbleValue of type @l{/Std/Char%.Char@}.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[char][c] The value of the character to write.
 *
 *  @returns[FbleValue*]
 *   The FbleValue c represented as an @l{/Std/Char%.Char@}.
 *
 *  @sideeffects
 *   Allocates a value that must be freed when no longer required.
 */
FbleValue* FbleNewCharValue(FbleValueHeap* heap, wchar_t c);

/**
 * @func[FbleCharValueAccess] Reads a character type @l{/Std/Char%.Char@}.
 *  @arg[FbleValue*][c] The value of the character.
 *
 *  @returns[char]
 *   The value x represented as a c char.
 *
 *  @sideeffects
 *   None
 */
wchar_t FbleCharValueAccess(FbleValue* c);

/**
 * @func[FbleNewIntValue] Creates a new @l{/Std/Int%.Int@} value.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[int64_t][x] The integer value to create.
 *  @returns[FbleValue*] An newly allocated fble integer value.
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewIntValue(FbleValueHeap* heap, int64_t x);

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
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[FbleValue*][arg]
 *   The Just value of the maybe, or NULL to return a Nothing value.
 *  @returns[FbleValue*] The constructed Maybe@ value.
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewMaybeValue(FbleValueHeap* heap, FbleValue* arg);

/**
 * @func[FbleNewStringValue] Converts a C string to @l{/Std/String%.String@}.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[const char*][str]
 *   The string to convert, interpreted as a multibyte character string based
 *   on the current locale, and assuming that the current locale wchar_t
 *   values are unicode code points.
 *
 *  @returns[FbleValue*]
 *   A newly allocated fble @l{/Std/String%.String@} with the contents of str.
 *   Returns NULL if the string is not a valid multibyte character string
 *   under the current locale.
 *
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewStringValue(FbleValueHeap* heap, const char* str);

/**
 * @func[FbleStringValueAccess] Convert a @l{String@} value to a c string.
 *  @arg[FbleValue*][str] The @l{/Std/String%.String@} to convert.
 *
 *  @returns[char*]
 *   A newly allocated nul terminated c string with the contents of str. The
 *   resulting string is a multibyte character string encoded based on the
 *   current locale, assuming that the current locale wchar_t values are
 *   unicode code points. Returns NULL if the string could not be converted
 *   under the current locale.
 *
 *  @sideeffects
 *   The caller should call FbleFree on the returned string when it is no
 *   longer needed.
 */
char* FbleStringValueAccess(FbleValue* str);

/**
 * @func[FbleDebugTrace] Prints a @l{/Std/String%.String@} value to stderr.
 *  @arg[FbleValue*][str] The @l{/Std/String%.String@} to print.
 *
 *  @sideeffects
 *   Prints the string to stderr.
 */
void FbleDebugTrace(FbleValue* str);

#endif // FBLE_STD_DATA_FBLE_H_

