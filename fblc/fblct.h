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
// Initial id will be UNRESOLVED_ID. Relevant ids are resolved during the type
// checking phase.
#define UNRESOLVED_ID (-1)
typedef struct LocName {
  Loc* loc;
  Name name;
  size_t id;
} LocName;

typedef enum {
  VAR_EXPR,
  APP_EXPR,
  UNION_EXPR,
  ACCESS_EXPR,
  COND_EXPR,
  LET_EXPR,
} ExprTag;

// Expr is the base structure for all  expressions. Each
// specialization of Expr has the same initial layout with tag and
// location.

typedef struct {
  ExprTag tag;
  Loc* loc;
} Expr;

// VAR_EXPR: Variable expressions of the form: <name>
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName name;
} VarExpr;

// APP_EXPR: Application expressions of the form: <func>(<argv>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName func;
  int argc;
  Expr** argv;
} AppExpr;

// ACCESS_EXPR: Member access expressions of the form: <object>.<field>
typedef struct {
  ExprTag tag;
  Loc* loc;
  Expr* object;
  LocName field;
} AccessExpr;

// UNION_EXPR: Union literals of the form: <type>:<field>(<value>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName type;
  LocName field;
  Expr* value;
} UnionExpr;

// LET_EXPR: let expressions of the form: <type> <name> = <def> ; <body>
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName type;
  LocName name;
  Expr* def;
  Expr* body;
} LetExpr;

// COND_EXPR: Conditional expressions of the form: <select>?(<argv>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  Expr* select;
  int argc;
  Expr** argv;
} CondExpr;

typedef struct {
  LocName type;
  LocName name;
} Field;

typedef enum { STRUCT_DECL, UNION_DECL, FUNC_DECL, PROC_DECL } DeclTag;

typedef struct {
  DeclTag tag;
  LocName name;
} Decl;

typedef struct {
  DeclTag tag;
  LocName name;
  int fieldc;
  Field* fieldv;
} TypeDecl;

typedef struct {
  DeclTag tag;
  LocName name;
  LocName return_type;
  Expr* body;
  int argc;
  Field* argv;
} FuncDecl;

typedef enum {
  POLARITY_GET,
  POLARITY_PUT
} Polarity;

typedef struct {
  LocName type;
  LocName name;
  Polarity polarity;
} Port;

typedef enum {
  EVAL_ACTN,
  GET_ACTN,
  PUT_ACTN,
  COND_ACTN,
  CALL_ACTN,
  LINK_ACTN,
  EXEC_ACTN,
} ActnTag;

// Actn is the base structure for all  actions. Each specialization of
// Actn will have the same initial layout with tag and location.

typedef struct {
  ActnTag tag;
  Loc* loc;
} Actn;

// EVAL_ACTN: Processes of the form: $(<expr>)
typedef struct {
  ActnTag tag;
  Loc* loc;
  Expr* expr;
} EvalActn;

// GET_ACTN: Processes of the form: <pname>~()
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName port;
} GetActn;

// PUT_ACTN: Processes of the form: <pname>~(<expr>)
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName port;
  Expr* expr;
} PutActn;

// CALL_ACTN: Processes of the form: <tname>(<port>, ... ; <expr>, ...)
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName proc;
  int portc;
  LocName* ports;   // Array of portc ports
  int exprc;
  Expr** exprs;      // Array of exprc exprs
} CallActn;

// LINK_ACTN: Processes of the form:
//    <tname> '<~>' <pname> ',' <pname> ';' <actn>
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName type;
  LocName getname;
  LocName putname;
  Actn* body;
} LinkActn;

typedef struct {
  Field var;
  Actn* actn;
} Exec;

// EXEC_ACTN: Processes of the form:
//    <tname> <vname> = <actn>,  ...  ; <body>
typedef struct {
  ActnTag tag;
  Loc* loc;
  int execc;
  Exec* execv;          // Array of execc execs.
  Actn* body;
} ExecActn;

// COND_ACTN: Processes of the form: <expr>?(<proc>, ...)
typedef struct {
  ActnTag tag;
  Loc* loc;
  Expr* select;
  int argc;
  Actn** args;    //  Array of argc args.
} CondActn;

typedef struct {
  DeclTag tag;
  LocName name;
  LocName return_type;
  Actn* body;
  int portc;
  Port* portv;              // Array of portc ports.
  int argc;
  Field* argv;              // Array of argv fields.
} ProcDecl;

// An environment contains all the type, function, and process declarations
// for a program.
typedef struct {
  int declc;
  Decl** declv;
} Env;

Env* NewEnv(FblcArena* arena, int declc, Decl** declv);

// Parser
Env* ParseProgram(FblcArena* arena, const char* filename);
FblcValue* ParseValue(FblcArena* arena, Env* env, FblcTypeId typeid, int fd);
FblcValue* ParseValueFromString(FblcArena* arena, Env* env, FblcTypeId typeid, const char* string);

// Checker
bool CheckProgram(Env* env);

// Strip
FblcProgram* StripProgram(FblcArena* arena, Env* tprog);

#endif  // FBLCT_H_
