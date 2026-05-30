/**
 * @file literal.h
 *  Header for parsing and instantiating literals.
 */

#ifndef FBLE_INTERNAL_LITERAL_H_
#define FBLE_INTERNAL_LITERAL_H_

#include <fble/fble-literal.h>    // For FbleLiteral

#include "type.h"                 // For FbleTypeHeap, FbleType

/**
 * @func[FbleParseLiteral] Parses a literal value from source code.
 *  @arg[FbleTypeHeap*][th] The type heap
 *  @arg[FbleType*][type] The letter type.
 *  @arg[const char*][word] The literal word to parse the literal from.
 *  @returns[FbleLiteral]
 *   On success, the parsed literal with dynamically allocated data. On
 *   failure, data is set to NULL and the size field indicates an offset into
 *   the word where an error was encountered.
 *  @sideeffects
 *   Allocates data that should be freed using FbleFree when no longer needed.
 */
FbleLiteral FbleParseLiteral(FbleTypeHeap* th, FbleType* type, const char* word);

#endif // FBLE_INTERNAL_LITERAL_H_
