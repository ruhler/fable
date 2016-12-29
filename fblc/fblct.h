// fblct.h --
//   This header file describes the externally visible interface to the
//   text-level fblc facilities.

#ifndef FBLCT_H_
#define FBLCT_H_

#include <stdint.h>   // for uint32_t
#include <stdio.h>    // for BUFSIZ

#include "fblc.h"

// Program
typedef const char* Name;
bool NamesEqual(Name a, Name b);

typedef struct {
  const char* source;
  int line;
  int col;
} Loc;

void ReportError(const char* format, Loc* loc, ...);

// LocName stores a name along with a location for error reporting purposes.
// The id field contains the id of name as used in the binary encoded program.
typedef struct LocName {
  Loc* loc;
  Name name;
} LocName;

// Expr is the base structure for all  expressions. Each
// specialization of Expr has the same initial layout with tag and
// location.

// Initial ids are set to UNRESOLVED_ID. Relevant ids are resolved during the
// type checking phase.
#define UNRESOLVED_ID (-1)

typedef struct {
  FblcExprTag tag;
} Expr;

// VAR_EXPR: Variable expressions of the form: <name>
typedef struct {
  FblcVarExpr x;
  LocName name;
} VarExpr;

// APP_EXPR: Application expressions of the form: <func>(<argv>)
typedef struct {
  FblcAppExpr x;
  LocName func;
} AppExpr;

// ACCESS_EXPR: Member access expressions of the form: <object>.<field>
typedef struct {
  FblcAccessExpr x;
  LocName field;
} AccessExpr;

// UNION_EXPR: Union literals of the form: <type>:<field>(<value>)
typedef struct {
  FblcUnionExpr x;
  LocName type;
  LocName field;
} UnionExpr;

// LET_EXPR: let expressions of the form: <type> <name> = <def> ; <body>
typedef struct {
  FblcLetExpr x;
  LocName type;
  LocName name;
} LetExpr;

// COND_EXPR: Conditional expressions of the form: <select>?(<argv>)
typedef struct {
  FblcCondExpr x;
} CondExpr;

typedef struct {
  LocName type;
  LocName name;
} Field;

typedef struct {
  FblcDeclTag tag;
} Decl;

typedef struct {
  FblcDeclTag tag;
  int fieldc;
  LocName name;
  FblcTypeId* fieldv;
  Field* fields;
} TypeDecl;

typedef struct {
  FblcDeclTag tag;
  LocName name;
  LocName return_type;
  Expr* body;
  int argc;
  FblcTypeId* argv;
  Field* args;
  FblcTypeId return_type_id;
} FuncDecl;

typedef struct {
  LocName type;
  LocName name;
  FblcPolarity polarity;
  FblcTypeId type_id;
} Port;

// Actn is the base structure for all  actions. Each specialization of
// Actn will have the same initial layout with tag and location.

typedef struct {
  FblcActnTag tag;
} Actn;

// EVAL_ACTN: Processes of the form: $(<expr>)
typedef struct {
  FblcEvalActn x;
} EvalActn;

// GET_ACTN: Processes of the form: <pname>~()
typedef struct {
  FblcGetActn x;
  LocName port;
} GetActn;

// PUT_ACTN: Processes of the form: <pname>~(<expr>)
typedef struct {
  FblcPutActn x;
  LocName port;
} PutActn;

// CALL_ACTN: Processes of the form: <tname>(<port>, ... ; <expr>, ...)
typedef struct {
  FblcCallActn x;
  LocName proc;
  LocName* ports;   // Array of x.portc ports
} CallActn;

// LINK_ACTN: Processes of the form:
//    <tname> '<~>' <pname> ',' <pname> ';' <actn>
typedef struct {
  FblcLinkActn x;
  LocName type;
  LocName getname;
  LocName putname;
} LinkActn;

// EXEC_ACTN: Processes of the form:
//    <tname> <vname> = <actn>,  ...  ; <body>
typedef struct {
  FblcExecActn x;
  Field* vars;           // Array of x.execc fields.
} ExecActn;

// COND_ACTN: Processes of the form: <expr>?(<proc>, ...)
typedef struct {
  FblcCondActn x;
} CondActn;

typedef struct {
  FblcDeclTag tag;
  LocName name;
  LocName return_type;
  Actn* body;
  int portc;
  Port* portv;
  int argc;
  FblcTypeId* argv;
  Field* args;
  FblcTypeId return_type_id;
} ProcDecl;

// An environment contains all the type, function, and process declarations
// for a program.
typedef struct {
  int declc;
  Decl** declv;
} Env;

Env* NewEnv(FblcArena* arena, int declc, Decl** declv);
LocName* DeclName(Decl* decl);

// Parser
Env* ParseProgram(FblcArena* arena, const char* filename);
FblcValue* ParseValue(FblcArena* arena, Env* env, FblcTypeId typeid, int fd);
FblcValue* ParseValueFromString(FblcArena* arena, Env* env, FblcTypeId typeid, const char* string);

// Checker
bool CheckProgram(Env* env);

// Strip
FblcProgram* StripProgram(FblcArena* arena, Env* tprog);

#endif  // FBLCT_H_
