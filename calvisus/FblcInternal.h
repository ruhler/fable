// FblcInternal.h --
//
//   This header file describes the internally-visible facilities of the Fblc
//   interpreter.

#ifndef FBLC_INTERNAL_H_
#define FBLC_INTERNAL_H_

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gc/gc.h>

// FblcProgram
typedef const char* FblcName;
bool FblcNamesEqual(FblcName a, FblcName b);

typedef enum {
  FBLC_VAR_EXPR,
  FBLC_APP_EXPR,
  FBLC_ACCESS_EXPR,
  FBLC_UNION_EXPR,
  FBLC_LET_EXPR,
  FBLC_COND_EXPR,
} FblcExprTag;

typedef struct FblcExpr {
  FblcExprTag tag;

  union {
    // For variable expressions of the form: <name>
    struct {
      FblcName name;
    } var;

    // For application expressions of the form: <function>(<args>)
    // The function args are in the 'args' field of FblcExpr.
    struct {
      FblcName function;
    } app;

    // For member access expressions of the form: <object>.<field>
    struct {
      const struct FblcExpr* object;
      FblcName field;
    } access; 

    // For union literals of the form: <type>:<field>(<value>)
    // 'union' is a keyword in C, so we use 'union_' instead.
    struct { 
      FblcName type;
      FblcName field;
      const struct FblcExpr* value;
    } union_;

    // For let expressions of the form: <type> <name> = <def> ; <body>
    struct {
      FblcName type;
      FblcName name;
      const struct FblcExpr* def;
      const struct FblcExpr* body;
    } let;

    // For conditional expressions of the form: <select>?(<args>)
    // The expressions to choose among are in the 'args' field of FblcExpr.
    struct {
      const struct FblcExpr* select;
    } cond;
  } ex;

  // Additional variable-length arguments for app and cond expressions.
  // The number of arguments is implicit, based on the function for an
  // app expression and based on the type of the select object for a
  // cond expression.
  struct FblcExpr* args[];
} FblcExpr;

// FblcTokenizer
#define FBLC_TOK_EOF -1
#define FBLC_TOK_NAME -2
#define FBLC_TOK_ERR -3
typedef int FblcTokenType;
typedef struct FblcTokenStream FblcTokenStream;
FblcTokenStream* FblcOpenTokenStream(const char* filename);
void FblcCloseTokenStream(FblcTokenStream* toks);
bool FblcIsToken(FblcTokenStream* toks, FblcTokenType which);
const char* FblcGetNameToken(FblcTokenStream* toks, const char* expected);
bool FblcGetToken(FblcTokenStream* toks, FblcTokenType which);
void FblcUnexpectedToken(FblcTokenStream* toks, const char* expected);

#endif  // FBLC_INTERNAL_H_
