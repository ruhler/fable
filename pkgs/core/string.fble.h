/**
 * @file string.fble.h
 *  Header for interacting with @l{String@} values.
 */
#ifndef FBLE_CORE_STRING_FBLE_H_
#define FBLE_CORE_STRING_FBLE_H_

#include <fble/fble-value.h>

/**
 * @func[FbleNewStringValue] Converts a C string to @l{/Std/String%.String@}.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[const char*][str] The string to convert.
 *
 *  @returns[FbleValue*]
 *   A newly allocated fble @l{/Std/String%.String@} with the contents of str.
 *
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewStringValue(FbleValueHeap* heap, const char* str);

/**
 * @func[FbleNewSubStringValue] Converts a C string to @l{/Std/String%.String@}.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[const char*][str] The string to convert.
 *  @arg[size_t][length] The max number of chars to convert from.
 *
 *  @returns[FbleValue*]
 *   A newly allocated fble @l{/Std/String%.String@} with the first @a[length]
 *   characters of str.
 *
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewSubStringValue(FbleValueHeap* heap, const char* str, size_t length);

/**
 * @func[FbleStringValueAccess] Convert a @l{String@} value to a c string.
 *  @arg[FbleValue*][str] The @l{/Std/String%.String@} to convert.
 *
 *  @returns[char*]
 *   A newly allocated nul terminated c string with the contents of str.
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

#endif // FBLE_STRING_FBLE_H_

