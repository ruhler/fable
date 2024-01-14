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
 *  @arg[size_t] profile_block_offset
 *   The profile block offset to use for the function.
 *  @arg[FbleValue**] statics
 *   The array of static variables for the function. The count should match
 *   executable->statics.
 *
 *  @returns FbleValue*
 *   A newly allocated function value.
 *
 *  @sideeffects
 *   Allocates a new function value that should be freed using
 *   FbleReleaseValue when it is no longer needed.
 */
FbleValue* FbleNewInterpretedFuncValue(FbleValueHeap* heap, FbleCode* code, size_t profile_block_offset, FbleValue** statics);

#endif // FBLE_INTERNAL_INTERPRET_H_
