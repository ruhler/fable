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

// Initial ids are set to UNRESOLVED_ID. Relevant ids are resolved during the
// type checking phase.
#define UNRESOLVED_ID (-1)

typedef struct {
  FblcDeclTag tag;
} Decl;

typedef struct {
  FblcDeclTag tag;
  size_t argc;
  FblcTypeId* argv;
  FblcTypeId return_type_id;
  FblcExpr* body;
} FuncDecl;

typedef struct {
  FblcDeclTag tag;
  size_t portc;
  FblcPort* portv;
  size_t argc;
  FblcTypeId* argv;
  FblcTypeId return_type_id;
  FblcActn* body;
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
