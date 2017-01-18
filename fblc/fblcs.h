// fblcs.h --
//   This header file describes the externally visible interface to the
//   text-level fblc facilities.

#ifndef FBLCS_H_
#define FBLCS_H_

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

// NULL_ID is used a sentinel value to indicate an otherwise invalid id.
// Note that NULL_ID does not have the same value as NULL.
#define NULL_ID (-1)

typedef struct Symbols Symbols;

// SProgram --
//   An FblcProgram augmented with symbols information.
typedef struct {
  FblcProgram* program;
  Symbols* symbols;
} SProgram;

// Parser
SProgram* ParseProgram(FblcArena* arena, const char* filename);
FblcValue* ParseValue(FblcArena* arena, SProgram* sprog, FblcTypeId typeid, int fd);
FblcValue* ParseValueFromString(FblcArena* arena, SProgram* sprog, FblcTypeId typeid, const char* string);

// ResolveProgram --
//   Perform id/name resolution for references to variables, ports,
//   declarations, and fields in the given program.
//
// Inputs:
//   sprog - A program whose ids need to be resolved.
//
// Results:
//   true if name resolution succeeded, false otherwise.
//
// Side effects:
//   IDs in the program are resolved.
//   Prints error messages to stderr in case of error.
bool ResolveProgram(SProgram* sprog);

// Checker
bool CheckProgram(SProgram* sprog);

// SLoadProgram --
//   Load a text fblc program from the given file using the given arena for
//   allocations. Performs parsing, name resolution, and type checking of the
//   program.
//
// Inputs:
//   arena - Arena to use for allocations.
//   filename - Name of the file to load the program from.
//
// Results:
//   The fully parsed, name-resolved and type-checked loaded program, or NULL
//   if the program could not be parsed, resolved, or failed to type check.
//
// Side effects:
//   Arena allocations are made in order to load the program. If the program
//   cannot be loaded, an error message is printed to stderr.
SProgram* SLoadProgram(FblcArena* arena, const char* filename);

// Symbols
Symbols* NewSymbols(FblcArena* arena);
void SetLocExpr(FblcArena* arena, Symbols* symbols, FblcLocId loc_id, Loc* loc);
void SetLocActn(FblcArena* arena, Symbols* symbols, FblcLocId loc_id, Loc* loc);
void SetLocId(FblcArena* arena, Symbols* symbols, FblcLocId loc_id, SName* name);
void SetLocTypedId(FblcArena* arena, Symbols* symbols, FblcLocId loc_id, SName* type, SName* name);
void SetLocLink(FblcArena* arena, Symbols* symbols, FblcLocId loc_id, SName* type, SName* get, SName* put);
void SetLocDecl(FblcArena* arena, Symbols* symbols, FblcLocId loc_id, SName* name, FblcDeclId decl_id);

Loc* LocIdLoc(Symbols* symbols, FblcLocId loc_id);
SName* LocIdName(Symbols* symbols, FblcLocId loc_id);
SName* LocIdType(Symbols* symbols, FblcLocId loc_id);
SName* LocIdLinkGet(Symbols* symbols, FblcLocId loc_id);
SName* LocIdLinkPut(Symbols* symbols, FblcLocId loc_id);

Name DeclName(SProgram* sprog, FblcDeclId decl_id);
Name FieldName(SProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id);
FblcLocId DeclLocId(SProgram* sprog, FblcDeclId decl_id);
FblcDeclId SLookupDecl(SProgram* sprog, Name name);
FblcFieldId SLookupField(SProgram* sprog, FblcDeclId decl_id, Name field);
#endif  // FBLCS_H_
