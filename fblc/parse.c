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
  Loc loc;
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
static bool IsEOFToken(TokenStream* toks);
static bool IsToken(TokenStream* toks, char which);
static bool GetToken(TokenStream* toks, char which);
static bool IsNameToken(TokenStream* toks);
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, SName* name);
static void UnexpectedToken(TokenStream* toks, const char* expected);

static bool ParseNonZeroArgs(FblcArena* arena, TokenStream* toks, FblcLocId* loc_id, Symbols* symbols, size_t* argc, FblcExpr*** argv);
static bool ParseArgs(FblcArena* arena, TokenStream* toks, FblcLocId* loc_id, Symbols* symbols, size_t* argc, FblcExpr*** argv);
static FblcExpr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcLocId* loc_id, Symbols* symbols);
static FblcActn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcLocId* loc_id, Symbols* symbols);

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
//   name - A pointer to an SName that will be filled in with the
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
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, SName* name)
{
  SkipToToken(toks);
  if (IsNameChar(CurrChar(toks))) {
    size_t n;
    char* namestr;
    FblcVectorInit(arena, namestr, n);
    name->loc = arena->alloc(arena, sizeof(Loc));
    name->loc->source = toks->loc.source;
    name->loc->line = toks->loc.line;
    name->loc->col = toks->loc.col;
    while (IsNameChar(CurrChar(toks))) {
      FblcVectorAppend(arena, namestr, n, CurrChar(toks));
      AdvanceChar(toks);
    } 
    FblcVectorAppend(arena, namestr, n, '\0');
    name->name = namestr;
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
  ReportError("Expected %s, but got token of type %s.\n",
      &toks->loc, expected, desc);
}

// ParseNonZeroArgs --
//   Parse a list of one or more arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls, conditional
//   expressions, and process calls.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the arguments from.
//   loc_id - The current location in the program.
//   symbols - The program symbols information.
//   argc - Pointer to the count field of a vector.
//   argv - Pointer to the contents of a vector.
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses arguments from the token stream, advancing the stream past the
//   final ')' token in the argument list.
//   Initializes the argc/argv vector and fills it with the parsed arguments.
//   Updates loc_id and symbol information based on the parsed arguments.
//   In case of an error, an error message is printed to stderr.
static bool ParseNonZeroArgs(FblcArena* arena, TokenStream* toks, FblcLocId* loc_id, Symbols* symbols, size_t* argc, FblcExpr*** argv)
{
  FblcVectorInit(arena, *argv, *argc);
  bool first = true;
  while (first || IsToken(toks, ',')) {
    if (first) {
      first = false;
    } else {
      assert(IsToken(toks, ','));
      GetToken(toks, ',');
    }
    FblcExpr* arg = ParseExpr(arena, toks, false, loc_id, symbols);
    if (arg == NULL) {
      return false;
    }
    FblcVectorAppend(arena, *argv, *argc, arg);
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
//   loc_id - The current location in the program.
//   symbols - The program symbols information.
//   argc - Pointer to the count field of a vector.
//   argv - Pointer to the contents of a vector.
//
// Returns:
//   True on success, false if an error is encountered.
//
// Side effects:
//   Parses arguments from the token stream, advancing the stream past the
//   final ')' token in the argument list.
//   Initializes the argc/argv vector and fills it with the parsed arguments.
//   Updates loc_id and symbol information based on the parsed arguments.
//   In case of an error, an error message is printed to stderr.
static bool ParseArgs(FblcArena* arena, TokenStream* toks, FblcLocId* loc_id, Symbols* symbols, size_t* argc, FblcExpr*** argv)
{
  if (IsToken(toks, ')')) {
    GetToken(toks, ')');
    FblcVectorInit(arena, *argv, *argc);
    return true;
  }
  return ParseNonZeroArgs(arena, toks, loc_id, symbols, argc, argv);
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
//   loc_id - The current location in the program.
//   symbols - The program symbols information.
//
// Returns:
//   The parsed expression, or NULL on error.
//
// Side effects:
//   Parses an expression from the token stream, advancing the token stream
//   past the parsed expression, including the trailing semicolon if in_stmt
//   is true.
//   Updates loc_id and symbol information based on the parsed expression.
//   In case of an error, an error message is printed to stderr.
static FblcExpr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcLocId* loc_id, Symbols* symbols)
{
  if (!IsToken(toks, '{')) {
    SetLocExpr(arena, symbols, (*loc_id)++, &toks->loc);
  }

  FblcExpr* expr = NULL;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    expr = ParseExpr(arena, toks, true, loc_id, symbols);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsNameToken(toks)) {
    SName start;
    GetNameToken(arena, toks, "start of expression", &start);
    if (IsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      FblcAppExpr* app_expr = arena->alloc(arena, sizeof(FblcAppExpr));
      app_expr->tag = FBLC_APP_EXPR;
      app_expr->func = NULL_ID;
      SetLocId(arena, symbols, (*loc_id)++, &start);
      GetToken(toks, '(');
      if (!ParseArgs(arena, toks, loc_id, symbols, &app_expr->argc, &app_expr->argv)) {
        return NULL;
      }
      expr = (FblcExpr*)app_expr;
    } else if (IsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      GetToken(toks, ':');
      FblcUnionExpr* union_expr = arena->alloc(arena, sizeof(FblcUnionExpr));
      union_expr->tag = FBLC_UNION_EXPR;
      SName field;
      if (!GetNameToken(arena, toks, "field name", &field)) {
        return NULL;
      }
      union_expr->type = NULL_ID;
      SetLocId(arena, symbols, (*loc_id)++, &start);
      union_expr->field = NULL_ID;
      SetLocId(arena, symbols, (*loc_id)++, &field);
      if (!GetToken(toks, '(')) {
        return NULL;
      }
      union_expr->arg = ParseExpr(arena, toks, false, loc_id, symbols);
      if (union_expr->arg == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      expr = (FblcExpr*)union_expr;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      FblcLetExpr* let_expr = arena->alloc(arena, sizeof(FblcLetExpr));
      let_expr->tag = FBLC_LET_EXPR;
      SName name;
      GetNameToken(arena, toks, "variable name", &name);
      let_expr->type = NULL_ID;
      SetLocTypedId(arena, symbols, (*loc_id)++, &start, &name);
      if (!GetToken(toks, '=')) {
        return NULL;
      }
      let_expr->def = ParseExpr(arena, toks, false, loc_id, symbols);
      if (let_expr->def == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      let_expr->body = ParseExpr(arena, toks, true, loc_id, symbols);
      if (let_expr->body == NULL) {
        return NULL;
      }
      // Return the expression immediately, because we have already parsed the
      // trailing semicolon.
      return (FblcExpr*)let_expr;
    } else {
      // This is the variable expression: start
      FblcVarExpr* var_expr = arena->alloc(arena, sizeof(FblcVarExpr));
      var_expr->tag = FBLC_VAR_EXPR;
      var_expr->var = NULL_ID;
      SetLocId(arena, symbols, (*loc_id)++, &start);
      expr = (FblcExpr*)var_expr;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional expression of the form: ?(<expr> ; <args>)
    FblcCondExpr* cond_expr = arena->alloc(arena, sizeof(FblcCondExpr));
    cond_expr->tag = FBLC_COND_EXPR;
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    cond_expr->select = ParseExpr(arena, toks, false, loc_id, symbols);
    if (cond_expr->select == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }
    if (!ParseNonZeroArgs(arena, toks, loc_id, symbols, &cond_expr->argc, &cond_expr->argv)) {
      return NULL;
    }
    expr = (FblcExpr*)cond_expr;
  } else if (IsToken(toks, '.')) {
    // This is an access expression of the form: .<field>(<expr>)
    FblcAccessExpr* access_expr = arena->alloc(arena, sizeof(FblcAccessExpr));
    access_expr->tag = FBLC_ACCESS_EXPR;
    GetToken(toks, '.');
    SName field;
    if (!GetNameToken(arena, toks, "field name", &field)) {
      return NULL;
    }
    access_expr->field = NULL_ID;
    SetLocId(arena, symbols, (*loc_id)++, &field);
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    access_expr->arg = ParseExpr(arena, toks, false, loc_id, symbols);
    if (access_expr->arg == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    expr = (FblcExpr*)access_expr;
  } else {
    UnexpectedToken(toks, "an expression");
    return NULL;
  }

  if (in_stmt) {
    if (!GetToken(toks, ';')) {
      return NULL;
    }
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
//   loc_id - The current location in the program.
//   symbols - The program symbols information.
//
// Returns:
//   The parsed process action, or NULL on error.
//
// Side effects:
//   Parses a process action from the token stream, advancing the token stream
//   past the parsed action, including the trailing semicolon if in_stmt
//   is true.
//   Updates loc_id and symbol information based on the parsed action.
//   In case of an error, an error message is printed to stderr.
static FblcActn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt, FblcLocId* loc_id, Symbols* symbols)
{
  if (!IsToken(toks, '{')) {
    SetLocActn(arena, symbols, (*loc_id)++, &toks->loc);
  }
  
  FblcActn* actn = NULL;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    actn = ParseActn(arena, toks, true, loc_id, symbols);
    if (actn == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsToken(toks, '$')) {
    // This is an eval action of the form: $(<arg>)
    FblcEvalActn* eval_actn = arena->alloc(arena, sizeof(FblcEvalActn));
    eval_actn->tag = FBLC_EVAL_ACTN;
    GetToken(toks, '$');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    eval_actn->arg = ParseExpr(arena, toks, false, loc_id, symbols);
    if (eval_actn->arg == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    actn = (FblcActn*)eval_actn;
  } else if (IsToken(toks, '~')) {
    // This is a get action or put action of the form: ~name() or ~name(<arg>)
    GetToken(toks, '~');
    SName port;
    if (!GetNameToken(arena, toks, "port", &port)) {
      return NULL;
    }
    SetLocId(arena, symbols, (*loc_id)++, &port);
    if (!GetToken(toks, '(')) {
      return NULL;
    }

    if (IsToken(toks, ')')) {
      FblcGetActn* get_actn = arena->alloc(arena, sizeof(FblcGetActn));
      get_actn->tag = FBLC_GET_ACTN;
      get_actn->port = NULL_ID;
      GetToken(toks, ')');
      actn = (FblcActn*)get_actn;
    } else {
      FblcPutActn* put_actn = arena->alloc(arena, sizeof(FblcPutActn));
      put_actn->tag = FBLC_PUT_ACTN;
      put_actn->port = NULL_ID;
      put_actn->arg = ParseExpr(arena, toks, false, loc_id, symbols);
      if (put_actn->arg == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      actn = (FblcActn*)put_actn;
    }
  } else if (IsNameToken(toks)) {
    SName start;
    GetNameToken(arena, toks, "process or type name", &start);

    if (IsToken(toks, '(')) {
      // This is a call action of the form: start(ports, args)
      FblcCallActn* call_actn = arena->alloc(arena, sizeof(FblcCallActn));
      call_actn->tag = FBLC_CALL_ACTN;
      GetToken(toks, '(');
      call_actn->proc = NULL_ID;
      SetLocId(arena, symbols, (*loc_id)++, &start);
      FblcVectorInit(arena, call_actn->portv, call_actn->portc);
      if (!IsToken(toks, ';')) {
        do {
          SName port;
          if (!GetNameToken(arena, toks, "port name", &port)) {
            return NULL;
          }
          FblcVectorAppend(arena, call_actn->portv, call_actn->portc, NULL_ID);
          SetLocId(arena, symbols, (*loc_id)++, &port);
        } while (IsToken(toks, ',') && GetToken(toks, ','));
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      if (!ParseArgs(arena, toks, loc_id, symbols, &call_actn->argc, &call_actn->argv)) {
        return NULL;
      }
      actn = (FblcActn*)call_actn;
    } else if (in_stmt && IsToken(toks, '<')) {
      // This is a link expression of the form: start <~> get, put; body
      FblcLinkActn* link_actn = arena->alloc(arena, sizeof(FblcLinkActn));
      link_actn->tag = FBLC_LINK_ACTN;
      link_actn->type = NULL_ID;
      GetToken(toks, '<');
      if (!GetToken(toks, '~')) {
        return NULL;
      }
      if (!GetToken(toks, '>')) {
        return NULL;
      }
      SName get;
      if (!GetNameToken(arena, toks, "port name", &get)) {
        return NULL;
      }
      if (!GetToken(toks, ',')) {
        return NULL;
      }
      SName put;
      if (!GetNameToken(arena, toks, "port name", &put)) {
        return NULL;
      }
      SetLocLink(arena, symbols, (*loc_id)++, &start, &get, &put);
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      link_actn->body = ParseActn(arena, toks, true, loc_id, symbols);
      if (link_actn->body == NULL) {
        return NULL;
      }
      return (FblcActn*)link_actn;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is an exec action of the form:
      //   start var0 = actn0, type1 var1 = actn1, ... ; body
      FblcExecActn* exec_actn = arena->alloc(arena, sizeof(FblcExecActn));
      exec_actn->tag = FBLC_EXEC_ACTN;
      FblcVectorInit(arena, exec_actn->execv, exec_actn->execc);
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
        SName name;
        if (!GetNameToken(arena, toks, "variable name", &name)) {
          return NULL;
        }
        SetLocTypedId(arena, symbols, (*loc_id)++, &start, &name);
        if (!GetToken(toks, '=')) {
          return NULL;
        }
        FblcVectorExtend(arena, exec_actn->execv, exec_actn->execc);
        FblcExec* exec = exec_actn->execv + exec_actn->execc++;
        exec->type = NULL_ID;
        exec->actn = ParseActn(arena, toks, false, loc_id, symbols);
        if (exec->actn == NULL) {
          return NULL;
        }
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(arena, toks, true, loc_id, symbols);
      if (exec_actn->body == NULL) {
        return NULL;
      }
      return (FblcActn*)exec_actn;
    } else {
      UnexpectedToken(toks, "The rest of a process starting with a name");
      return NULL;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional action of the form: ?(<expr> ; <proc>, ...)
    FblcCondActn* cond_actn = arena->alloc(arena, sizeof(FblcCondActn));
    cond_actn->tag = FBLC_COND_ACTN;
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    cond_actn->select = ParseExpr(arena, toks, false, loc_id, symbols);
    if (cond_actn->select == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ';')) {
      return NULL;
    }

    FblcVectorInit(arena, cond_actn->argv, cond_actn->argc);
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }
      FblcActn* arg = ParseActn(arena, toks, false, loc_id, symbols);
      if (arg == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, cond_actn->argv, cond_actn->argc, arg);
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    actn = (FblcActn*)cond_actn;
  } else {
    UnexpectedToken(toks, "a process action");
    return NULL;
  }

  if (in_stmt) {
    if (!GetToken(toks, ';')) {
      return NULL;
    }
  }
  return actn;
}

// ParseProgram --
//   Parse an fblc program from a file.
//
// Inputs:
//   arena - The arena to use for allocations.
//   filename - The name of the file to parse the program from.
//
// Result:
//   The parsed program environment, or NULL on error.
//   Name resolution is not performed; ids throughout the parsed program will
//   be set to NULL_ID in the returned result.
//
// Side effects:
//   A program environment is allocated. In the case of an error, an error
//   message is printed to standard error; the caller is still responsible for
//   freeing (unused) allocations made with the allocator in this case.
SProgram* ParseProgram(FblcArena* arena, const char* filename)
{
  TokenStream toks;
  if (!OpenFileTokenStream(&toks, filename)) {
    fprintf(stderr, "failed to open %s.\n", filename);
    return NULL;
  }

  SProgram* sprog = arena->alloc(arena, sizeof(SProgram));
  sprog->program = arena->alloc(arena, sizeof(FblcProgram));
  sprog->symbols = NewSymbols(arena);
  FblcVectorInit(arena, sprog->program->declv, sprog->program->declc);
  FblcLocId loc_id = 0;
  while (!IsEOFToken(&toks)) {
    // All declarations start with the form: <keyword> <name> (...
    static const char* keywords = "'struct', 'union', 'func', or 'proc'";
    SName keyword;
    if (!GetNameToken(arena, &toks, keywords, &keyword)) {
      return NULL;
    }

    bool is_struct = NamesEqual("struct", keyword.name);
    bool is_union = NamesEqual("union", keyword.name);
    bool is_func = NamesEqual("func", keyword.name);
    bool is_proc = NamesEqual("proc", keyword.name);

    if (is_struct) {
      // This is a struct declaration of the form:
      //   name(type0 field0, type1 field1, ...)
      FblcStructDecl* decl = arena->alloc(arena, sizeof(FblcStructDecl));
      decl->tag = FBLC_STRUCT_DECL;
      SName name;
      if (!GetNameToken(arena, &toks, "type name", &name)) {
        return NULL;
      }
      SetLocDecl(arena, sprog->symbols, loc_id++, &name, sprog->program->declc);
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, decl->fieldv, decl->fieldc);
      if (!IsToken(&toks, ')')) {
        while (decl->fieldc == 0 || IsToken(&toks, ',')) {
          if (decl->fieldc > 0 && !GetToken(&toks, ',')) {
            return NULL;
          }
          SName type;
          if (!GetNameToken(arena, &toks, "type name", &type)) {
            return NULL;
          }
          SName field;
          if (!GetNameToken(arena, &toks, "field name", &field)) {
            return NULL;
          }
          FblcVectorAppend(arena, decl->fieldv, decl->fieldc, NULL_ID);
          SetLocTypedId(arena, sprog->symbols, loc_id++, &type, &field);
        }
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      FblcVectorAppend(arena, sprog->program->declv, sprog->program->declc, (FblcDecl*)decl);
    } else if (is_union) {
      // This is a union declaration of the form:
      //   name(type0 field0, type1 field1, ...)
      FblcUnionDecl* decl = arena->alloc(arena, sizeof(FblcUnionDecl));
      decl->tag = FBLC_UNION_DECL;
      SName name;
      if (!GetNameToken(arena, &toks, "type name", &name)) {
        return NULL;
      }
      SetLocDecl(arena, sprog->symbols, loc_id++, &name, sprog->program->declc);
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, decl->fieldv, decl->fieldc);
      while (decl->fieldc == 0 || IsToken(&toks, ',')) {
        if (decl->fieldc > 0 && !GetToken(&toks, ',')) {
          return NULL;
        }
        SName type;
        if (!GetNameToken(arena, &toks, "type name", &type)) {
          return NULL;
        }
        SName field;
        if (!GetNameToken(arena, &toks, "field name", &field)) {
          return NULL;
        }
        FblcVectorAppend(arena, decl->fieldv, decl->fieldc, NULL_ID);
        SetLocTypedId(arena, sprog->symbols, loc_id++, &type, &field);
      }
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      FblcVectorAppend(arena, sprog->program->declv, sprog->program->declc, (FblcDecl*)decl);
    } else if (is_func) {
      // This is a function declaration of the form:
      //    name(type0 var0, type1 var1, ...; return_type) body
      FblcFuncDecl* func = arena->alloc(arena, sizeof(FblcFuncDecl));
      func->tag = FBLC_FUNC_DECL;
      SName name;
      if (!GetNameToken(arena, &toks, "function name", &name)) {
        return NULL;
      }
      SetLocDecl(arena, sprog->symbols, loc_id++, &name, sprog->program->declc);
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, func->argv, func->argc);
      if (!IsToken(&toks, ';')) {
        while (func->argc == 0 || IsToken(&toks, ',')) {
          if (func->argc > 0 && !GetToken(&toks, ',')) {
            return NULL;
          }
          SName type;
          if (!GetNameToken(arena, &toks, "type name", &type)) {
            return NULL;
          }
          SName var;
          if (!GetNameToken(arena, &toks, "variable name", &var)) {
            return NULL;
          }
          FblcVectorAppend(arena, func->argv, func->argc, NULL_ID);
          SetLocTypedId(arena, sprog->symbols, loc_id++, &type, &var);
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      SName return_type;
      if (!GetNameToken(arena, &toks, "type", &return_type)) {
        return NULL;
      }
      func->return_type = NULL_ID;
      SetLocId(arena, sprog->symbols, loc_id++, &return_type);
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      func->body = ParseExpr(arena, &toks, false, &loc_id, sprog->symbols);
      if (func->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, sprog->program->declv, sprog->program->declc, (FblcDecl*)func);
    } else if (is_proc) {
      // This is a process declaration of the form:
      //   name(type0 polarity0 port0, type1 polarity1, port1, ... ;
      //        type0 var0, type1 var1, ... ; return_type) body
      FblcProcDecl* proc = arena->alloc(arena, sizeof(FblcProcDecl));
      proc->tag = FBLC_PROC_DECL;
      SName name;
      if (!GetNameToken(arena, &toks, "process name", &name)) {
        return NULL;
      }
      SetLocDecl(arena, sprog->symbols, loc_id++, &name, sprog->program->declc);
      if (!GetToken(&toks, '(')) {
        return NULL;
      }
      FblcVectorInit(arena, proc->portv, proc->portc);
      if (!IsToken(&toks, ';')) {
        while (proc->portc == 0 || IsToken(&toks, ',')) {
          FblcVectorExtend(arena, proc->portv, proc->portc);
          if (proc->portc > 0 && !GetToken(&toks, ',')) {
            return NULL;
          }
          SName type;
          if (!GetNameToken(arena, &toks, "type name", &type)) {
            return NULL;
          }

          FblcPort* port = proc->portv + proc->portc++;
          port->type = NULL_ID;
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

          SName port_name;
          if (!GetNameToken(arena, &toks, "port name", &port_name)) {
            return NULL;
          }
          SetLocTypedId(arena, sprog->symbols, loc_id++, &type, &port_name);
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      FblcVectorInit(arena, proc->argv, proc->argc);
      if (!IsToken(&toks, ';')) {
        while (proc->argc == 0 || IsToken(&toks, ',')) {
          if (proc->argc > 0 && !GetToken(&toks, ',')) {
            return NULL;
          }
          SName type;
          if (!GetNameToken(arena, &toks, "type name", &type)) {
            return NULL;
          }
          SName var;
          if (!GetNameToken(arena, &toks, "variable name", &var)) {
            return NULL;
          }
          FblcVectorAppend(arena, proc->argv, proc->argc, NULL_ID);
          SetLocTypedId(arena, sprog->symbols, loc_id++, &type, &var);
        }
      }
      if (!GetToken(&toks, ';')) {
        return NULL;
      }
      SName return_type;
      if (!GetNameToken(arena, &toks, "type", &return_type)) {
        return NULL;
      }
      proc->return_type = NULL_ID;
      SetLocId(arena, sprog->symbols, loc_id++, &return_type);
      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      proc->body = ParseActn(arena, &toks, false, &loc_id, sprog->symbols);
      if (proc->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, sprog->program->declv, sprog->program->declc, (FblcDecl*)proc);
    } else {
      ReportError("Expected %s, but got '%s'.\n", keyword.loc, keywords, keyword.name);
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
static FblcValue* ParseValueFromToks(FblcArena* arena, SProgram* sprog, FblcTypeId type_id, TokenStream* toks)
{
  SName name;
  if (!GetNameToken(arena, toks, "type name", &name)) {
    return NULL;
  }

  Name expected = DeclName(sprog, type_id);
  if (!NamesEqual(name.name, expected)) {
    ReportError("Expected %s, but got %s.\n", name.loc, expected, name);
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);
    return NULL;
  }
  arena->free(arena, (void*)name.name);
  arena->free(arena, (void*)name.loc);

  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];
  if (type->tag == FBLC_STRUCT_DECL) {
    if (!GetToken(toks, '(')) {
      return NULL;
    }

    // If there is an error constructing the struct value, we need to release
    // the resources allocated for it. FblcRelease requires we don't leave any
    // of the fields of the struct value uninitialized. For that reason,
    // assign something to all fields regardless of error, keeping track of
    // whether we encountered an error as we go.
    FblcValue* value = FblcNewStruct(arena, type->fieldc);
    bool err = false;
    for (int i = 0; i < type->fieldc; i++) {
      value->fields[i] = NULL;
      if (!err && i > 0 && !GetToken(toks, ',')) {
        err = true;
      }

      if (!err) {
        value->fields[i] = ParseValueFromToks(arena, sprog, type->fieldv[i], toks);
        err = err || value->fields[i] == NULL;
      }
    }

    if (err || !GetToken(toks, ')')) {
      FblcRelease(arena, value);
      return NULL;
    }
    return value;
  } else {
    assert(type->tag == FBLC_UNION_DECL);
    if (!GetToken(toks, ':')) {
      return NULL;
    }

    if (!GetNameToken(arena, toks, "field name", &name)) {
      return NULL;
    }

    FblcFieldId tag = SLookupField(sprog, type_id, name.name);
    if (tag == NULL_ID) {
      ReportError("Invalid field %s for type %s.\n", name.loc, name.name, DeclName(sprog, type_id));
      arena->free(arena, (void*)name.name);
      arena->free(arena, (void*)name.loc);
      return NULL;
    }
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);

    if (!GetToken(toks, '(')) {
      return NULL;
    }
    FblcValue* field = ParseValueFromToks(arena, sprog, type->fieldv[tag], toks);
    if (field == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }
    return FblcNewUnion(arena, type->fieldc, tag, field);
  }
}

// ParseValue --
//   Parse an fblc value from a file.
//
// Inputs:
//   sprog - The program environment.
//   type_id - The type id of value to parse.
//   fd - A file descriptor of a file open for reading.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The value is read from the given file descriptor. In the case of an
//   error, an error message is printed to standard error.
FblcValue* ParseValue(FblcArena* arena, SProgram* sprog, FblcTypeId type_id, int fd)
{
  TokenStream toks;
  OpenFdTokenStream(&toks, fd, "file descriptor");
  return ParseValueFromToks(arena, sprog, type_id, &toks);
}

// ParseValueFromString --
//   Parse an fblc value from a string.
//
// Inputs:
//   sprog - The program environment.
//   type_id - The type id of value to parse.
//   string - The string to parse the value from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   In the case of an error, an error message is printed to standard error.
FblcValue* ParseValueFromString(FblcArena* arena, SProgram* sprog, FblcTypeId type_id, const char* string)
{
  TokenStream toks;
  OpenStringTokenStream(&toks, string, string);
  return ParseValueFromToks(arena, sprog, type_id, &toks);
}
