// Parser.c --
//
//   This file implements routines to parse an fblc program from a token
//   stream into abstract syntax form.

#include <assert.h>     // for assert
#include <ctype.h>        // for isalnum, isspace
#include <fcntl.h>        // for open
#include <sys/stat.h>     // for open
#include <sys/types.h>    // for ssize_t, open
#include <unistd.h>       // for read, close

#include "fblct.h"

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

  // next is the character after the character currently at the front of the
  // stream. It is valid only if has_next is true, in which case the next
  // character has already been read from the file or string.
  // has_next implies has_curr.
  bool has_next;
  int next;

  // Location information for the next token for the purposes of error
  // reporting.
  Loc loc;
} TokenStream;

#define MAX_TOK_DESC_LEN 5
static int CurrChar(TokenStream* toks);
static int NextChar(TokenStream* toks);
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
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, LocName* name);
static void UnexpectedToken(TokenStream* toks, const char* expected);

static int ParseFields(FblcArena* arena, TokenStream* toks, Field** plist);
static int ParsePorts(FblcArena* arena, TokenStream* toks, Port** ports);
static int ParseArgs(FblcArena* arena, TokenStream* toks, Expr*** plist);
static Expr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt);
static Actn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt);

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
    assert(!toks->has_next);
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

// NextChar --
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
static int NextChar(TokenStream* toks)
{
  if (!toks->has_next) {
    if (CurrChar(toks) == EOF) {
      return EOF;
    }
    assert(toks->has_curr);
    if (toks->fd != -1) {
      char c;
      if (read(toks->fd, &c, 1) == 1) {
        toks->next = c;
      } else {
        toks->next = EOF;
      }
    } else {
      assert(toks->string != NULL);
      if (*toks->string == '\0') {
        toks->next = EOF;
      } else {
        toks->next = *toks->string++;
      }
    }
    toks->has_next = true;
  }
  return toks->next;
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

    if (toks->has_next) {
      assert(toks->has_curr);
      toks->curr = toks->next;
      toks->has_next = false;
    } else {
      toks->has_curr = false;
    }
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
  toks->has_next = false;
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
  toks->has_next = false;
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
//   name - A pointer to an LocName that will be filled in with the
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
static bool GetNameToken(FblcArena* arena, TokenStream* toks, const char* expected, LocName* name)
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

// ParseFields -
//   Parse fields in the form: <type> <name>, <type> <name>, ...
//   This is used for parsing the fields of a struct or union type, and for
//   parsing function input parameters.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the fields from.
//   plist - A pointer to a list of fields to output the parsed fields to.
//
// Returns:
//   The number of fields parsed or -1 on error.
//
// Side effects:
//   *plist is set to point to a list of the parsed fields.
//   The token stream is advanced past the tokens describing the fields.
//   In case of an error, an error message is printed to standard error.
static int ParseFields(FblcArena* arena, TokenStream* toks, Field** plist)
{
  if (!IsNameToken(toks)) {
    *plist = NULL;
    return 0;
  }

  Field* fieldv;
  int fieldc;
  FblcVectorInit(arena, fieldv, fieldc);
  FblcVectorExtend(arena, fieldv, fieldc);
  Field* field = fieldv + fieldc++;
  field->type_id = UNRESOLVED_ID;
  GetNameToken(arena, toks, "type name", &(field->type));
  if (!GetNameToken(arena, toks, "field name", &(field->name))) {
    return -1;
  }

  int parsed;
  for (parsed = 1; IsToken(toks, ','); parsed++) {
    GetToken(toks, ',');

    FblcVectorExtend(arena, fieldv, fieldc);
    Field* field = fieldv + fieldc++;
    if (!GetNameToken(arena, toks, "type name", &(field->type))) {
      return -1;
    }
    if (!GetNameToken(arena, toks, "field name", &(field->name))) {
      return -1;
    }
  }

  *plist = fieldv;
  return fieldc;
}

// ParsePorts -
//   Parse a list of zero or more ports in the form:
//      <type> <polarity> <name>, <type> <polarity> <name>, ...
//   This is used for parsing the process input port parameters.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the fields from.
//   ports - An out parameter that will be set to the list of parsed ports.
//
// Returns:
//   The number of ports parsed or -1 on error.
//
// Side effects:
//   *ports is set to point to a list parsed ports.
//   The token stream is advanced past the last port token.
//   In case of an error, an error message is printed to standard error.
static int ParsePorts(FblcArena* arena, TokenStream* toks, Port** ports)
{
  if (!IsNameToken(toks)) {
    *ports = NULL;
    return 0;
  }

  Port* portv;
  int portc;
  FblcVectorInit(arena, portv, portc);
  bool first = true;
  while (first || IsToken(toks, ',')) {
    if (first) {
      first = false;
    } else {
      GetToken(toks, ',');
    }

    FblcVectorExtend(arena, portv, portc);
    Port* port = portv + portc++;
    port->type_id = UNRESOLVED_ID;

    // Get the type.
    if (!GetNameToken(arena, toks, "type name", &(port->type))) {
      return -1;
    }

    // Get the polarity.
    if (IsToken(toks, '<')) {
      GetToken(toks, '<');
      if (!GetToken(toks, '~')) {
        return -1;
      }
      port->polarity = POLARITY_GET;
    } else if (IsToken(toks, '~')) {
      GetToken(toks, '~');
      if (!GetToken(toks, '>')) {
        return -1;
      }
      port->polarity = POLARITY_PUT;
    } else {
      UnexpectedToken(toks, "'<~' or '~>'");
      return -1;
    }

    // Get the name.
    if (!GetNameToken(arena, toks, "port name", &(port->name))) {
      return -1;
    }
  }
  *ports = portv;
  return portc;
}

// ParseArgs --
//   Parse a list of zero or more arguments in the form: <expr>, <expr>, ...)
//   This is used for parsing arguments to function calls, conditional
//   expressions, and process calls.
//
// Inputs:
//   arena - The arena to use for allocations.
//   toks - The token stream to parse the arguments from.
//   plist - An out parameter that will be set to the list of parsed args.
//
// Returns:
//   The number of arguments parsed or -1 on error.
//
// Side effects:
//   *plist is set to point to the parsed arguments.
//   The token stream is advanced past the final ')' token in the
//   argument list. In case of an error, an error message is printed to
//   standard error.
static int ParseArgs(FblcArena* arena, TokenStream* toks, Expr*** plist)
{
  Expr** argv;
  size_t argc;
  FblcVectorInit(arena, argv, argc);
  if (!IsToken(toks, ')')) {
    Expr* arg = ParseExpr(arena, toks, false);
    if (arg == NULL) {
      return -1;
    }
    FblcVectorAppend(arena, argv, argc, arg);

    while (IsToken(toks, ',')) {
      GetToken(toks, ',');
      arg = ParseExpr(arena, toks, false);
      if (arg == NULL) {
        return -1;
      }
      FblcVectorAppend(arena, argv, argc, arg);
    }
  }
  if (!GetToken(toks, ')')) {
    return -1;
  }

  *plist = argv;
  return argc;
}

// ParseExpr --
//   Parse an expression from the token stream.
//   As complete an expression as can be will be parsed.
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
//   Advances the token stream past the parsed expression. In case of error,
//   an error message is printed to standard error.
static Expr* ParseExpr(FblcArena* arena, TokenStream* toks, bool in_stmt)
{
  Expr* expr;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    expr = ParseExpr(arena, toks, true);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsNameToken(toks)) {
    LocName start;
    GetNameToken(arena, toks, "start of expression", &start);

    if (IsToken(toks, '(')) {
      // This is an application expression of the form: start(<args>)
      GetToken(toks, '(');

      Expr** args = NULL;
      int argc = ParseArgs(arena, toks, &args);
      if (argc < 0) {
        return NULL;
      }
      AppExpr* app_expr = arena->alloc(arena, sizeof(AppExpr));
      app_expr->tag = FBLC_APP_EXPR;
      app_expr->func.name = start.name;
      app_expr->func.loc = start.loc;
      app_expr->argc = argc;
      app_expr->argv = args;
      app_expr->func_id = UNRESOLVED_ID;
      expr = (Expr*)app_expr;
    } else if (IsToken(toks, ':')) {
      // This is a union expression of the form: start:field(<expr>)
      GetToken(toks, ':');
      UnionExpr* union_expr = arena->alloc(arena, sizeof(UnionExpr));
      union_expr->tag = FBLC_UNION_EXPR;
      union_expr->type.name = start.name;
      union_expr->type.loc = start.loc;
      union_expr->type_id = UNRESOLVED_ID;
      union_expr->field_id = UNRESOLVED_ID;
      if (!GetNameToken(arena, toks, "field name", &(union_expr->field))) {
        return NULL;
      }
      if (!GetToken(toks, '(')) {
        return NULL;
      }
      union_expr->body = ParseExpr(arena, toks, false);
      if (union_expr->body == NULL) {
        return NULL;
      }
      if (!GetToken(toks, ')')) {
        return NULL;
      }
      expr = (Expr*)union_expr;
    } else if (in_stmt && IsNameToken(toks)) {
      // This is a let statement of the form: <type> <name> = <expr>; <stmt>
      LetExpr* let_expr = arena->alloc(arena, sizeof(LetExpr));
      let_expr->tag = FBLC_LET_EXPR;
      let_expr->type.name = start.name;
      let_expr->type.loc = start.loc;
      GetNameToken(arena, toks, "variable name", &(let_expr->name));
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

      // Return the expression immediately, because it is already complete.
      return (Expr*)let_expr;
    } else {
      // This is the variable expression: start
      VarExpr* var_expr = arena->alloc(arena, sizeof(VarExpr));
      var_expr->tag = FBLC_VAR_EXPR;
      var_expr->name.name = start.name;
      var_expr->name.loc = start.loc;
      var_expr->var = UNRESOLVED_ID;
      expr = (Expr*)var_expr;
    }
  } else if (IsToken(toks, '?')) {
    // This is a conditional expression of the form: ?(<expr> ; <args>)
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    Expr* condition = ParseExpr(arena, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }
    
    Expr** args = NULL;
    int argc = ParseArgs(arena, toks, &args);
    if (argc < 0) {
      return NULL;
    }
    CondExpr* cond_expr = arena->alloc(arena, sizeof(CondExpr));
    cond_expr->tag = FBLC_COND_EXPR;
    cond_expr->select = condition;
    cond_expr->argc = argc;
    cond_expr->argv = args;
    expr = (Expr*)cond_expr;
  } else {
    UnexpectedToken(toks, "an expression");
    return NULL;
  }

  while (IsToken(toks, '.')) {
    GetToken(toks, '.');

    // This is an access expression of the form: <expr>.<field>
    AccessExpr* access_expr = arena->alloc(arena, sizeof(AccessExpr));
    access_expr->tag = FBLC_ACCESS_EXPR;
    access_expr->object = expr;
    access_expr->field_id = UNRESOLVED_ID;
    if (!GetNameToken(arena, toks, "field name", &(access_expr->field))) {
      return NULL;
    }
    expr = (Expr*)access_expr;
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
//   As complete an action as can be will be parsed.
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
//   Advances the token stream past the parsed action. In case of error,
//   an error message is printed to standard error.
static Actn* ParseActn(FblcArena* arena, TokenStream* toks, bool in_stmt)
{
  Actn* actn = NULL;
  if (IsToken(toks, '{')) {
    GetToken(toks, '{');
    actn = ParseActn(arena, toks, true);
    if (actn == NULL) {
      return NULL;
    }
    if (!GetToken(toks, '}')) {
      return NULL;
    }
  } else if (IsToken(toks, '$')) {
    // $(<expr>)
    GetToken(toks, '$');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    Expr* expr = ParseExpr(arena, toks, false);
    if (expr == NULL) {
      return NULL;
    }
    if (!GetToken(toks, ')')) {
      return NULL;
    }

    EvalActn* eval_actn = arena->alloc(arena, sizeof(EvalActn));
    eval_actn->tag = EVAL_ACTN;
    eval_actn->expr = expr;
    actn = (Actn*)eval_actn;
  } else if (IsNameToken(toks)) {
    LocName name;
    GetNameToken(arena, toks, "port, process, or type name", &name);

    if (IsToken(toks, '~')) {
      GetToken(toks, '~');
      if (!GetToken(toks, '(')) {
        return NULL;
      }

      if (IsToken(toks, ')')) {
        GetToken(toks, ')');
        GetActn* get_actn = arena->alloc(arena, sizeof(GetActn));
        get_actn->tag = GET_ACTN;
        get_actn->port.loc = name.loc;
        get_actn->port.name = name.name;
        get_actn->port_id = UNRESOLVED_ID;
        actn = (Actn*)get_actn;
      } else {
        Expr* expr = ParseExpr(arena, toks, false);
        if (expr == NULL) {
          return NULL;
        }
        if (!GetToken(toks, ')')) {
          return NULL;
        }

        PutActn* put_actn = arena->alloc(arena, sizeof(PutActn));
        put_actn->tag = PUT_ACTN;
        put_actn->port.loc = name.loc;
        put_actn->port.name = name.name;
        put_actn->expr = expr;
        put_actn->port_id = UNRESOLVED_ID;
        actn = (Actn*)put_actn;
      }
    } else if (IsToken(toks, '(')) {
      GetToken(toks, '(');
      CallActn* call_actn = arena->alloc(arena, sizeof(CallActn));
      call_actn->tag = CALL_ACTN;
      call_actn->proc.loc = name.loc;
      call_actn->proc.name = name.name;
      call_actn->proc_id = UNRESOLVED_ID;

      FblcVectorInit(arena, call_actn->ports, call_actn->portc);
      if (!IsToken(toks, ';')) {
        FblcVectorExtend(arena, call_actn->ports, call_actn->portc);
        if (!GetNameToken(arena, toks, "port name", call_actn->ports + call_actn->portc++)) {
          return NULL;
        }

        while (IsToken(toks, ',')) {
          GetToken(toks, ',');
          FblcVectorExtend(arena, call_actn->ports, call_actn->portc);
          if (!GetNameToken(arena, toks, "port name", call_actn->ports + call_actn->portc++)) {
            return NULL;
          }
        }
      }
      call_actn->port_ids = arena->alloc(arena, call_actn->portc * sizeof(FblcPortId));
      for (size_t i = 0; i < call_actn->portc; ++i) {
        call_actn->port_ids[i] = UNRESOLVED_ID;
      }

      if (!GetToken(toks, ';')) {
        return NULL;
      }

      Expr** args = NULL;
      int exprc = ParseArgs(arena, toks, &args);
      if (exprc < 0) {
        return NULL;
      }
      call_actn->exprc = exprc;
      call_actn->exprs = args;
      actn = (Actn*)call_actn;
    } else if (in_stmt && IsToken(toks, '<')) {
      GetToken(toks, '<');
      if (!GetToken(toks, '~')) {
        return NULL;
      }
      if (!GetToken(toks, '>')) {
        return NULL;
      }
      LocName getname;
      if (!GetNameToken(arena, toks, "port name", &getname)) {
        return NULL;
      }
      if (!GetToken(toks, ',')) {
        return NULL;
      }
      LocName putname;
      if (!GetNameToken(arena, toks, "port name", &putname)) {
        return NULL;
      }
      if (!GetToken(toks, ';')) {
        return NULL;
      }
      Actn* body = ParseActn(arena, toks, true);
      if (body == NULL) {
        return NULL;
      }
      LinkActn* link_actn = arena->alloc(arena, sizeof(LinkActn));
      link_actn->tag = LINK_ACTN;
      link_actn->type = name;
      link_actn->getname = getname;
      link_actn->putname = putname;
      link_actn->body = body;
      link_actn->type_id = UNRESOLVED_ID;
      return (Actn*)link_actn;
    } else if (in_stmt && IsNameToken(toks)) {
      ExecActn* exec_actn = arena->alloc(arena, sizeof(ExecActn));
      exec_actn->tag = EXEC_ACTN;

      FblcVectorInit(arena, exec_actn->execv, exec_actn->execc);
      bool first = true;
      while (first || IsToken(toks, ',')) {
        FblcVectorExtend(arena, exec_actn->execv, exec_actn->execc);
        Exec* exec = exec_actn->execv + exec_actn->execc++;
        if (first) {
          exec->var.type.loc = name.loc;
          exec->var.type.name = name.name;
          first = false;
        } else {
          assert(IsToken(toks, ','));
          GetToken(toks, ',');

          if (!GetNameToken(arena, toks, "type name", &(exec->var.type))) {
            return NULL;
          }
        }

        if (!GetNameToken(arena, toks, "variable name", &(exec->var.name))) {
          return NULL;
        }

        if (!GetToken(toks, '=')) {
          return NULL;
        }

        exec->actn = ParseActn(arena, toks, false);
        if (exec->actn == NULL) {
          return NULL;
        }
      };

      if (!GetToken(toks, ';')) {
        return NULL;
      }
      exec_actn->body = ParseActn(arena, toks, true);
      if (exec_actn->body == NULL) {
        return NULL;
      }
      return (Actn*)exec_actn;
    } else {
      UnexpectedToken(toks, "The rest of a process starting with a name");
      return NULL;
    }
  } else if (IsToken(toks, '?')) {
    // ?(<expr> ; <proc>, ...)
    GetToken(toks, '?');
    if (!GetToken(toks, '(')) {
      return NULL;
    }
    Expr* condition = ParseExpr(arena, toks, false);
    if (condition == NULL) {
      return NULL;
    }

    if (!GetToken(toks, ';')) {
      return NULL;
    }

    CondActn* cond_actn = arena->alloc(arena, sizeof(CondActn));
    cond_actn->tag = COND_ACTN;
    cond_actn->select = condition;

    FblcVectorInit(arena, cond_actn->args, cond_actn->argc);
    bool first = true;
    while (first || IsToken(toks, ',')) {
      if (first) {
        first = false;
      } else {
        assert(IsToken(toks, ','));
        GetToken(toks, ',');
      }

      Actn* arg = ParseActn(arena, toks, false);
      if (arg == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, cond_actn->args, cond_actn->argc, arg);
    } while (IsToken(toks, ','));

    if (!GetToken(toks, ')')) {
      return NULL;
    }
    actn = (Actn*)cond_actn;
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
//   ids throughout the parsed program will be set to UNRESOLVED_ID in the
//   returned result.
//
// Side effects:
//   A program environment is allocated. In the case of an error, an error
//   message is printed to standard error; the caller is still responsible for
//   freeing (unused) allocations made with the allocator in this case.
Env* ParseProgram(FblcArena* arena, const char* filename)
{
  TokenStream toks;
  if (!OpenFileTokenStream(&toks, filename)) {
    fprintf(stderr, "failed to open %s.\n", filename);
    return NULL;
  }

  const char* keywords = "'struct', 'union', 'func', or 'proc'";
  Decl** declv;
  int declc;
  FblcVectorInit(arena, declv, declc);
  while (!IsEOFToken(&toks)) {
    // All declarations start with the form: <keyword> <name> (...
    LocName keyword;
    if (!GetNameToken(arena, &toks, keywords, &keyword)) {
      return NULL;
    }

    LocName name;
    if (!GetNameToken(arena, &toks, "declaration name", &name)) {
      return NULL;
    }

    if (!GetToken(&toks, '(')) {
      return NULL;
    }

    bool is_struct = NamesEqual("struct", keyword.name);
    bool is_union = NamesEqual("union", keyword.name);
    bool is_func = NamesEqual("func", keyword.name);
    bool is_proc = NamesEqual("proc", keyword.name);

    if (is_struct || is_union) {
      // Struct and union declarations end with: ... <fields>);
      TypeDecl* type = arena->alloc(arena, sizeof(TypeDecl));
      type->tag = is_struct ? STRUCT_DECL : UNION_DECL;
      type->name.name = name.name;
      type->name.loc = name.loc;
      type->fieldc = ParseFields(arena, &toks, &(type->fieldv));
      if (type->fieldc < 0) {
        return NULL;
      }

      if (!GetToken(&toks, ')')) {
        return NULL;
      }
      FblcVectorAppend(arena, declv, declc, (Decl*)type);
    } else if (is_func) {
      // Function declarations end with: ... <fields>; <type>) <expr>;
      FuncDecl* func = arena->alloc(arena, sizeof(FuncDecl));
      func->tag = FUNC_DECL;
      func->name.name = name.name;
      func->name.loc = name.loc;
      func->argc = ParseFields(arena, &toks, &(func->argv));
      func->return_type_id = UNRESOLVED_ID;
      if (func->argc < 0) {
        return NULL;
      }

      if (!GetToken(&toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(arena, &toks, "type", &(func->return_type))) {
        return NULL;
      }

      if (!GetToken(&toks, ')')) {
        return NULL;
      }

      func->body = ParseExpr(arena, &toks, false);
      if (func->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, declv, declc, (Decl*)func);
    } else if (is_proc) {
      // Proc declarations end with: ... <ports> ; <fields>; [<type>]) <proc>;
      ProcDecl* proc = arena->alloc(arena, sizeof(ProcDecl));
      proc->tag = PROC_DECL;
      proc->name.name = name.name;
      proc->name.loc = name.loc;
      proc->return_type_id = UNRESOLVED_ID;
      proc->portc = ParsePorts(arena, &toks, &(proc->portv));
      if (proc->portc < 0) {
        return NULL;
      }

      if (!GetToken(&toks, ';')) {
        return NULL;
      }

      proc->argc = ParseFields(arena, &toks, &(proc->argv));
      if (proc->argc < 0) {
        return NULL;
      }

      if (!GetToken(&toks, ';')) {
        return NULL;
      }

      if (!GetNameToken(arena, &toks, "type", &(proc->return_type))) {
        return NULL;
      }

      if (!GetToken(&toks, ')')) {
        return NULL;
      }

      proc->body = ParseActn(arena, &toks, false);
      if (proc->body == NULL) {
        return NULL;
      }
      FblcVectorAppend(arena, declv, declc, (Decl*)proc);
    } else {
      ReportError("Expected %s, but got '%s'.\n", keyword.loc, keywords, keyword.name);
      return NULL;
    }

    if (!GetToken(&toks, ';')) {
      return NULL;
    }
  }
  return NewEnv(arena, declc, declv);
}

// ParseValueFromToks --
//   Parse an Fblc value from the token stream.
//
// Inputs:
//   env - The program environment.
//   type - The type of value to parse.
//   toks - The token stream to parse the program from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The token stream is advanced to the end of the value. In the case of an
//   error, an error message is printed to standard error.
static FblcValue* ParseValueFromToks(FblcArena* arena, Env* env, FblcTypeId typeid, TokenStream* toks)
{
  TypeDecl* type = (TypeDecl*)env->declv[typeid];
  LocName name;
  if (!GetNameToken(arena, toks, "type name", &name)) {
    return NULL;
  }

  if (!NamesEqual(name.name, type->name.name)) {
    ReportError("Expected %s, but got %s.\n", name.loc, type->name, name);
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);
    return NULL;
  }
  arena->free(arena, (void*)name.name);
  arena->free(arena, (void*)name.loc);

  if (type->tag == STRUCT_DECL) {
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
        value->fields[i] = ParseValueFromToks(arena, env, type->fieldv[i].type_id, toks);
        err = err || value->fields[i] == NULL;
      }
    }

    if (err || !GetToken(toks, ')')) {
      FblcRelease(arena, value);
      return NULL;
    }
    return value;
  } else {
    assert(type->tag == UNION_DECL);
    if (!GetToken(toks, ':')) {
      return NULL;
    }

    if (!GetNameToken(arena, toks, "field name", &name)) {
      return NULL;
    }

    int tag = -1;
    for (size_t i = 0; i < type->fieldc; ++i) {
      if (NamesEqual(type->fieldv[i].name.name, name.name)) {
        tag = i;
        break;
      }
    }
    if (tag < 0) {
      ReportError("Invalid field %s for type %s.\n", name.loc, name.name, type->name);
      arena->free(arena, (void*)name.name);
      arena->free(arena, (void*)name.loc);
      return NULL;
    }
    arena->free(arena, (void*)name.name);
    arena->free(arena, (void*)name.loc);

    if (!GetToken(toks, '(')) {
      return NULL;
    }
    FblcValue* field = ParseValueFromToks(arena, env, type->fieldv[tag].type_id, toks);
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
//   env - The program environment.
//   typeid - The type id of value to parse.
//   fd - A file descriptor of a file open for reading.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   The value is read from the given file descriptor. In the case of an
//   error, an error message is printed to standard error.
FblcValue* ParseValue(FblcArena* arena, Env* env, FblcTypeId typeid, int fd)
{
  TokenStream toks;
  OpenFdTokenStream(&toks, fd, "file descriptor");
  return ParseValueFromToks(arena, env, typeid, &toks);
}

// ParseValueFromString --
//   Parse an fblc value from a string.
//
// Inputs:
//   env - The program environment.
//   typeid - The type id of value to parse.
//   string - The string to parse the value from.
//
// Result:
//   The parsed value, or NULL on error.
//
// Side effects:
//   In the case of an error, an error message is printed to standard error.
FblcValue* ParseValueFromString(FblcArena* arena, Env* env, FblcTypeId typeid, const char* string)
{
  TokenStream toks;
  OpenStringTokenStream(&toks, string, string);
  return ParseValueFromToks(arena, env, typeid, &toks);
}
