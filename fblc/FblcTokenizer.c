// FblcTokenizer.c --
//
//   This file implements routines for turning a file into a stream of tokens. 

#include "FblcInternal.h"

// For the most part, tokens are single punctuation characters, e.g. ';', '(',
// ')'. The three exceptions are the name token, which is a string of name
// characters, the token to represent the end of the token stream, and a token
// to represent an error has occurred during tokenization.
// FblcTokenType uses an int to represent the type of a token. Its value is
// a character literal for single punctuation characters, FBLC_TOK_NAME for a
// name token, FBLC_TOK_EOF for the token representing the end of the
// token stream, and FBLC_TOK_ERR for a token representing an error has
// occurred. The special value FBLC_TOK_PENDING is used to indicate the next
// token has not yet been determined.

// A stream of tokens is represented using the FblcTokenStream data structure.
// The data structure includes a cached of the next token in the stream, the
// underlying file descriptor, and, for error reporting purposes, the location
// in the input file of the next token and the file descriptor itself.
//
// The conventional variable name for a FblcTokenStream* is 'toks'.

struct FblcTokenStream {
  // A cache of the next token. If the 'type' of token is FBLC_TOK_PENDING,
  // the next token has not yet been read from the underlying file.
  // If the 'type' of token is FBLC_TOK_NAME, then 'name' is the value of the
  // name token. Otherwise 'name' is NULL.
  FblcTokenType type;
  const char* name;

  // The underlying file descriptor and a buffer containing data read.
  // curr points to the current character in the buffer, if any.
  // end points just past the last character in the buffer. If the buffer is
  // empty, curr and end are equal.
  int fd;
  char buffer[BUFSIZ];
  char* curr;
  char* end;

  // Location information for the next token and the current character in the
  // underlying file.
  // Because no token spans multiple lines, both the next token and the
  // underlying file share the same 'filename' and 'line' position.
  const char* filename;
  int line;
  int token_column;
  int file_column;
};

static int CurrChar(FblcTokenStream* toks);
static int NextChar(FblcTokenStream* toks);
static void AdvanceChar(FblcTokenStream* toks);
static bool IsNameChar(int c);
static void ReadNextIfPending(FblcTokenStream* toks);
static FblcLoc* TokenLoc(FblcTokenStream* toks);
static const char* DescribeTokenType(FblcTokenType which);

// CurrChar --
//
//   Look at the character at the front of the token stream's file.
//
// Inputs:
//   toks - The token stream to see the character from.
//
// Result:
//   The character at the front of the token stream's file, or EOF if the end
//   of the file has been reached.
//
// Side effects:
//   Reads data from the underlying file if necessary.

static int CurrChar(FblcTokenStream* toks)
{
  if (toks->curr == toks->end) {
    ssize_t red = read(toks->fd, toks->buffer, BUFSIZ);
    if (red <= 0) {
      return EOF;
    }
    toks->curr = toks->buffer;
    toks->end = toks->curr + red;
  }
  return *toks->curr;
}

// NextChar --
//
//   Look at the next character of the token stream's file.
//
// Inputs:
//   toks - The token stream to see the character from.
//
// Result:
//   The next character of the token stream's file, or EOF if the end of the
//   file has been reached.
//
// Side effects:
//   Reads data from the underlying file if necessary.

static int NextChar(FblcTokenStream* toks)
{
  int c = CurrChar(toks);
  if (c == EOF) {
    return EOF;
  }
  if (toks->curr + 1 == toks->end) {
    toks->buffer[0] = c;
    ssize_t red = read(toks->fd, toks->buffer + 1, BUFSIZ - 1);
    if (red <= 0) {
      return EOF;
    }
    toks->curr = toks->buffer;
    toks->end = toks->curr + 1 + red;
  }
  return *(toks->curr + 1);
}

// AdvanceChar --
//
//   Advance to the next character of the token stream's file.
//
// Inputs:
//   toks - The token stream to advance.
//
// Result:
//   None.
//
// Side effects:
//   Advances to the next character in the underyling file and updates the
//   current file location.

static void AdvanceChar(FblcTokenStream* toks)
{
  int c = CurrChar(toks);
  if (c != EOF) {
    toks->curr++;
    if (c == '\n') {
      toks->line++;
      toks->file_column = 1;
    } else {
      toks->file_column++;
    }
  }
}

// IsNameChar --
//
//   Tests whether a character is an acceptable character to use in a name
//   token.
//
// Inputs:
//   c - The character to test. This must have a value of an unsigned char or
//       EOF.
//
// Result:
//   The value true if the character is an acceptable character to use in a
//   name token, and the value false otherwise.
//
// Side effects:
//   None.

static bool IsNameChar(int c)
{
  return isalnum(c) || c == '_';
}

// ReadNextIfPending --
//
//   Read the next token from the token stream if the next token is marked as
//   FBLC_TOK_PENDING.
//
// Inputs:
//   toks - the token stream to advance to the next token.
//
// Result:
//   None.
//
// Side effects:
//   Updates the current cached next token of the token stream with the next
//   token read from the underlying file if needed. Advances the
//   underlying file to just after the token read.
//
//   If there is an error reading the next token, an error message will be
//   printed to standard error and the current token will be of type
//   FBLC_TOK_ERR.

static void ReadNextIfPending(FblcTokenStream* toks)
{
  if (toks->type == FBLC_TOK_PENDING) {
    bool is_comment_start = (CurrChar(toks) == '/' && NextChar(toks) == '/');
    while (isspace(CurrChar(toks)) || is_comment_start) {
      AdvanceChar(toks);
      if (is_comment_start) {
        while (CurrChar(toks) != EOF && CurrChar(toks) != '\n') {
          AdvanceChar(toks);
        }
      }
      is_comment_start = (CurrChar(toks) == '/' && NextChar(toks) == '/');
    }

    toks->token_column = toks->file_column;
    int c = CurrChar(toks);
    if (c == EOF) {
      toks->type = FBLC_TOK_EOF;
      toks->name = NULL;
    } else if (IsNameChar(c)) {
      size_t n;
      int capacity = 32;        // Most names are less than 32 chars?
      char* name = GC_MALLOC(capacity * sizeof(char));
      for (n = 0; IsNameChar(CurrChar(toks)); n++) {
        if (n + 1 >= capacity) {
          capacity *= 2;
          name = GC_REALLOC(name, capacity * sizeof(char));
        }
        name[n] = CurrChar(toks);
        AdvanceChar(toks);
      } 
      name[n] = '\0';

      toks->type = FBLC_TOK_NAME;
      toks->name = GC_REALLOC(name, (n+1) * sizeof(char));
    } else {
      toks->type = c;
      toks->name = NULL;
      AdvanceChar(toks);
    }
  }
}

// TokenLoc --
//
//   Return a location for the next token in the token stream.
//
// Inputs:
//   toks - The token stream.
//
// Result:
//   A new location object with the location of the next token in the stream.
//
// Side effects:
//   None.

static FblcLoc* TokenLoc(FblcTokenStream* toks)
{
  return FblcNewLoc(toks->filename, toks->line, toks->token_column);
}

// DescribeTokenType --
//   
//   Returns a human readable string description of the given token type.
//   The description is used in error message.
//
// Inputs:
//   which - The type of token to describe.
//
// Results:
//   A human reasonable string description of the given token type.
//
// Side effects:
//   None.

const char* DescribeTokenType(FblcTokenType which)
{
  if (which == FBLC_TOK_NAME) {
    return "NAME";
  } else if (which == FBLC_TOK_EOF) {
    return "EOF";
  } else if (which == FBLC_TOK_ERR) {
    return "ERR";
  } else {
    char* description = GC_MALLOC(4 * sizeof(char));
    description[0] = '\'';
    description[1] = which;
    description[2] = '\'';
    description[3] = '\0';
    return description;
  }
}

// FblcOpenTokenStream --
//
//   Create a token stream for the given file name.
//
// Inputs:
//   filename - The name of the file to return a token stream for.
//
// Results:
//   A token stream for the given file, or NULL if there was an error opening
//   the token stream.
//
// Side effects:
//   Opens the given file. The file can be closed when the token stream is no
//   longer in use by calling FblcCloseTokenStream.

FblcTokenStream* FblcOpenTokenStream(const char* filename)
{
  FblcTokenStream* toks = GC_MALLOC(sizeof(FblcTokenStream));
  toks->fd = open(filename, O_RDONLY);
  if (toks->fd < 0) {
    return NULL;
  }
  toks->type = FBLC_TOK_PENDING;
  toks->name = NULL;
  toks->curr = toks->buffer;
  toks->end = toks->buffer;
  toks->filename = filename;
  toks->line = 1;
  toks->token_column = 0;
  toks->file_column = 1;
  return toks;
}

// FblcCloseTokenStream
//
//   Close the underlying file for the given token stream.
//
// Inputs:
//   toks - the token stream to close.
//
// Results:
//   None.
//
// Side effects:
//   Closes the underlying file for the given token stream.
void FblcCloseTokenStream(FblcTokenStream* toks)
{
  close(toks->fd);
}

// FblcIsToken --
//
//   Check if the next token is of the given type.
//
// Inputs:
//   toks - The token stream to check the next token for.
//   which - The type of token to check for.
//
// Returns:
//   The value true if the next token has type 'which', false otherwise.
//
// Side effects:
//   Reads the next token from the underlying file if necessary.

bool FblcIsToken(FblcTokenStream* toks, FblcTokenType which)
{
  ReadNextIfPending(toks);
  return toks->type == which;
}

// FblcGetNameToken --
//
//   Get the value and location of the next token in the stream, which is
//   assumed to be a name token. The result of this function should always be
//   checked for false unless FblcIsToken has already been called to verify
//   the next token is a name token.
//
// Inputs:
//   toks - The token stream to retrieve the name token from.
//   expected - A short description of the expected name token, for use in
//              error messages e.g. "a field name" or "a type name".
//   name - A pointer to an FblcLocName that will be filled in with the
//          token's value and location.
//
// Returns:
//   True if the next token is a name token, false otherwise.
//
// Side effects:
//   If the next token in the stream is a name token, name is filled in with
//   the value and location of the name token, and the name token is removed
//   from the front of the token stream. Otherwise an error message is printed
//   to standard error.

bool FblcGetNameToken(
    FblcTokenStream* toks, const char* expected, FblcLocName* name)
{
  ReadNextIfPending(toks);
  if (toks->type == FBLC_TOK_NAME) {
    name->name = toks->name;
    name->loc = TokenLoc(toks);
    toks->type = FBLC_TOK_PENDING;
    toks->name = NULL;
    return name;
  }
  FblcUnexpectedToken(toks, expected);
  return NULL;
}

// FblcGetToken --
//
//   Remove the next token in the token stream, assuming the token has the
//   given token type. The result of this function should always be checked
//   for false unless FblcIsToken has already been called to verify the next
//   token has the given type.
//
// Inputs:
//   toks - The token stream to get the next token from.
//   which - The type of token that is expected to be next.
//
// Results:
//   The value true if the next token has the type 'which', false otherwise.
//
// Side effects:
//   If the next token in the stream has the type 'which', that token is
//   removed from the front of the token stream. Otherwise an error message is
//   printed to standard error.

bool FblcGetToken(FblcTokenStream* toks, int which)
{
  ReadNextIfPending(toks);
  if (toks->type == which) {
    toks->type = FBLC_TOK_PENDING;
    return true;
  }
  FblcUnexpectedToken(toks, DescribeTokenType(which));
  return false;
}

// FblcUnexpectedToken --
//
//   Report an error message indicating the next token was not of the expected
//   type.
//
// Inputs:
//   toks - The token stream with the unexpected next token type.
//   expected - A short description of the expected name token, for use in
//              error messages e.g. "a field name" or "a type name".
//
// Result:
//   None.
//
// Side effect:
//   An error message is printed to standard error.

void FblcUnexpectedToken(FblcTokenStream* toks, const char* expected)
{
  const char* next_token = DescribeTokenType(toks->type);
  FblcReportError("Expected %s, but got token of type %s.\n",
      TokenLoc(toks), expected, next_token);
}
