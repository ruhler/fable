// parse.c --
//   This file implements routines to parse a fblc program from a text file.
//   into abstract syntax form.

#include <assert.h>       // for assert
#include <ctype.h>        // for isalnum, isspace
#include <fcntl.h>        // for open
#include <stdio.h>        // for EOF
#include <sys/stat.h>     // for open
#include <sys/types.h>    // for ssize_t, open
#include <unistd.h>       // for read, close

#include "fblcs.h"


// Vector --
//   There are quite a few places in the fblc data structure where vectors are
//   used: an array of elements with a size. Specifically the type of a vector
//   of elements T is struct { size_t size; T* xs; }.
//   Often these vectors are constructed without knowing the size ahead of
//   time. The following macros are used to help construct these vectors.

// VectorInit --
//   Initialize a vector for construction.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to an uninitialized vector.
//
// Results:
//   None.
//
// Side effects:
//   The vector is initialized to an array containing 0 elements.
//
// Implementation notes:
//   The array initially has size 0 and capacity 1.
#define VectorInit(arena, vector) \
  (vector).size = 0; \
  (vector).xs = arena->alloc(arena, sizeof(*((vector).xs)))

// VectorExtend --
//   Extend a vector's capacity if necessary to ensure it has space for more
//   than size elements.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using VectorInit.
//
// Results:
//   None.
//
// Side effects:
//   Extends the vector's capacity if necessary to ensure it has space for
//   more than size elements. If necessary, the array is re-allocated to make
//   space for the new element.
//
// Implementation notes:
//   We assume the capacity of the array is the smallest power of 2 that holds
//   size elements. If size is equal to the capacity of the array, we double
//   the capacity of the array, which preserves the invariant after the size is
//   incremented.
#define VectorExtend(arena, vector) \
  if ((vector).size > 0 && (((vector).size & ((vector).size-1)) == 0)) { \
    void* resized = arena->alloc(arena, 2 * (vector).size * sizeof(*((vector).xs))); \
    memcpy(resized, (vector).xs, (vector).size * sizeof(*((vector).xs))); \
    arena->free(arena, (vector).xs); \
    (vector).xs = resized; \
  }

// VectorAppend --
//   Append an element to a vector.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vector - A reference to a vector that was initialized using VectorInit.
//   elem - An element of type T to append to the array.
//
// Results:
//   None.
//
// Side effects:
//   The given element is appended to the array and the size is incremented.
//   If necessary, the array is re-allocated to make space for the new
//   element.
#define VectorAppend(arena, vector, elem) \
  VectorExtend(arena, vector); \
  (vector).xs[(vector).size++] = elem

// TokenStream --
//   A stream of tokens is represented using the TokenStream data structure.
//   Tokens can be read either from a file or a string.
//
//   The conventional variable name for a TokenStream* is 'toks'.
typedef struct {
  // When reading from a file, fd is the file descriptor for the underlying
  // file, string is NULL. When reading from a string, string contains the
  // characters of interest and fd is -1.
  int fd;
  const char* string;

  // curr is the character currently at the front of the stream. It is valid
  // only if has_curr is true, in which case the current character has already
  // been read from the file or string.
  bool has_curr;
  int curr;

  // Location information for the next token for the purposes of error
  // reporting.
  FblcsLoc loc;
} TokenStream;

#define MAX_TOK_DESC_LEN 5
static int CurrChar(TokenStream* toks);
static void AdvanceChar(TokenStream* toks);
static bool IsNameChar(int c);
static void SkipToToken(TokenStream* toks);
static void DescribeTokenType(int which, char desc[MAX_TOK_DESC_LEN]);

static bool OpenFdTokenStream(TokenStream* toks, int fd, const char* source);
static bool OpenFileTokenStream(TokenStream* toks, const char* filename);
static bool OpenStringTokenStream(TokenStream* toks, const char* source, const char* string);
static FblcsLoc* GetCurrentLoc(FblcArena* arena, TokenStream* toks);
static bool IsEOFToken(TokenStream* toks);
static bool IsToken(TokenStream* toks, char which);
static bool GetToken(TokenStream* toks, char which);
static bool IsNameToken(TokenStream* toks);
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, FblcsNameL* name);
static void UnexpectedToken(TokenStream* toks, const char* expected);

static bool ParseTypedId(FblcArena* arena, TokenStream* toks, const char* expected, FblcsTypedName* name);
static bool ParseNonZeroArgs(FblcArena* arena, TokenStream* toks, FblcExprV* argv, FblcsExprV* exprv);
static bool ParseArgs(FblcArena* arena, TokenStream* toks, FblcExprV* argv, FblcsExprV* exprv);
static FblcExpr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcsExprV* exprv);
static FblcActn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcsActnV* actnv, FblcsExprV* exprv);

// CurrChar --
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
static int CurrChar(TokenStream* toks)
{
  if (!toks->has_curr) {
    if (toks->fd != -1) {
      char c;
      if (read(toks->fd, &c, 1) == 1) {
        toks->curr = c;
      } else {
        toks->curr = EOF;
      }
    } else {
      assert(toks->string != NULL);
      if (*toks->string == '\0') {
        toks->curr = EOF;
      } else {
        toks->curr = *toks->string++;
      }
    }
    toks->has_curr = true;
  }
  return toks->curr;
}

// AdvanceChar --
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
static void AdvanceChar(TokenStream* toks)
{
  int c = CurrChar(toks);
  if (c != EOF) {
    if (c == '\n') {
      toks->loc.line++;
      toks->loc.col = 1;
    } else {
      toks->loc.col++;
    }
    toks->has_curr = false;
  }
}

// IsNameChar --
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
static void SkipToToken(TokenStream* toks)
{
  bool is_comment_start = CurrChar(toks) == '#';
  while (isspace(CurrChar(toks)) || is_comment_start) {
    AdvanceChar(toks);
    if (is_comment_start) {
      while (CurrChar(toks) != EOF && CurrChar(toks) != '\n') {
        AdvanceChar(toks);
      }
    }
    is_comment_start = CurrChar(toks) == '#';
  }
}

// DescribeTokenType --
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
static void DescribeTokenType(int which, char desc[MAX_TOK_DESC_LEN])
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

// OpenFdTokenStream --
//   Open a token stream for the given file descriptor.
//
// Inputs:
//   toks - An uninitialized token stream to open.
//   fd - The file descriptor to open the token stream from.
//   source - A label to identify the source for error reporting purposes.
//
// Results:
//   True if the token stream was successfully opened, false otherwise.
//
// Side effects:
//   Initializes the toks object. Opens the given file. If the token stream is
//   successfully opened, the file can be closed when the token stream is no
//   longer in use by calling CloseTokenStream.
static bool OpenFdTokenStream(TokenStream* toks, int fd, const char* source)
{
  toks->fd = fd;
  if (toks->fd < 0) {
    return false;
  }
  toks->string = NULL;
  toks->has_curr = false;
  toks->loc.source = source;
  toks->loc.line = 1;
  toks->loc.col = 1;
  return true;
}

// OpenFileTokenStream --
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
//   longer in use by calling CloseTokenStream.
static bool OpenFileTokenStream(TokenStream* toks, const char* filename)
{
  return OpenFdTokenStream(toks, open(filename, O_RDONLY), filename);
}

// OpenStringTokenStream --
//   Open a token stream for the given string data.
//
// Inputs:
//   toks - An uninitialized token stream to open.
//   source - A label to identify the source for error reporting purposes.
//   string - The string to return a token stream for.
//
// Results:
//   True if the token stream was successfully opened, false otherwise.
//
// Side effects:
//   Initializes the toks object.
static bool OpenStringTokenStream(TokenStream* toks, const char* source, const char* string)
{
  toks->fd = -1;
  toks->string = string;
  toks->has_curr = false;
  toks->loc.source = source;
  toks->loc.line = 1;
  toks->loc.col = 1;
  return true;
}

// GetCurrentLoc --
//   Return the current location of the token stream.
//
// Inputs:
//   arena - Arena used to allocate returned location struction.
//   toks - The token stream to get the location of.
//
// Results:
//   The current location of the token stream in a newly allocated structure
//   that is not affected by future changes to the token stream.
//
// Side effects:
//   Allocates an FblcsLoc object using the arena.
//   Advances past whitespace or comments to reach a token character, if the
//   stream is not already positioned at a token character.
static FblcsLoc* GetCurrentLoc(FblcArena* arena, TokenStream* toks)
{
  SkipToToken(toks);
  FblcsLoc* loc = arena->alloc(arena, sizeof(FblcsLoc));
  loc->source = toks->loc.source;
  loc->line = toks->loc.line;
  loc->col = toks->loc.col;
  return loc;
}

// IsEOFToken --
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
static bool IsEOFToken(TokenStream* toks)
{
  SkipToToken(toks);
  return CurrChar(toks) == EOF;
}

// IsToken --
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
static bool IsToken(TokenStream* toks, char which)
{
  SkipToToken(toks);
  return CurrChar(toks) == which;
}

// GetToken --
//   Remove the next token in the token stream, assuming the token is the
//   given character. The result of this function should always be checked
//   for false unless IsToken has already been called to verify the next
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
static bool GetToken(TokenStream* toks, char which)
{
  SkipToToken(toks);
  if (CurrChar(toks) == which) {
    AdvanceChar(toks);
    return true;
  }
  char desc[MAX_TOK_DESC_LEN];
  DescribeTokenType(which, desc);
  UnexpectedToken(toks, desc);
  return false;
}

// IsNameToken --
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
static bool IsNameToken(TokenStream* toks)
{
  SkipToToken(toks);
  return IsNameChar(CurrChar(toks));
}

// GetNameToken --
//   Get the value and location of the next token in the stream, which is
//   assumed to be a name token. The result of this function should always be
//   checked for false unless IsNameToken has already been called to
//   verify the next token is a name token.
//
// Inputs:
//   arena - The arena to use for allocating the name.
//   toks - The token stream to retrieve the name token from.
//   expected - A short description of the expected name token, for use in
//              error messages e.g. "a field name" or "a type name".
//   name - A pointer to an FblcsNameL that will be filled in with the
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
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, FblcsNameL* name)
{
  SkipToToken(toks);
  if (IsNameChar(CurrChar(toks))) {
    struct { size_t size; char* xs; } namev;
    VectorInit(arena, namev);
    name->loc = GetCurrentLoc(arena, toks);
    while (IsNameChar(CurrChar(toks))) {
      VectorAppend(arena, namev, CurrChar(toks));
      AdvanceChar(toks);
    } 
    VectorAppend(arena, namev, '\0');
    name->name = namev.xs;
    return true;
  }
  UnexpectedToken(toks, expected);
  return false;
}

// UnexpectedToken --
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
static void UnexpectedToken(TokenStream* toks, const char* expected)
{
  SkipToToken(toks);
  char desc[MAX_TOK_DESC_LEN];
  DescribeTokenType(CurrChar(toks), desc);
  FblcsReportError("Expected %s, but got token of type %s.\n",
      &toks->loc, expected, desc);
}

// ParseTypedId --
//   Parse a type id followed by a name id from the given token stream.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the arguments from.
//   expected - A short description of the expected name token, for use in
//              error messages e.g. "a field name" or "a variable name".
//   name - Output parameter set to the parsed type and name.
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses two ids from the token stream, advancing the stream past the ids.
//   Updates name with the parsed ids.
//   In case of an error, an error message is printed to stderr.
static bool ParseTypedId(FblcArena* arena, TokenStream* toks, const char* expected, FblcsTypedName* name)
{
  return GetNameToken(arena, toks, "type name", &name->type)
      && GetNameToken(arena, toks, expected, &name->name);
}

// ParseNonZeroArgs --
//   Parse a list of one or more arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls, conditional
//   expressions, and process calls.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the arguments from.
//   argv - Pointer to an uninitialized vector to fill with the parsed arguments.
//   exprv - Pointer to the vector of expr symbol information for the current declaration.
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses arguments from the token stream, advancing the stream past the
//   final ')' token in the argument list.
//   Initializes the argv vector and fills it with the parsed arguments.
//   Appends symbol information for the newly parsed arguments to the exprv
//   vector.
//   In case of an error, an error message is printed to stderr.
static bool ParseNonZeroArgs(FblcArena* arena, TokenStream* toks, FblcExprV* argv, FblcsExprV* exprv)
{
  VectorInit(arena, *argv);
  bool first = true;
  while (first || IsToken(toks, ',')) {
    if (first) {
      first = false;
    } else {
      assert(IsToken(toks, ','));
      GetToken(toks, ',');
    }
    FblcExpr* arg = ParseExpr(arena, toks, false, exprv);
    if (arg == NULL) {
      return false;
    }
    VectorAppend(arena, *argv, arg);
  }
  if (!GetToken(toks, ')')) {
    return false;
  }
  return true;
}

// ParseArgs --
//   Parse a list of zero or more arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls and process calls.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the arguments from.
//   argv - Pointer to an uninitialized vector to fill with the parsed arguments.
//   exprv - Pointer to the vector of expr symbol information for the current declaration.
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses arguments from the token stream, advancing the stream past the
//   final ')' token in the argument list.
//   Initializes the argv vector and fills it with the parsed arguments.
//   Appends symbol information for the newly parsed arguments to the exprv
//   vector.
//   In case of an error, an error message is printed to stderr.
static bool ParseArgs(FblcArena* arena, TokenStream* toks, FblcExprV* argv, FblcsExprV* exprv)
{
  if (IsToken(toks, ')')) {
    GetToken(toks, ')');
    VectorInit(arena, *argv);
    return true;
  }
  return ParseNonZeroArgs(arena, toks, argv, exprv);
}

// ParseExpr --
//   Parse an expression from the token stream.
//   If in_stmt is true, the expression is parsed in a statement context,
//   otherwise the expression must be standalone.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the expression from.
//   in_stmt - True if parsing an expression in the statement context.
//   exprv - Pointer to the vector of expr symbol information for the current declaration.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Parses an expression from the token stream, advancing the token stream
//   past the parsed expression, not including the trailing semicolon in the
//   case when in_stmt is true.
//   Appends symbol information for the parsed expression to the exprv vector.
//   In case of an error, an error message is printed to stderr.
static FblcExpr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcsExprV* exprv)
{
  FblcExpr* expr = NULL;
  FblcsLoc* loc = GetCurrentLoc(arena, toks);
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    expr = ParseExpr(arena, toks, true, exprv);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsNameToken(toks)) {
    FblcsNameL start;
    GetNameToken(arena, toks, "start of expression", &start);
    if (IsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      FblcAppExpr* app_expr = arena->alloc(arena, sizeof(FblcAppExpr));
      FblcsAppExpr* sapp_expr = arena->alloc(arena, sizeof(FblcsAppExpr));
      app_expr->_base.tag = FBLC_APP_EXPR;
      app_expr->_base.id = exprv->size;
      sapp_expr->_base.loc = loc;
      VectorAppend(arena, *exprv, &sapp_expr->_base);
      app_expr->func = FBLC_NULL_ID;
      sapp_expr->func.loc = start.loc;
      sapp_expr->func.name = start.name;
      GetToken(toks, '(');
      if (!ParseArgs(arena, toks, &app_expr->argv, exprv)) {
        return NULL;
      }
      expr = &app_expr->_base;
    } else if (IsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      GetToken(toks, ':');
      FblcUnionExpr* union_expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      FblcsUnionExpr* sunion_expr = arena->alloc(arena, sizeof(FblcsUnionExpr));
      union_expr->_base.tag = FBLC_UNION_EXPR;
      union_expr->_base.id = exprv->size;
      sunion_expr->_base.loc = loc;
      VectorAppend(arena, *exprv, &sunion_expr->_base);
      union_expr->type = FBLC_NULL_ID;
      sunion_expr->type.loc = start.loc;
      sunion_expr->type.name = start.name;
      union_expr->field = FBLC_NULL_ID;
      if (!GetNameToken(arena, toks, "field name", &sunion_expr->field)) {
        return NULL;
      }

      if (!GetToken(toks, '(')) {
        return NULL;
      }
      union_expr->arg = ParseExpr(arena, toks, false, exprv);
      if (union_expr->arg == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      expr = &union_expr->_base;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      FblcLetExpr* let_expr = arena->alloc(arena, sizeof(FblcLetExpr));
      FblcsLetExpr* slet_expr = arena->alloc(arena, sizeof(FblcsLetExpr));
      let_expr->_base.tag = FBLC_LET_EXPR;
      let_expr->_base.id = exprv->size;
      slet_expr->_base.loc = loc;
      VectorAppend(arena, *exprv, &slet_expr->_base);
      let_expr->type = FBLC_NULL_ID;
      slet_expr->type.loc = start.loc;
      slet_expr->type.name = start.name;
      if (!GetNameToken(arena, toks, "variable name", &slet_expr->name)) {
        return NULL;
      }
      if (!GetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(arena, toks, false, exprv);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(arena, toks, true, exprv);
      if (let_expr->body == NULL) {
        return NULL;
      }
      return &let_expr->_base;
    } else {
      // This is the variable expression: start
      FblcVarExpr* var_expr = arena->alloc(arena, sizeof(FblcVarExpr));
      FblcsVarExpr* svar_expr = arena->alloc(arena, sizeof(FblcsVarExpr));
      var_expr->_base.tag = FBLC_VAR_EXPR;
      var_expr->_base.id = exprv->size;
      svar_expr->_base.loc = loc;
      VectorAppend(arena, *exprv, &svar_expr->_base);
      var_expr->var = FBLC_NULL_ID;
      svar_expr->var.loc = start.loc;
      svar_expr->var.name = start.name;
      expr = &var_expr->_base;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional expression of the form: ?(<expr> ; <args>)
    FblcCondExpr* cond_expr = arena->alloc(arena, sizeof(FblcCondExpr));
    FblcsCondExpr* scond_expr = arena->alloc(arena, sizeof(FblcsCondExpr));
    cond_expr->_base.tag = FBLC_COND_EXPR;
    cond_expr->_base.id = exprv->size;
    scond_expr->_base.loc = loc;
    VectorAppend(arena, *exprv, &scond_expr->_base);
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    cond_expr->select = ParseExpr(arena, toks, false, exprv);
    if (cond_expr->select == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }
    if (!ParseNonZeroArgs(arena, toks, &cond_expr->argv, exprv)) {
      return NULL;
    }
    expr = &cond_expr->_base;
  } else {
    UnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (IsToken(toks, '.')) {
    // This is an access expression of the form: <obj>.<field>
    FblcAccessExpr* access_expr = arena->alloc(arena, sizeof(FblcAccessExpr));
    FblcsAccessExpr* saccess_expr = arena->alloc(arena, sizeof(FblcsAccessExpr));
    access_expr->_base.tag = FBLC_ACCESS_EXPR;
    access_expr->_base.id = exprv->size;
    saccess_expr->_base.loc = loc;
    VectorAppend(arena, *exprv, &saccess_expr->_base);
    access_expr->obj = expr;
    GetToken(toks, '.');
    access_expr->field = FBLC_NULL_ID;
    if (!GetNameToken(arena, toks, "field name", &saccess_expr->field)) {
      return NULL;
    }
    expr = &access_expr->_base;
  }
  return expr;
}

// ParseActn --
//   Parse a process action from the token stream.
//   If in_stmt is true, the action is parsed in a statement context,
//   otherwise the action must be standalone.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the action from.
//   in_stmt - True if parsing an action in the statement context.
//   actnv - Pointer to the vector of actn symbol information for the current declaration.
//   exprv - Pointer to the vector of expr symbol information for the current declaration.
//
// Returns:
//   The parsed process action, or NULL on error.
//
// Side effects:
//   Parses a process action from the token stream, advancing the token stream
//   past the parsed action, not including the trailing semicolon in the case
//   when in_stmt is true.
//   Appends symbol information for the parsed action to the actnv and exprv
//   vectors.
//   In case of an error, an error message is printed to stderr.
static FblcActn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcsActnV* actnv, FblcsExprV* exprv)
{
  FblcsLoc* loc = GetCurrentLoc(arena, toks);
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    FblcActn* actn = ParseActn(arena, toks, true, actnv, exprv);
    if (actn == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
    return actn;
  } else if (IsToken(toks, '$')) {
    // This is an eval action of the form: $(<arg>)
    FblcEvalActn* eval_actn = arena->alloc(arena, sizeof(FblcEvalActn));
    FblcsEvalActn* seval_actn = arena->alloc(arena, sizeof(FblcsEvalActn));
    eval_actn->_base.tag = FBLC_EVAL_ACTN;
    eval_actn->_base.id = actnv->size;
    seval_actn->_base.loc = loc;
    VectorAppend(arena, *actnv, &seval_actn->_base);

    GetToken(toks, '$');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    eval_actn->arg = ParseExpr(arena, toks, false, exprv);
    if (eval_actn->arg == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return &eval_actn->_base;
  } else if (IsToken(toks, '~')) {
    // This is a get action or put action of the form: ~name() or ~name(<arg>)
    GetToken(toks, '~');
    FblcsNameL port;
    if (!GetNameToken(arena, toks, "port", &port)) {
      return NULL;
    }
    if (!GetToken(toks, '(')) {
      return NULL;
    }

    if (IsToken(toks, ')')) {
      FblcGetActn* get_actn = arena->alloc(arena, sizeof(FblcGetActn));
      FblcsGetActn* sget_actn = arena->alloc(arena, sizeof(FblcsGetActn));
      get_actn->_base.tag = FBLC_GET_ACTN;
      get_actn->_base.id = actnv->size;
      sget_actn->_base.loc = loc;
      VectorAppend(arena, *actnv, &sget_actn->_base);
      get_actn->port = FBLC_NULL_ID;
      sget_actn->port.loc = port.loc;
      sget_actn->port.name = port.name;
      GetToken(toks, ')');
      return &get_actn->_base;
    } else {
      FblcPutActn* put_actn = arena->alloc(arena, sizeof(FblcPutActn));
      FblcsPutActn* sput_actn = arena->alloc(arena, sizeof(FblcsPutActn));
      put_actn->_base.tag = FBLC_PUT_ACTN;
      put_actn->_base.id = actnv->size;
      sput_actn->_base.loc = loc;
      VectorAppend(arena, *actnv, &sput_actn->_base);
      put_actn->port = FBLC_NULL_ID;
      sput_actn->port.loc = port.loc;
      sput_actn->port.name = port.name;
      put_actn->arg = ParseExpr(arena, toks, false, exprv);
      if (put_actn->arg == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      return &put_actn->_base;
    }
  } else if (IsNameToken(toks)) {
    FblcsNameL start;
    GetNameToken(arena, toks, "process or type name", &start);

    if (IsToken(toks, '(')) {
      // This is a call action of the form: start(ports, args)
      FblcCallActn* call_actn = arena->alloc(arena, sizeof(FblcCallActn));
      FblcsCallActn* scall_actn = arena->alloc(arena, sizeof(FblcsCallActn));
      call_actn->_base.tag = FBLC_CALL_ACTN;
      call_actn->_base.id = actnv->size;
      scall_actn->_base.loc = loc;
      VectorAppend(arena, *actnv, &scall_actn->_base);
      GetToken(toks, '(');
      call_actn->proc = FBLC_NULL_ID;
      scall_actn->proc.loc = start.loc;
      scall_actn->proc.name = start.name;
      VectorInit(arena, call_actn->portv);
      VectorInit(arena, scall_actn->portv);
      if (!IsToken(toks, ';')) {
        do {
          VectorAppend(arena, call_actn->portv, FBLC_NULL_ID);
          VectorExtend(arena, scall_actn->portv);
          if (!GetNameToken(arena, toks, "port name", scall_actn->portv.xs + scall_actn->portv.size++)) {
            return NULL;
          }
        } while (IsToken(toks, ',') && GetToken(toks, ','));
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      if (!ParseArgs(arena, toks, &call_actn->argv, exprv)) {
        return NULL;
      }
      return &call_actn->_base;
    } else if (in_stmt && IsToken(toks, '<')) {
      // This is a link expression of the form: start <~> get, put; body
      FblcLinkActn* link_actn = arena->alloc(arena, sizeof(FblcLinkActn));
      FblcsLinkActn* slink_actn = arena->alloc(arena, sizeof(FblcsLinkActn));
      link_actn->_base.tag = FBLC_LINK_ACTN;
      link_actn->_base.id = actnv->size;
      slink_actn->_base.loc = loc;
      VectorAppend(arena, *actnv, &slink_actn->_base);
      link_actn->type = FBLC_NULL_ID;
      slink_actn->type.name = start.name;
      slink_actn->type.loc = start.loc;
      GetToken(toks, '<');
      if (!GetToken(toks, '~')) {
        return NULL;
      }
      if (!GetToken(toks, '>')) {
        return NULL;
      }
      if (!GetNameToken(arena, toks, "port name", &slink_actn->get)) {
        return NULL;
      }
      if (!GetToken(toks, ',')) {
        return NULL;
      }
      if (!GetNameToken(arena, toks, "port name", &slink_actn->put)) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      link_actn->body = ParseActn(arena, toks, true, actnv, exprv);
      if (link_actn->body == NULL) {
        return NULL;
      }
      return &link_actn->_base;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is an exec action of the form:
      //   start var0 = actn0, type1 var1 = actn1, ... ; body
      FblcExecActn* exec_actn = arena->alloc(arena, sizeof(FblcExecActn));
      FblcsExecActn* sexec_actn = arena->alloc(arena, sizeof(FblcsExecActn));
      exec_actn->_base.tag = FBLC_EXEC_ACTN;
      exec_actn->_base.id = actnv->size;
      sexec_actn->_base.loc = loc;
      VectorAppend(arena, *actnv, &sexec_actn->_base);
      VectorInit(arena, exec_actn->execv);
      VectorInit(arena, sexec_actn->execv);
      bool first = true;
      while (first || IsToken(toks, ',')) {
        if (first) {
          first = false;
        } else {
          assert(IsToken(toks, ','));
          GetToken(toks, ',');
          if (!GetNameToken(arena, toks, "type name", &start)) {
            return NULL;
          }
        }
        VectorExtend(arena, sexec_actn->execv);
        FblcsTypedName* name = sexec_actn->execv.xs + sexec_actn->execv.size++;
        name->type.loc = start.loc;
        name->type.name = start.name;
        if (!GetNameToken(arena, toks, "variable name", &name->name)) {
          return NULL;
        }
        if (!GetToken(toks, '=')) {
          return NULL;
        }
        VectorExtend(arena, exec_actn->execv);
        FblcExec* exec = exec_actn->execv.xs + exec_actn->execv.size++;
        exec->type = FBLC_NULL_ID;
        exec->actn = ParseActn(arena, toks, false, actnv, exprv);
        if (exec->actn == NULL) {
          return NULL;
        }
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(arena, toks, true, actnv, exprv);
      if (exec_actn->body == NULL) {
        return NULL;
      }
      return &exec_actn->_base;
    } else {
      UnexpectedToken(toks, "The rest of a process starting with a name");
      return NULL;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional action of the form: ?(<expr> ; <proc>, ...)
    FblcCondActn* cond_actn = arena->alloc(arena, sizeof(FblcCondActn));
    FblcsCondActn* scond_actn = arena->alloc(arena, sizeof(FblcsCondActn));
    cond_actn->_base.tag = FBLC_COND_ACTN;
    cond_actn->_base.id = actnv->size;
    scond_actn->_base.loc = loc;
    VectorAppend(arena, *actnv, &scond_actn->_base);
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    cond_actn->select = ParseExpr(arena, toks, false, exprv);
    if (cond_actn->select == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }

    VectorInit(arena, cond_actn->argv);
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }
      FblcActn* arg = ParseActn(arena, toks, false, actnv, exprv);
      if (arg == NULL) {
        return NULL;
      }
      VectorAppend(arena, cond_actn->argv, arg);
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return &cond_actn->_base;
  } else {
    UnexpectedToken(toks, "a process action");
    return NULL;
  }
}

// FblcsParseProgram -- see documentation in fblcs.h.
FblcsProgram* FblcsParseProgram(FblcArena* arena, const char* filename)
{
  TokenStream toks;
  if (!OpenFileTokenStream(&toks, filename)) {
    fprintf(stderr, "failed to open %s.\n", filename);
    return NULL;
  }

  FblcsProgram* sprog = arena->alloc(arena, sizeof(FblcsProgram));
  sprog->program = arena->alloc(arena, sizeof(FblcProgram));
  VectorInit(arena, sprog->program->declv);
  VectorInit(arena, sprog->sdeclv);
  while (!IsEOFToken(&toks)) {
    // All declarations start with the form: <keyword> <name> (...
    static const char* keywords = "'struct', 'union', 'func', or 'proc'";
    FblcsNameL keyword;
    if (!GetNameToken(arena, &toks, keywords, &keyword)) {
      return NULL;
    }

    // Parse the remainder of the declaration, starting after the initial open
    // parenthesis.
    if (FblcsNamesEqual("struct", keyword.name)) {
      // This is a struct declaration of the form: struct name(
      //   type0 field0, type1 field1, ...)
      FblcStructDecl* type = arena->alloc(arena, sizeof(FblcStructDecl));
      FblcsTypeDecl* stype = arena->alloc(arena, sizeof(FblcsTypeDecl));
      type->_base.tag = FBLC_STRUCT_DECL;
      if (!GetNameToken(arena, &toks, "declaration name", &stype->_base.name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      VectorInit(arena, type->fieldv);
      VectorInit(arena, stype->fieldv);
      if (!IsToken(&toks, ')')) {
        while (type->fieldv.size == 0 || IsToken(&toks, ',')) {
          if (type->fieldv.size > 0) {
            GetToken(&toks, ',');
          }
          VectorAppend(arena, type->fieldv, FBLC_NULL_ID);
          VectorExtend(arena, stype->fieldv);
          if (!ParseTypedId(arena, &toks, "field name", stype->fieldv.xs + stype->fieldv.size++)) {
            return NULL;
          }
        }
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      VectorAppend(arena, sprog->program->declv, &type->_base);
      VectorAppend(arena, sprog->sdeclv, &stype->_base);
    } else if (FblcsNamesEqual("union", keyword.name)) {
      // This is a union declaration of the form: union name(
      //   type0 field0, type1 field1, ...)
      FblcUnionDecl* type = arena->alloc(arena, sizeof(FblcUnionDecl));
      FblcsTypeDecl* stype = arena->alloc(arena, sizeof(FblcsTypeDecl));
      type->_base.tag = FBLC_UNION_DECL;
      if (!GetNameToken(arena, &toks, "declaration name", &stype->_base.name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      VectorInit(arena, type->fieldv);
      VectorInit(arena, stype->fieldv);
      while (type->fieldv.size == 0 || IsToken(&toks, ',')) {
        if (type->fieldv.size > 0) {
          GetToken(&toks, ',');
        }
        VectorAppend(arena, type->fieldv, FBLC_NULL_ID);
        VectorExtend(arena, stype->fieldv);
        if (!ParseTypedId(arena, &toks, "field name", stype->fieldv.xs + stype->fieldv.size++)) {
          return NULL;
        }
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      VectorAppend(arena, sprog->program->declv, &type->_base);
      VectorAppend(arena, sprog->sdeclv, &stype->_base);
    } else if (FblcsNamesEqual("func", keyword.name)) {
      // This is a function declaration of the form: func name(
      //   type0 var0, type1 var1, ...; return_type) body
      FblcFuncDecl* func = arena->alloc(arena, sizeof(FblcFuncDecl));
      FblcsFuncDecl* sfunc = arena->alloc(arena, sizeof(FblcsFuncDecl));
      func->_base.tag = FBLC_FUNC_DECL;
      if (!GetNameToken(arena, &toks, "declaration name", &sfunc->_base.name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      VectorInit(arena, func->argv);
      VectorInit(arena, sfunc->argv);
      if (!IsToken(&toks, ';')) {
        while (func->argv.size == 0 || IsToken(&toks, ',')) {
          if (func->argv.size > 0) {
            GetToken(&toks, ',');
          }
          VectorExtend(arena, sfunc->argv);
          if (!ParseTypedId(arena, &toks, "variable name", sfunc->argv.xs + sfunc->argv.size++)) {
            return NULL;
          }
          VectorAppend(arena, func->argv, FBLC_NULL_ID);
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }

      func->return_type = FBLC_NULL_ID;
      if (!GetNameToken(arena, &toks, "type", &sfunc->return_type)) {
        return NULL;
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      VectorInit(arena, sfunc->exprv);
      func->body = ParseExpr(arena, &toks, false, &sfunc->exprv);
      if (func->body == NULL) {
        return NULL;
      }
      VectorAppend(arena, sprog->program->declv, &func->_base);
      VectorAppend(arena, sprog->sdeclv, &sfunc->_base);
    } else if (FblcsNamesEqual("proc", keyword.name)) {
      // This is a process declaration of the form: proc name(
      //   type0 polarity0 port0, type1 polarity1, port1, ... ;
      //   type0 var0, type1 var1, ... ; return_type) body
      FblcProcDecl* proc = arena->alloc(arena, sizeof(FblcProcDecl));
      FblcsProcDecl* sproc = arena->alloc(arena, sizeof(FblcsProcDecl));
      proc->_base.tag = FBLC_PROC_DECL;
      if (!GetNameToken(arena, &toks, "declaration name", &sproc->_base.name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      VectorInit(arena, proc->portv);
      VectorInit(arena, sproc->portv);
      if (!IsToken(&toks, ';')) {
        while (proc->portv.size == 0 || IsToken(&toks, ',')) {
          VectorExtend(arena, proc->portv);
          VectorExtend(arena, sproc->portv);
          if (proc->portv.size > 0 && !GetToken(&toks, ',')) {
            return NULL;
          }
          FblcsTypedName* port_name = sproc->portv.xs + sproc->portv.size++;
          if (!GetNameToken(arena, &toks, "type name", &port_name->type)) {
            return NULL;
          }

          FblcPort* port = proc->portv.xs + proc->portv.size++;
          port->type = FBLC_NULL_ID;
          if (IsToken(&toks, '<')) {
            GetToken(&toks, '<');
            if (!GetToken(&toks, '~')) {
              return NULL;
            }
            port->polarity = FBLC_GET_POLARITY;
          } else if (IsToken(&toks, '~')) {
            GetToken(&toks, '~');
            if (!GetToken(&toks, '>')) {
              return NULL;
            }
            port->polarity = FBLC_PUT_POLARITY;
          } else {
            UnexpectedToken(&toks, "'<~' or '~>'");
            return NULL;
          }

          if (!GetNameToken(arena, &toks, "port name", &port_name->name)) {
            return NULL;
          }
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      VectorInit(arena, proc->argv);
      VectorInit(arena, sproc->argv);
      if (!IsToken(&toks, ';')) {
        while (proc->argv.size == 0 || IsToken(&toks, ',')) {
          if (proc->argv.size > 0) {
            GetToken(&toks, ',');
          }
          VectorExtend(arena, sproc->argv);
          if (!ParseTypedId(arena, &toks, "variable name", sproc->argv.xs + sproc->argv.size++)) {
            return NULL;
          }
          VectorAppend(arena, proc->argv, FBLC_NULL_ID);
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      proc->return_type = FBLC_NULL_ID;
      if (!GetNameToken(arena, &toks, "type", &sproc->return_type)) {
        return NULL;
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      VectorInit(arena, sproc->exprv);
      VectorInit(arena, sproc->actnv);
      proc->body = ParseActn(arena, &toks, false, &sproc->actnv, &sproc->exprv);
      if (proc->body == NULL) {
        return NULL;
      }
      VectorAppend(arena, sprog->program->declv, &proc->_base);
      VectorAppend(arena, sprog->sdeclv, &sproc->_base);
    } else {
      FblcsReportError("Expected %s, but got '%s'.\n", keyword.loc, keywords, keyword.name);
      return NULL;
    }

    if (!GetToken(&toks, ';')) {
      return NULL;
    }
  }
  return sprog;
}

// ParseValueFromToks --
//   Parse an Fblc value from the token stream.
//
// Inputs:
//   sprog - The program environment.
//   type - The type of value to parse.
//   toks - The token stream to parse the program from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The token stream is advanced to the end of the value. In the case of an
//   error, an error message is printed to standard error.
static FblcValue* ParseValueFromToks(FblcArena* arena, FblcsProgram* sprog, FblcTypeId type_id, TokenStream* toks)
{
  FblcsNameL name;
  if (!GetNameToken(arena, toks, "type name", &name)) {
    return NULL;
  }

  FblcsName expected = FblcsDeclName(sprog, type_id);
  if (!FblcsNamesEqual(name.name, expected)) {
    FblcsReportError("Expected %s, but got %s.\n", name.loc, expected, name);
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);
    return NULL;
  }
  arena->free(arena, (void*)name.name);
  arena->free(arena, (void*)name.loc);

  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv.xs[type_id];
  if (type->_base.tag == FBLC_STRUCT_DECL) {
    if (!GetToken(toks, '(')) {
      return NULL;
    }

    // If there is an error constructing the struct value, we need to release
    // the resources allocated for it. FblcRelease requires we don't leave any
    // of the fields of the struct value uninitialized. For that reason,
    // assign something to all fields regardless of error, keeping track of
    // whether we encountered an error as we go.
    FblcValue* value = FblcNewStruct(arena, type->fieldv.size);
    bool err = false;
    for (int i = 0; i < type->fieldv.size; i++) {
      value->fields[i] = NULL;
      if (!err && i > 0 && !GetToken(toks, ',')) {
        err = true;
      }

      if (!err) {
        value->fields[i] = ParseValueFromToks(arena, sprog, type->fieldv.xs[i], toks);
        err = err || value->fields[i] == NULL;
      }
    }

    if (err || !GetToken(toks, ')')) {
      FblcRelease(arena, value);
      return NULL;
    }
    return value;
  } else {
    assert(type->_base.tag == FBLC_UNION_DECL);
    if (!GetToken(toks, ':')) {
      return NULL;
    }

    if (!GetNameToken(arena, toks, "field name", &name)) {
      return NULL;
    }

    FblcFieldId tag = FblcsLookupField(sprog, type_id, name.name);
    if (tag == FBLC_NULL_ID) {
      FblcsReportError("Invalid field %s for type %s.\n", name.loc, name.name, FblcsDeclName(sprog, type_id));
      arena->free(arena, (void*)name.name);
      arena->free(arena, (void*)name.loc);
      return NULL;
    }
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);

    if (!GetToken(toks, '(')) {
      return NULL;
    }
    FblcValue* field = ParseValueFromToks(arena, sprog, type->fieldv.xs[tag], toks);
    if (field == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return FblcNewUnion(arena, type->fieldv.size, tag, field);
  }
}

// FblcsParseValue -- see documentation in fblcs.h
FblcValue* FblcsParseValue(FblcArena* arena, FblcsProgram* sprog, FblcTypeId type, int fd)
{
  TokenStream toks;
  OpenFdTokenStream(&toks, fd, "file descriptor");
  return ParseValueFromToks(arena, sprog, type, &toks);
}

// ParseValueFromString -- see documentation in fblcs.h
FblcValue* FblcsParseValueFromString(FblcArena* arena, FblcsProgram* sprog, FblcTypeId type, const char* string)
{
  TokenStream toks;
  OpenStringTokenStream(&toks, string, string);
  return ParseValueFromToks(arena, sprog, type, &toks);
}
