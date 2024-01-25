/**
 * @file interpret.h
 *  Header for fble interpreter related functions.
 */

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include <fble/fble-function.h>   // for FbleFunction, etc.

#include "code.h"   // for FbleCode

/**
 * @func[FbleNewInterpretedFuncValue] Creates an interprted fble function value.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleCode*] code
 *   The code to interpret. Borrowed.
 *  @arg[size_t] profile_block_id
 *   The profile block id to use for the function.
 *  @arg[FbleValue**] statics
 *   The array of static variables for the function. The count should match
 *   executable->statics.
 *
 *  @returns FbleValue*
 *   A newly allocated function value.
 *
 *  @sideeffects
 *   Allocates a new function value on the heap.
 */
FbleValue* FbleNewInterpretedFuncValue(FbleValueHeap* heap, FbleCode* code, size_t profile_block_id, FbleValue** statics);

#endif // FBLE_INTERNAL_INTERPRET_H_
