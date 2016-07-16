// FblcTokenizer.c --
//
//   This file implements routines for turning a file into a stream of tokens. 

#include "FblcInternal.h"

#define MAX_TOK_DESC_LEN 5

static int CurrChar(FblcTokenStream* toks);
static int NextChar(FblcTokenStream* toks);
static void AdvanceChar(FblcTokenStream* toks);
static bool IsNameChar(int c);
static void SkipToToken(FblcTokenStream* toks);
static FblcLoc* TokenLoc(FblcTokenStream* toks);
static void DescribeTokenType(int which, char desc[MAX_TOK_DESC_LEN]);

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
      toks->column = 1;
    } else {
      toks->column++;
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

// SkipToToken --
//
//   Skip past any whitespace or comments to a token character.
//
// Inputs:
//   toks - the token stream to skip to a token in.
//
// Result:
//   None.
//
// Side effects:
//   Advances past whitespace or comments to reach a token character, if the
//   stream is not already positioned at a token character.

static void SkipToToken(FblcTokenStream* toks)
{
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
  return FblcNewLoc(toks->filename, toks->line, toks->column);
}

// DescribeTokenType --
//   
//   Computes a human readable string description of the given token type.
//   The description is used in error message.
//
// Inputs:
//   which - The type of token to describe.
//   desc - A buffer of at least MAX_TOK_DESC_LEN bytes that will be filled
//          with the token description.
//
// Results:
//   None.
//
// Side effects:
//   'desc' is filled with a human reasonable string description of the given
//   token type.

void DescribeTokenType(int which, char desc[MAX_TOK_DESC_LEN])
{
  char* p = desc;
  if (IsNameChar(which)) {
    *p++ = 'N';
    *p++ = 'A';
    *p++ = 'M';
    *p++ = 'E';
  } else if (which == EOF) {
    *p++ = 'E';
    *p++ = 'O';
    *p++ = 'F';
  } else {
    *p++ = '\'';
    *p++ = which;
    *p++ = '\'';
  }
  *p = '\0';
  assert(p < desc + MAX_TOK_DESC_LEN && "MAX_TOK_DESC_LEN is too small");
}

// FblcOpenTokenStream --
//
//   Open a token stream for the given file name.
//
// Inputs:
//   toks - An uninitialized token stream to open.
//   filename - The name of the file to return a token stream for.
//
// Results:
//   True if the token stream was successfully opened, false otherwise.
//
// Side effects:
//   Initializes the toks object. Opens the given file. If the token stream is
//   successfully opened, the file can be closed when the token stream is no
//   longer in use by calling FblcCloseTokenStream.

bool FblcOpenTokenStream(FblcTokenStream* toks, const char* filename)
{
  toks->fd = open(filename, O_RDONLY);
  if (toks->fd < 0) {
    return false;
  }
  toks->curr = toks->buffer;
  toks->end = toks->buffer;
  toks->filename = filename;
  toks->line = 1;
  toks->column = 1;
  return true;
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

// FblcIsEOFToken --
//
//   Check if the end of the token stream has been reached.
//
// Inputs:
//   toks - The token stream to check for the end of.
//
// Returns:
//   The value true if the end of the token stream has been reached.
//
// Side effects:
//   Reads the next token from the underlying file if necessary.

bool FblcIsEOFToken(FblcTokenStream* toks)
{
  SkipToToken(toks);
  return CurrChar(toks) == EOF;
}

// FblcIsToken --
//
//   Check if the next token is the given character.
//
// Inputs:
//   toks - The token stream to check the next token for.
//   which - The character to check for.
//
// Returns:
//   The value true if the next token is the character 'which', false otherwise.
//
// Side effects:
//   Reads the next token from the underlying file if necessary.

bool FblcIsToken(FblcTokenStream* toks, char which)
{
  SkipToToken(toks);
  return CurrChar(toks) == which;
}

// FblcGetToken --
//
//   Remove the next token in the token stream, assuming the token is the
//   given character. The result of this function should always be checked
//   for false unless FblcIsToken has already been called to verify the next
//   token has the given type.
//
// Inputs:
//   toks - The token stream to get the next token from.
//   which - The character that is expected to be the next token.
//
// Results:
//   The value true if the next token is the character 'which', false otherwise.
//
// Side effects:
//   If the next token in the stream is the character 'which', that token is
//   removed from the front of the token stream. Otherwise an error message is
//   printed to standard error.

bool FblcGetToken(FblcTokenStream* toks, char which)
{
  SkipToToken(toks);
  if (CurrChar(toks) == which) {
    *toks->curr = ' ';
    return true;
  }
  char desc[MAX_TOK_DESC_LEN];
  DescribeTokenType(which, desc);
  FblcUnexpectedToken(toks, desc);
  return false;
}

// FblcIsNameToken --
//
//   Check if the next token is a name token.
//
// Inputs:
//   toks - The token stream to check the next token for.
//
// Returns:
//   The value true if the next token is a name token, false otherwise.
//
// Side effects:
//   Reads the next token from the underlying file if necessary.

bool FblcIsNameToken(FblcTokenStream* toks)
{
  SkipToToken(toks);
  return IsNameChar(CurrChar(toks));
}

// FblcGetNameToken --
//
//   Get the value and location of the next token in the stream, which is
//   assumed to be a name token. The result of this function should always be
//   checked for false unless FblcIsNameToken has already been called to
//   verify the next token is a name token.
//
// Inputs:
//   alloc - The allocator to use for allocating the name.
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

bool FblcGetNameToken(FblcAllocator* alloc,
    FblcTokenStream* toks, const char* expected, FblcLocName* name)
{
  SkipToToken(toks);
  if (IsNameChar(CurrChar(toks))) {
    FblcVector vector;
    FblcVectorInit(alloc, &vector, sizeof(char));

    name->loc = TokenLoc(toks);
    while (IsNameChar(CurrChar(toks))) {
      *((char*)FblcVectorAppend(&vector)) = CurrChar(toks);
      AdvanceChar(toks);
    } 
    *((char*)FblcVectorAppend(&vector)) = '\0';
    int n;
    name->name = FblcVectorExtract(&vector, &n);
    return true;
  }
  FblcUnexpectedToken(toks, expected);
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
  SkipToToken(toks);
  char desc[MAX_TOK_DESC_LEN];
  DescribeTokenType(CurrChar(toks), desc);
  FblcReportError("Expected %s, but got token of type %s.\n",
      TokenLoc(toks), expected, desc);
}
