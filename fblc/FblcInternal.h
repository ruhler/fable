// FblcInternal.h --
//
//   This header file describes the internally-visible facilities of the Fblc
//   interpreter.

#ifndef FBLC_INTERNAL_H_
#define FBLC_INTERNAL_H_

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gc/gc.h>

// FblcProgram
typedef const char* FblcName;
bool FblcNamesEqual(FblcName a, FblcName b);

typedef struct FblcLoc FblcLoc;
FblcLoc* FblcNewLoc(const char* source, int line, int col);
void FblcReportError(const char* format, const FblcLoc* loc, ...);

typedef struct FblcLocName {
  FblcLoc* loc;
  FblcName name;
} FblcLocName;

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
  FblcLoc* loc;

  union {
    // For variable expressions of the form: <name>
    struct {
      FblcLocName name;
    } var;

    // For application expressions of the form: <func>(<argv>)
    // The function args are in the 'argv' field of FblcExpr.
    struct {
      FblcLocName func;
    } app;

    // For member access expressions of the form: <object>.<field>
    struct {
      const struct FblcExpr* object;
      FblcLocName field;
    } access; 

    // For union literals of the form: <type>:<field>(<value>)
    // 'union' is a keyword in C, so we use 'union_' instead.
    struct { 
      FblcLocName type;
      FblcLocName field;
      const struct FblcExpr* value;
    } union_;

    // For let expressions of the form: <type> <name> = <def> ; <body>
    struct {
      FblcLocName type;
      FblcLocName name;
      const struct FblcExpr* def;
      const struct FblcExpr* body;
    } let;

    // For conditional expressions of the form: <select>?(<argv>)
    // The expressions to choose among are in the 'argv' field of FblcExpr.
    struct {
      const struct FblcExpr* select;
    } cond;
  } ex;

  // Additional variable-length arguments for app and cond expressions.
  // The number of arguments is given by argc, and the arguments themselves
  // are in argv. The values of argc and argv are undefined unless the
  // expression is an application or conditional expression.
  int argc;
  struct FblcExpr* argv[];
} FblcExpr;

typedef enum { FBLC_KIND_UNION, FBLC_KIND_STRUCT } FblcKind;

typedef struct {
  FblcLocName type;
  FblcLocName name;
} FblcField;

typedef struct {
  FblcLocName name;
  FblcKind kind;
  int fieldc;
  FblcField fieldv[];
} FblcType;

typedef struct {
  FblcLocName name;
  FblcLocName return_type;
  FblcExpr* body;
  int argc;
  FblcField argv[];
} FblcFunc;

typedef enum { FBLC_POLARITY_PUT, FBLC_POLARITY_GET } FblcPolarity;

typedef struct {
  FblcLocName type;
  FblcLocName name;
  FblcPolarity polarity;
} FblcPort;

typedef enum {
  FBLC_EVAL_PROC,
} FblcProcTag;

typedef struct FblcProcExpr {
  FblcProcTag tag;
  FblcLoc* loc;
  union {
    // For processes of the form: $(<expr>)
    FblcExpr* expr;
  } eval;
} FblcProcExpr;

typedef struct {
  FblcLocName name;
  FblcLocName* return_type;     // NULL if no return type.
  FblcProcExpr* body;
  int portc;
  FblcPort* portv;
  int argc;
  FblcField* argv;
} FblcProc;

// An environment contains all the type, function, and process declarations
// for a program. All names used for types, functions, and processes must be
// unique. This is enforced during the construction of the environment.
typedef struct FblcTypeEnv {
  FblcType* decl;
  struct FblcTypeEnv* next;
} FblcTypeEnv;

typedef struct FblcFuncEnv {
  FblcFunc* decl;
  struct FblcFuncEnv* next;
} FblcFuncEnv;

typedef struct FblcProcEnv {
  FblcProc* decl;
  struct FblcProcEnv* next;
} FblcProcEnv;

typedef struct FblcEnv {
  FblcTypeEnv* types;
  FblcFuncEnv* funcs;
  FblcProcEnv* procs;
} FblcEnv;

FblcEnv* FblcNewEnv();
FblcType* FblcLookupType(const FblcEnv* env, FblcName name);
FblcFunc* FblcLookupFunc(const FblcEnv* env, FblcName name);
FblcProc* FblcLookupProc(const FblcEnv* env, FblcName name);
bool FblcAddType(FblcEnv* env, FblcType* type);
bool FblcAddFunc(FblcEnv* env, FblcFunc* func);
bool FblcAddProc(FblcEnv* env, FblcProc* proc);

// FblcTokenizer
#define FBLC_TOK_EOF -1
#define FBLC_TOK_NAME -2
#define FBLC_TOK_ERR -3
typedef int FblcTokenType;
typedef struct FblcTokenStream FblcTokenStream;
FblcTokenStream* FblcOpenTokenStream(const char* filename);
void FblcCloseTokenStream(FblcTokenStream* toks);
bool FblcIsToken(FblcTokenStream* toks, FblcTokenType which);
bool FblcGetNameToken(
    FblcTokenStream* toks, const char* expected, FblcLocName* name);
bool FblcGetToken(FblcTokenStream* toks, FblcTokenType which);
void FblcUnexpectedToken(FblcTokenStream* toks, const char* expected);

// FblcParser
FblcEnv* FblcParseProgram(FblcTokenStream* toks);

// FblcChecker
bool FblcCheckProgram(const FblcEnv* env);

// FblcEvaluator
typedef struct FblcValue FblcValue;
void FblcPrintValue(FILE* fout, FblcValue* value);
FblcValue* FblcEvaluate(const FblcEnv* env, const FblcExpr* expr);

#endif  // FBLC_INTERNAL_H_
