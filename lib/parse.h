/**
 * @file parse.h
 *  Some utility functions related to parsing.
 */

#ifndef FBLE_INTERNAL_PARSE_H_
#define FBLE_INTERNAL_PARSE_H_

#include <stdbool.h>    // for bool

/**
 * @func[FbleIsPlainWord] Checks if @a[word] is a plain word.
 *  Tests whether a sequence of characters can be expressed as a plain,
 *  unquoted, word in the fble syntax.
 *
 *  @arg[const char*][word] the characters to test.
 *  @returns[bool] true if all characters are normal 'word' characters.
 *  @sideeffects None
 */
bool FbleIsPlainWord(const char* word);

#endif // FBLE_INTERNAL_PARSE_H_
