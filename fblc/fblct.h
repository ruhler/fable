// fblct.h --
//   This header file describes the externally visible interface to the
//   text-level fblc facilities.

#ifndef FBLCT_H_
#define FBLCT_H_

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
} VarExpr;

// APP_EXPR: Application expressions of the form: <func>(<argv>)
typedef struct {
  FblcAppExpr x;
} AppExpr;

// ACCESS_EXPR: Member access expressions of the form: <object>.<field>
typedef struct {
  FblcAccessExpr x;
} AccessExpr;

// UNION_EXPR: Union literals of the form: <type>:<field>(<value>)
typedef struct {
  FblcUnionExpr x;
} UnionExpr;

// LET_EXPR: let expressions of the form: <type> <name> = <def> ; <body>
typedef struct {
  FblcLetExpr x;
} LetExpr;

// COND_EXPR: Conditional expressions of the form: <select>?(<argv>)
typedef struct {
  FblcCondExpr x;
} CondExpr;

typedef struct {
  FblcDeclTag tag;
} Decl;

typedef struct {
  FblcDeclTag tag;
  size_t argc;
  FblcTypeId* argv;
  FblcTypeId return_type_id;
  Expr* body;
} FuncDecl;

// Actn is the base structure for all  actions. Each specialization of
// Actn will have the same initial layout with tag and location.

typedef struct {
  FblcActnTag tag;
} Actn;

// EVAL_ACTN: Processes of the form: $(<expr>)
typedef struct {
  FblcEvalActn x;
} EvalActn;

// GET_ACTN: Processes of the form: ~<pname>()
typedef struct {
  FblcGetActn x;
} GetActn;

// PUT_ACTN: Processes of the form: ~<pname>(<expr>)
typedef struct {
  FblcPutActn x;
} PutActn;

// CALL_ACTN: Processes of the form: <tname>(<port>, ... ; <expr>, ...)
typedef struct {
  FblcCallActn x;
} CallActn;

// LINK_ACTN: Processes of the form:
//    <tname> '<~>' <pname> ',' <pname> ';' <actn>
typedef struct {
  FblcLinkActn x;
} LinkActn;

// EXEC_ACTN: Processes of the form:
//    <tname> <vname> = <actn>,  ...  ; <body>
typedef struct {
  FblcExecActn x;
} ExecActn;

// COND_ACTN: Processes of the form: <expr>?(<proc>, ...)
typedef struct {
  FblcCondActn x;
} CondActn;

typedef struct {
  FblcDeclTag tag;
  size_t portc;
  FblcPort* portv;
  size_t argc;
  FblcTypeId* argv;
  FblcTypeId return_type_id;
  Actn* body;
} ProcDecl;

typedef struct {
  LocName type;
  LocName name;
} SVar;

typedef struct {
  LocName name;
} SDecl;

typedef struct {
  LocName name;
  SVar* fields;
} STypeDecl;

typedef struct {
  LocName name;
  size_t locc;
  Loc* locv;  // locations of all expressions in body.
  size_t svarc;
  SVar* svarv; // types and names of all local variables in order they appear.
} SFuncDecl;

typedef struct {
  LocName name;
  size_t locc;
  Loc* locv;  // locations of all actions and expressions in body.
  size_t svarc;
  SVar* svarv; // types and names of all local variables in order they appear.
  size_t sportc;  // types and names of all ports in order they appear.
  SVar* sportv;
} SProcDecl;

// An environment contains all the type, function, and process declarations
// for a program.
typedef struct {
  size_t declc;
  Decl** declv;
  SDecl** sdeclv;
} Env;

// Parser
Env* ParseProgram(FblcArena* arena, const char* filename);
FblcValue* ParseValue(FblcArena* arena, Env* env, FblcTypeId typeid, int fd);
FblcValue* ParseValueFromString(FblcArena* arena, Env* env, FblcTypeId typeid, const char* string);

// ResolveProgram --
//   Perform id and name resolution for references to variables, ports,
//   declarations, and fields in the given program.
//
// Inputs:
//   env - A program whose ids need to be resolved. Each ID in the program
//         should be an index into the names array that gives the name and
//         location corresponding to the id.
//   names - Array of names for use in resolution.
//
// Results:
//   true if name resolution succeeded, false otherwise.
//
// Side effects:
//   IDs in the program are resolved.
bool ResolveProgram(Env* env, LocName* names);

// Checker
bool CheckProgram(Env* env);

// Strip
FblcProgram* StripProgram(FblcArena* arena, Env* tprog);

#endif  // FBLCT_H_
