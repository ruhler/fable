// FblcTokenizer.c --
//
//   The file implements routines for turning a FILE stream into a stream of
//   tokens. 

#include "FblcInternal.h"

// For the most part, tokens are single punctuation characters, e.g. ';', '(',
// ')'. The two exceptions are the name token, which is a string of name
// characters, and the token to represent the end of the token stream.
// FblcTokenType uses an int to represent the type of a token. Its value is
// a character literal for single punctuation characters, FBLC_TOK_NAME for a
// name token, and FBLC_TOK_EOF for the token representing the end of the
// token stream.

// A stream of tokens is represented using the FblcTokenStream data structure.
// The data structure includes the next token in the stream, the underlying
// FILE stream, and, for error reporting purposes, the location in the input
// file of the next token and the FILE stream itself.
//
// The conventional variable name for a FblcTokenStream* is 'toks'.

struct FblcTokenStream {
  // The next token. This token has been read from the underlying FILE stream,
  // but has not yet been taken by the user.
  // If the 'type' of token is FBLC_TOK_NAME, then 'name' is the value of the
  // name token. Otherwise 'name' is NULL.
  FblcTokenType type;
  const char* name;

  // The underlying FILE stream.
  FILE* stream;

  // Location information for the next token and the underlying stream.
  // Because no token spans multiple lines, both the next token and the
  // underlying stream share the same 'filename' and 'line' position.
  const char* filename;
  int line;
  int token_column;
  int stream_column;
};

static int GetChar(FblcTokenStream* toks);
static void UnGetChar(FblcTokenStream* toks, int c);
static bool IsNameChar(int c);
static void ReadNextToken(FblcTokenStream* toks);




// Notes:
//  * The results of FblcGetToken and FblcGetNameToken should always be
//  checked, unless FblcIsToken has already been called to verify the type of
//  the next token.


static bool IsNameChar(int c) {
  return isalnum(c) || c == '_';
}

static int GetChar(FblcTokenStream* toks) {
  int c = fgetc(toks->stream);
  if (c == '\n') {
    toks->line++;
    toks->stream_column = 0;
  } else if (c != EOF) {
    toks->stream_column++;
  }
  return c;
}

static void UnGetChar(FblcTokenStream* toks, int c) {
  toks->stream_column--;
  ungetc(c == '\n' ? ' ' : c, toks->stream);
}


static void ReadNextToken(FblcTokenStream* toks) {
  int c;
  do {
    c = GetChar(toks);
  } while (isspace(c));
  toks->token_column = toks->stream_column;

  if (c == EOF) {
    toks->type = FBLC_TOK_EOF;
    toks->name = NULL;
    return;
  }

  if (IsNameChar(c)) {
    char buf[BUFSIZ];
    size_t n;
    buf[0] = c;
    c = GetChar(toks);
    for (n = 1; IsNameChar(c); n++) {
      assert(n < BUFSIZ && "TODO: Support longer names.");
      buf[n] = c;
      c = GetChar(toks);
    }
    UnGetChar(toks, c);
    toks->type = FBLC_TOK_NAME;
    char* name = GC_MALLOC((n+1) * sizeof(char));
    for (int i = 0; i < n; i++) {
      name[i] = buf[i];
    }
    name[n] = '\0';
    toks->name = name;
    return;
  }

  toks->type = c;
  toks->name = NULL;
}

FblcTokenStream* FblcOpenTokenStream(const char* filename) {
  FblcTokenStream* toks = GC_MALLOC(sizeof(FblcTokenStream));
  toks->stream = fopen(filename, "r");
  if (toks->stream == NULL) {
    return NULL;
  }
  toks->filename = filename;
  toks->line = 1;
  toks->token_column = 0;
  toks->stream_column = 0;
  ReadNextToken(toks);
  return toks;
}

void FblcCloseTokenStream(FblcTokenStream* toks) {
  fclose(toks->stream);
}

// Get and pop the next token, assuming the token is a name token.
// If the token is not a name token, an error message is reported and NULL is
// returned. 'expected' should be a descriptive string that will be included in
// the error message.
const char* FblcGetNameToken(FblcTokenStream* toks, const char* expected) {
  if (toks->type == FBLC_TOK_NAME) {
    const char* name = toks->name;
    ReadNextToken(toks);
    return name;
  }
  fprintf(stderr, "%s:%d:%d: error: Expected %s, but got token of type '%c'\n",
      toks->filename, toks->line, toks->token_column, expected, toks->type);
  return NULL;
}

// Pop the next token, assuming it is of the given token type.
// If the token is not of the given type, an error message is reported and
// false is returned.
bool FblcGetToken(FblcTokenStream* toks, int type) {
  if (toks->type == type) {
    ReadNextToken(toks);
    return true;
  }
  fprintf(stderr, "%s:%d:%d: error: Expected %c, but got token of type '%c'\n",
      toks->filename, toks->line, toks->token_column, type, toks->type);
  return false;
}

// Return true if the next token is of the given type.
bool FblcIsToken(FblcTokenStream* toks, int type) {
  return toks->type == type;
}

// Report an error message of an unexpected token.
// expected should contain a descriptive string of the expected token to be
// included in the error message.
void FblcUnexpectedToken(FblcTokenStream* toks, const char* expected) {
  fprintf(stderr, "%s:%d:%d: error: Expected %s, but got token of type '%c'\n",
      toks->filename, toks->line, toks->token_column, expected, toks->type);
}

