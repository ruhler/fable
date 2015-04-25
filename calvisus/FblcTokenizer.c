
#include "FblcInternal.h"

// Notes:
//  * 'toks' is conventional variable name for a token stream
//  * FblcTokenType is FBLC_TOK_EOF, FBLC_TOK_NAME, or a character literal.
//  * The results of FblcGetToken and FblcGetNameToken should always be
//  checked, unless FblcIsToken has already been called to verify the type of
//  the next token.

// Token types: Any single character is a token of that type. For end of file
// and name tokens, the following types are defined:
struct FblcTokenStream {
  FblcTokenType type;
  const char* name;
  FILE* fin;
  const char* filename;
  int line;
  int col;    // Column of the current token.
  int ncol;   // Column of the input stream.
};


static bool isname(int c) {
  return isalnum(c) || c == '_';
}

static int toker_getc(FblcTokenStream* toks) {
  int c = fgetc(toks->fin);
  if (c == '\n') {
    toks->line++;
    toks->ncol = 0;
  } else if (c != EOF) {
    toks->ncol++;
  }
  return c;
}

static void toker_ungetc(FblcTokenStream* toks, int c) {
  toks->ncol--;
  ungetc(c == '\n' ? ' ' : c, toks->fin);
}


static void read_next(FblcTokenStream* toks) {
  int c;
  do {
    c = toker_getc(toks);
  } while (isspace(c));
  toks->col = toks->ncol;

  if (c == EOF) {
    toks->type = FBLC_TOK_EOF;
    toks->name = NULL;
    return;
  }

  if (isname(c)) {
    char buf[BUFSIZ];
    size_t n;
    buf[0] = c;
    c = toker_getc(toks);
    for (n = 1; isname(c); n++) {
      assert(n < BUFSIZ && "TODO: Support longer names.");
      buf[n] = c;
      c = toker_getc(toks);
    }
    toker_ungetc(toks, c);
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
  toks->fin = fopen(filename, "r");
  if (toks->fin == NULL) {
    return NULL;
  }
  toks->filename = filename;
  toks->line = 1;
  toks->col = 0;
  toks->ncol = 0;
  read_next(toks);
  return toks;
}

void FblcCloseTokenStream(FblcTokenStream* toks) {
  fclose(toks->fin);
}

// Get and pop the next token, assuming the token is a name token.
// If the token is not a name token, an error message is reported and NULL is
// returned. 'expected' should be a descriptive string that will be included in
// the error message.
const char* FblcGetNameToken(FblcTokenStream* toks, const char* expected) {
  if (toks->type == FBLC_TOK_NAME) {
    const char* name = toks->name;
    read_next(toks);
    return name;
  }
  fprintf(stderr, "%s:%d:%d: error: Expected %s, but got token of type '%c'\n",
      toks->filename, toks->line, toks->col, expected, toks->type);
  return NULL;
}

// Pop the next token, assuming it is of the given token type.
// If the token is not of the given type, an error message is reported and
// false is returned.
bool FblcGetToken(FblcTokenStream* toks, int type) {
  if (toks->type == type) {
    read_next(toks);
    return true;
  }
  fprintf(stderr, "%s:%d:%d: error: Expected %c, but got token of type '%c'\n",
      toks->filename, toks->line, toks->col, type, toks->type);
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
      toks->filename, toks->line, toks->col, expected, toks->type);
}

