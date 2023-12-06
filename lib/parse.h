/**
 * @file parse.h
 * Some utility functions related to parsing.
 */

#ifndef FBLE_INTERNAL_PARSE_H_
#define FBLE_INTERNAL_PARSE_H_

#include <stdbool.h>    // for bool

// FbleIsPlainWord --
//   Tests whether a sequence of characters can be expressed as a plain,
//   unquoted, word in the fble syntax.
//
// @param word  the characters to test.
// @returns  true if all characters are normal 'word' characters.
// @sideeffects None
bool FbleIsPlainWord(const char* word);

#endif // FBLE_INTERNAL_PARSE_H_
