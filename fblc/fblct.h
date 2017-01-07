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

// SName stores a name along with a location for error reporting purposes.
typedef struct SName {
  Loc* loc;
  Name name;
} SName;

// Initial ids are set to UNRESOLVED_ID. Relevant ids are resolved during the
// type checking phase.
#define UNRESOLVED_ID (-1)

typedef struct {
  SName type;
  SName name;
} SVar;

typedef struct {
  SName name;
} SDecl;

typedef struct {
  SName name;
  SVar* fields;
} STypeDecl;

typedef struct {
  SName name;
  size_t locc;
  Loc* locv;  // locations of all expressions in body.
  size_t svarc;
  SVar* svarv; // types and names of all local variables in order they appear.
} SFuncDecl;

typedef struct {
  SName name;
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
  FblcDecl** declv;
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
bool ResolveProgram(Env* env, SName* names);

// Checker
bool CheckProgram(Env* env);

// Strip
FblcProgram* StripProgram(FblcArena* arena, Env* tprog);

#endif  // FBLCT_H_
