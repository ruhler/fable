/**
 * @file fble-literal.h
 *  Definition of literal values.
 */

#ifndef FBLE_LITERAL_H_
#define FBLE_LITERAL_H_

#include <stdint.h>        // for uint8_t

#include "fble-runtime.h"    // for FbleRuntime, FbleValue

/**
 * @struct[FbleLiteral] Raw data describing a literal value.
 *  This should be treated as an opaque representation of raw data needed to
 *  construct a literal value at runtime, except that you can copy the raw
 *  data around to a different (suitably aligned) location in memory if you
 *  like.
 *
 *  @field[size_t][size] Number of data elements.
 *  @field[uint8_t*][data] The elements of the data.
 */
typedef struct {
  size_t size;
  uint8_t* data;
} FbleLiteral;

/**
 * @func[FbleNewLiteralValue] Creates an fble literal value.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[size_t][size] The size of the literal data.
 *  @arg[size_t*][data] The contents of the literal data.
 *  @returns[FbleValue*] A newly allocated literal value.
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @i Behavior is undefined if @a[data] is malformed.
 */
FbleValue* FbleNewLiteralValue(FbleRuntime* runtime, size_t size, uint8_t* data);

#endif // FBLE_LITERAL_H_

