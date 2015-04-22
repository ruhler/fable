
#ifndef TOKER_H_
#define TOKER_H_

#include <stdbool.h>
#include <stdio.h>

// Token types: Any single character is a token of that type. For end of file
// and name tokens, the following types are defined:
#define TOK_EOF -1
#define TOK_NAME -2

typedef struct {
  int type;
  const char* name;
  FILE* fin;
  const char* filename;
  int line;
  int col;    // Column of the current token.
  int ncol;   // Column of the input stream.
} toker_t;

toker_t* toker_open(const char* filename);
void toker_close(toker_t* toker);

// Get and pop the next token, assuming the token is a name token.
// If the token is not a name token, an error message is reported and NULL is
// returned. 'expected' should be a descriptive string that will be included in
// the error message.
const char* toker_get_name(toker_t* toker, const char* expected);

// Pop the next token, assuming it is of the given token type.
// If the token is not of the given type, an error message is reported and
// false is returned.
bool toker_get(toker_t* toker, int type);

// Return true if the next token is of the given type.
bool toker_is(toker_t* toker, int type);

// Report an error message of an unexpected token.
// expected should contain a descriptive string of the expected token to be
// included in the error message.
void toker_unexpected(toker_t* toker, const char* expected);

#endif//TOKER_H_

