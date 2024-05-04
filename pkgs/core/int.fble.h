/**
 * @file int.fble.h
 *  Routines for interacting with @l{Int@} type values.
 */

#ifndef FBLE_CORE_INT_FBLE_H_
#define FBLE_CORE_INT_FBLE_H_

#include <fble/fble-value.h>

/**
 * @func[FbleNewIntValue] Creates a new @l{/Core/Int%.Int@} value.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[int64_t][x] The integer value to create.
 *  @returns[FbleValue*] An newly allocated fble integer value.
 *  @sideeffects
 *   Allocates an FbleValue on the heap.
 */
FbleValue* FbleNewIntValue(FbleValueHeap* heap, int64_t x);

/**
 * @func[FbleIntValueAccess] Read a number of type @l{/Core/Int%.Int@}.
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

#endif // FBLE_CORE_INT_FBLE_H_
