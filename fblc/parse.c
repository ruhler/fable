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
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, FblcsName* name);
static void UnexpectedToken(TokenStream* toks, const char* expected);

static bool ParseTypedId(FblcArena* arena, TokenStream* toks, const char* expected, FblcsArg* name);
static bool ParseNonZeroArgs(FblcArena* arena, TokenStream* toks, FblcsExprV* argv);
static bool ParseArgs(FblcArena* arena, TokenStream* toks, FblcsExprV* argv);
static FblcsExpr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt);
static FblcsActn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt);

static FblcValue* ParseValueFromToks(FblcArena* arena, FblcsProgram* prog, FblcsName* typename, TokenStream* toks);

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
  FblcsLoc* loc = FBLC_ALLOC(arena, FblcsLoc);
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
//   name - A pointer to an FblcsName that will be filled in with the
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
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, FblcsName* name)
{
  SkipToToken(toks);
  if (IsNameChar(CurrChar(toks))) {
    struct { size_t size; char* xs; } namev;
    FblcVectorInit(arena, namev);
    name->loc = GetCurrentLoc(arena, toks);
    while (IsNameChar(CurrChar(toks))) {
      FblcVectorAppend(arena, namev, CurrChar(toks));
      AdvanceChar(toks);
    } 
    FblcVectorAppend(arena, namev, '\0');
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
static bool ParseTypedId(FblcArena* arena, TokenStream* toks, const char* expected, FblcsArg* name)
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
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses arguments from the token stream, advancing the stream past the
//   final ')' token in the argument list.
//   Initializes the argv vector and fills it with the parsed arguments.
//   In case of an error, an error message is printed to stderr.
static bool ParseNonZeroArgs(FblcArena* arena, TokenStream* toks, FblcsExprV* argv)
{
  FblcVectorInit(arena, *argv);
  bool first = true;
  while (first || IsToken(toks, ',')) {
    if (first) {
      first = false;
    } else {
      assert(IsToken(toks, ','));
      GetToken(toks, ',');
    }
    FblcsExpr* arg = ParseExpr(arena, toks, false);
    if (arg == NULL) {
      return false;
    }
    FblcVectorAppend(arena, *argv, arg);
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
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses arguments from the token stream, advancing the stream past the
//   final ')' token in the argument list.
//   Initializes the argv vector and fills it with the parsed arguments.
//   In case of an error, an error message is printed to stderr.
static bool ParseArgs(FblcArena* arena, TokenStream* toks, FblcsExprV* argv)
{
  if (IsToken(toks, ')')) {
    GetToken(toks, ')');
    FblcVectorInit(arena, *argv);
    return true;
  }
  return ParseNonZeroArgs(arena, toks, argv);
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
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Parses an expression from the token stream, advancing the token stream
//   past the parsed expression, not including the trailing semicolon in the
//   case when in_stmt is true.
//   In case of an error, an error message is printed to stderr.
static FblcsExpr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt)
{
  FblcsExpr* expr = NULL;
  FblcsLoc* loc = GetCurrentLoc(arena, toks);
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    expr = ParseExpr(arena, toks, true);
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
    FblcsName start;
    GetNameToken(arena, toks, "start of expression", &start);
    if (IsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      FblcsAppExpr* app_expr = FBLC_ALLOC(arena, FblcsAppExpr);
      app_expr->_base.tag = FBLCS_APP_EXPR;
      app_expr->_base.loc = loc;
      app_expr->func.loc = start.loc;
      app_expr->func.name = start.name;
      GetToken(toks, '(');
      if (!ParseArgs(arena, toks, &app_expr->argv)) {
        return NULL;
      }
      expr = &app_expr->_base;
    } else if (IsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      GetToken(toks, ':');
      FblcsUnionExpr* union_expr = FBLC_ALLOC(arena, FblcsUnionExpr);
      union_expr->_base.tag = FBLCS_UNION_EXPR;
      union_expr->_base.loc = loc;
      union_expr->type.loc = start.loc;
      union_expr->type.name = start.name;
      union_expr->field.id = FBLC_NULL_ID;
      if (!GetNameToken(arena, toks, "field name", &union_expr->field.name)) {
        return NULL;
      }

      if (!GetToken(toks, '(')) {
        return NULL;
      }
      union_expr->arg = ParseExpr(arena, toks, false);
      if (union_expr->arg == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      expr = &union_expr->_base;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      FblcsLetExpr* let_expr = FBLC_ALLOC(arena, FblcsLetExpr);
      let_expr->_base.tag = FBLCS_LET_EXPR;
      let_expr->_base.loc = loc;
      let_expr->type.loc = start.loc;
      let_expr->type.name = start.name;
      GetNameToken(arena, toks, "variable name", &let_expr->name);
      if (!GetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(arena, toks, false);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(arena, toks, true);
      if (let_expr->body == NULL) {
        return NULL;
      }
      return &let_expr->_base;
    } else {
      // This is the variable expression: start
      FblcsVarExpr* var_expr = FBLC_ALLOC(arena, FblcsVarExpr);
      var_expr->_base.tag = FBLCS_VAR_EXPR;
      var_expr->_base.loc = loc;
      var_expr->var.name.loc = start.loc;
      var_expr->var.name.name = start.name;
      var_expr->var.id = FBLC_NULL_ID;
      expr = &var_expr->_base;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional expression of the form:
    //    ?(<expr> ; <name>: <arg>, ...)
    FblcsCondExpr* cond_expr = FBLC_ALLOC(arena, FblcsCondExpr);
    cond_expr->_base.tag = FBLCS_COND_EXPR;
    cond_expr->_base.loc = loc;
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    cond_expr->select = ParseExpr(arena, toks, false);
    if (cond_expr->select == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }
    FblcVectorInit(arena, cond_expr->argv);
    FblcVectorInit(arena, cond_expr->tagv);
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }
      if (!GetNameToken(arena, toks, "field name", FblcVectorExtend(arena, cond_expr->tagv))) {
        return NULL;
      }
      if (!GetToken(toks, ':')) {
        return NULL;
      }
      FblcsExpr* arg = ParseExpr(arena, toks, false);
      if (arg == NULL) {
        return false;
      }
      FblcVectorAppend(arena, cond_expr->argv, arg);
    }
    if (!GetToken(toks, ')')) {
      return false;
    }
    expr = &cond_expr->_base;
  } else {
    UnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (IsToken(toks, '.')) {
    // This is an access expression of the form: <obj>.<field>
    FblcsAccessExpr* access_expr = FBLC_ALLOC(arena, FblcsAccessExpr);
    access_expr->_base.tag = FBLCS_ACCESS_EXPR;
    access_expr->_base.loc = loc;
    access_expr->obj = expr;
    access_expr->field.id = FBLC_NULL_ID;
    GetToken(toks, '.');
    if (!GetNameToken(arena, toks, "field name", &access_expr->field.name)) {
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
//
// Returns:
//   The parsed process action, or NULL on error.
//
// Side effects:
//   Parses a process action from the token stream, advancing the token stream
//   past the parsed action, not including the trailing semicolon in the case
//   when in_stmt is true.
//   In case of an error, an error message is printed to stderr.
static FblcsActn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt)
{
  FblcsLoc* loc = GetCurrentLoc(arena, toks);
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    FblcsActn* actn = ParseActn(arena, toks, true);
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
    FblcsEvalActn* eval_actn = FBLC_ALLOC(arena, FblcsEvalActn);
    eval_actn->_base.tag = FBLCS_EVAL_ACTN;
    eval_actn->_base.loc = loc;

    GetToken(toks, '$');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    eval_actn->arg = ParseExpr(arena, toks, false);
    if (eval_actn->arg == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return &eval_actn->_base;
  } else if (IsToken(toks, '-')) {
    // This is a get action of the form: -name()
    FblcsGetActn* get_actn = FBLC_ALLOC(arena, FblcsGetActn);
    get_actn->_base.tag = FBLCS_GET_ACTN;
    get_actn->_base.loc = loc;
    get_actn->port.id = FBLC_NULL_ID;

    GetToken(toks, '-');
    if (!GetNameToken(arena, toks, "port", &get_actn->port.name)) {
      return NULL;
    }
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return &get_actn->_base;
  } else if (IsToken(toks, '+')) {
    // This is a put action of the form: +name(<arg>)
    FblcsPutActn* put_actn = FBLC_ALLOC(arena, FblcsPutActn);
    put_actn->_base.tag = FBLC_PUT_ACTN;
    put_actn->_base.loc = loc;
    put_actn->port.id = FBLC_NULL_ID;

    GetToken(toks, '+');
    if (!GetNameToken(arena, toks, "port", &put_actn->port.name)) {
      return NULL;
    }
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    put_actn->arg = ParseExpr(arena, toks, false);
    if (put_actn->arg == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return &put_actn->_base;
  } else if (IsNameToken(toks)) {
    FblcsName start;
    GetNameToken(arena, toks, "process or type name", &start);

    if (IsToken(toks, '(')) {
      // This is a call action of the form: start(ports, args)
      FblcsCallActn* call_actn = FBLC_ALLOC(arena, FblcsCallActn);
      call_actn->_base.tag = FBLC_CALL_ACTN;
      call_actn->_base.loc = loc;
      GetToken(toks, '(');
      call_actn->proc.loc = start.loc;
      call_actn->proc.name = start.name;
      FblcVectorInit(arena, call_actn->portv);
      if (!IsToken(toks, ';')) {
        do {
          FblcsId* port = FblcVectorExtend(arena, call_actn->portv);
          port->id = FBLC_NULL_ID;
          if (!GetNameToken(arena, toks, "port name", &port->name)) {
            return NULL;
          }
        } while (IsToken(toks, ',') && GetToken(toks, ','));
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      if (!ParseArgs(arena, toks, &call_actn->argv)) {
        return NULL;
      }
      return &call_actn->_base;
    } else if (in_stmt && IsToken(toks, '+')) {
      // This is a link expression of the form: start +- put, get; body
      FblcsLinkActn* link_actn = FBLC_ALLOC(arena, FblcsLinkActn);
      link_actn->_base.tag = FBLC_LINK_ACTN;
      link_actn->_base.loc = loc;
      link_actn->type.name = start.name;
      link_actn->type.loc = start.loc;
      GetToken(toks, '+');
      if (!GetToken(toks, '-')) {
        return NULL;
      }
      if (!GetNameToken(arena, toks, "port name", &link_actn->put)) {
        return NULL;
      }
      if (!GetToken(toks, ',')) {
        return NULL;
      }
      if (!GetNameToken(arena, toks, "port name", &link_actn->get)) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      link_actn->body = ParseActn(arena, toks, true);
      if (link_actn->body == NULL) {
        return NULL;
      }
      return &link_actn->_base;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is an exec action of the form:
      //   start var0 = actn0, type1 var1 = actn1, ... ; body
      FblcsExecActn* exec_actn = FBLC_ALLOC(arena, FblcsExecActn);
      exec_actn->_base.tag = FBLC_EXEC_ACTN;
      exec_actn->_base.loc = loc;
      FblcVectorInit(arena, exec_actn->execv);
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
        FblcsExec* exec = FblcVectorExtend(arena, exec_actn->execv);
        exec->type.loc = start.loc;
        exec->type.name = start.name;
        if (!GetNameToken(arena, toks, "variable name", &exec->name)) {
          return NULL;
        }
        if (!GetToken(toks, '=')) {
          return NULL;
        }
        exec->actn = ParseActn(arena, toks, false);
        if (exec->actn == NULL) {
          return NULL;
        }
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(arena, toks, true);
      if (exec_actn->body == NULL) {
        return NULL;
      }
      return &exec_actn->_base;
    } else {
      UnexpectedToken(toks, "The rest of a process starting with a name");
      return NULL;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional action of the form:
    //  ?(<expr> ; <name>: <proc>, ...)
    FblcsCondActn* cond_actn = FBLC_ALLOC(arena, FblcsCondActn);
    cond_actn->_base.tag = FBLC_COND_ACTN;
    cond_actn->_base.loc = loc;
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    cond_actn->select = ParseExpr(arena, toks, false);
    if (cond_actn->select == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }

    FblcVectorInit(arena, cond_actn->argv);
    FblcVectorInit(arena, cond_actn->tagv);
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }
      if (!GetNameToken(arena, toks, "field name", FblcVectorExtend(arena, cond_actn->tagv))) {
        return NULL;
      }
      if (!GetToken(toks, ':')) {
        return NULL;
      }
      FblcsActn* arg = ParseActn(arena, toks, false);
      if (arg == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, cond_actn->argv, arg);
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

  FblcsProgram* prog = FBLC_ALLOC(arena, FblcsProgram);
  FblcVectorInit(arena, prog->typev);
  FblcVectorInit(arena, prog->funcv);
  FblcVectorInit(arena, prog->procv);
  while (!IsEOFToken(&toks)) {
    // All declarations start with the form: <keyword> <name> (...
    static const char* keywords = "'struct', 'union', 'func', or 'proc'";
    FblcsName keyword;
    if (!GetNameToken(arena, &toks, keywords, &keyword)) {
      return NULL;
    }

    // Parse the remainder of the declaration, starting after the initial open
    // parenthesis.
    if (FblcsNamesEqual("struct", keyword.name)) {
      // This is a struct declaration of the form: struct name(
      //   type0 field0, type1 field1, ...)
      FblcsType* type = FBLC_ALLOC(arena, FblcsType);
      type->kind = FBLCS_STRUCT_KIND;
      if (!GetNameToken(arena, &toks, "declaration name", &type->name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, type->fieldv);
      if (!IsToken(&toks, ')')) {
        while (type->fieldv.size == 0 || IsToken(&toks, ',')) {
          if (type->fieldv.size > 0) {
            GetToken(&toks, ',');
          }
          FblcsArg* field = FblcVectorExtend(arena, type->fieldv);
          if (!ParseTypedId(arena, &toks, "field name", field)) {
            return NULL;
          }
        }
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      FblcVectorAppend(arena, prog->typev, type);
    } else if (FblcsNamesEqual("union", keyword.name)) {
      // This is a union declaration of the form: union name(
      //   type0 field0, type1 field1, ...)
      FblcsType* type = FBLC_ALLOC(arena, FblcsType);
      type->kind = FBLCS_UNION_KIND;
      if (!GetNameToken(arena, &toks, "declaration name", &type->name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, type->fieldv);
      while (type->fieldv.size == 0 || IsToken(&toks, ',')) {
        if (type->fieldv.size > 0) {
          GetToken(&toks, ',');
        }
        FblcsArg* field = FblcVectorExtend(arena, type->fieldv);
        if (!ParseTypedId(arena, &toks, "field name", field)) {
          return NULL;
        }
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      FblcVectorAppend(arena, prog->typev, type);
    } else if (FblcsNamesEqual("func", keyword.name)) {
      // This is a function declaration of the form: func name(
      //   type0 var0, type1 var1, ...; return_type) body
      FblcsFunc* func = FBLC_ALLOC(arena, FblcsFunc);
      if (!GetNameToken(arena, &toks, "declaration name", &func->name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, func->argv);
      if (!IsToken(&toks, ';')) {
        while (func->argv.size == 0 || IsToken(&toks, ',')) {
          if (func->argv.size > 0) {
            GetToken(&toks, ',');
          }
          FblcsArg* arg = FblcVectorExtend(arena, func->argv);
          if (!ParseTypedId(arena, &toks, "variable name", arg)) {
            return NULL;
          }
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(arena, &toks, "type", &func->return_type)) {
        return NULL;
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      func->body = ParseExpr(arena, &toks, false);
      if (func->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, prog->funcv, func);
    } else if (FblcsNamesEqual("proc", keyword.name)) {
      // This is a process declaration of the form: proc name(
      //   type0 polarity0 port0, type1 polarity1, port1, ... ;
      //   type0 var0, type1 var1, ... ; return_type) body
      FblcsProc* proc = FBLC_ALLOC(arena, FblcsProc);
      if (!GetNameToken(arena, &toks, "declaration name", &proc->name)) {
        return NULL;
      }
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, proc->portv);
      if (!IsToken(&toks, ';')) {
        while (proc->portv.size == 0 || IsToken(&toks, ',')) {
          if (proc->portv.size > 0) {
            GetToken(&toks, ',');
          }

          FblcsPort* port = FblcVectorExtend(arena, proc->portv);
          if (!GetNameToken(arena, &toks, "type name", &port->type)) {
            return NULL;
          }

          if (IsToken(&toks, '-')) {
            GetToken(&toks, '-');
            port->polarity = FBLCS_GET_POLARITY;
          } else if (IsToken(&toks, '+')) {
            GetToken(&toks, '+');
            port->polarity = FBLCS_PUT_POLARITY;
          } else {
            UnexpectedToken(&toks, "'-' or '+'");
            return NULL;
          }

          if (!GetNameToken(arena, &toks, "port name", &port->name)) {
            return NULL;
          }
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      FblcVectorInit(arena, proc->argv);
      if (!IsToken(&toks, ';')) {
        while (proc->argv.size == 0 || IsToken(&toks, ',')) {
          if (proc->argv.size > 0) {
            GetToken(&toks, ',');
          }
          FblcsArg* var = FblcVectorExtend(arena, proc->argv);
          if (!ParseTypedId(arena, &toks, "variable name", var)) {
            return NULL;
          }
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      if (!GetNameToken(arena, &toks, "type", &proc->return_type)) {
        return NULL;
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      proc->body = ParseActn(arena, &toks, false);
      if (proc->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, prog->procv, proc);
    } else {
      FblcsReportError("Expected %s, but got '%s'.\n", keyword.loc, keywords, keyword.name);
      return NULL;
    }

    if (!GetToken(&toks, ';')) {
      return NULL;
    }
  }
  return prog;
}

// ParseValueFromToks --
//   Parse an Fblc value from the token stream.
//
// Inputs:
//   prog - The program environment.
//   typename - The name of the type of value to parse.
//   toks - The token stream to parse the program from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The token stream is advanced to the end of the value. In the case of an
//   error, an error message is printed to standard error.
static FblcValue* ParseValueFromToks(FblcArena* arena, FblcsProgram* prog, FblcsName* typename, TokenStream* toks)
{
  FblcsName name;
  if (!GetNameToken(arena, toks, "type name", &name)) {
    return NULL;
  }

  if (!FblcsNamesEqual(name.name, typename->name)) {
    FblcsReportError("Expected %s, but got %s.\n", name.loc, typename->name, name);
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);
    return NULL;
  }
  arena->free(arena, (void*)name.name);
  arena->free(arena, (void*)name.loc);

  FblcsType* type = FblcsLookupType(prog, typename->name);
  if (type == NULL) {
    FblcsReportError("Unable to find definition of type %s.\n", typename->loc, typename->name);
    return NULL;
  }

  if (type->kind == FBLCS_STRUCT_KIND) {
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
        value->fields[i] = ParseValueFromToks(arena, prog, &type->fieldv.xs[i].type, toks);
        err = err || value->fields[i] == NULL;
      }
    }

    if (err || !GetToken(toks, ')')) {
      FblcRelease(arena, value);
      return NULL;
    }
    return value;
  } else {
    assert(type->kind == FBLCS_UNION_KIND);
    if (!GetToken(toks, ':')) {
      return NULL;
    }

    if (!GetNameToken(arena, toks, "field name", &name)) {
      return NULL;
    }

    FblcFieldId tag = FBLC_NULL_ID;
    for (size_t i = 0; i < type->fieldv.size; ++i) {
      if (FblcsNamesEqual(name.name, type->fieldv.xs[i].name.name)) {
        tag = i;
        break;
      }
    }

    if (tag == FBLC_NULL_ID) {
      FblcsReportError("Invalid field %s for type %s.\n", name.loc, name.name, type->name.name);
      arena->free(arena, (void*)name.name);
      arena->free(arena, (void*)name.loc);
      return NULL;
    }
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);

    if (!GetToken(toks, '(')) {
      return NULL;
    }

    FblcValue* field = ParseValueFromToks(arena, prog, &type->fieldv.xs[tag].type, toks);
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
FblcValue* FblcsParseValue(FblcArena* arena, FblcsProgram* prog, FblcsName* typename, int fd)
{
  TokenStream toks;
  OpenFdTokenStream(&toks, fd, "file descriptor");
  return ParseValueFromToks(arena, prog, typename, &toks);
}

// ParseValueFromString -- see documentation in fblcs.h
FblcValue* FblcsParseValueFromString(FblcArena* arena, FblcsProgram* prog, FblcsName* typename, const char* string)
{
  TokenStream toks;
  OpenStringTokenStream(&toks, string, string);
  return ParseValueFromToks(arena, prog, typename, &toks);
}
