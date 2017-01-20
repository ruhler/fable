// fblcs.c --
//   This file implements routines for manipulating fblc symbol information,
//   which maps source level names and locations to machine level program
//   constructs.

#include <assert.h>     // For assert
#include <stdarg.h>     // For va_start
#include <stdio.h>      // For fprintf

#include "fblcs.h"


// FblcsNamesEqual -- see documentation in fblcs.h
bool FblcsNamesEqual(FblcsName a, FblcsName b)
{
  return strcmp(a, b) == 0;
}

// FblcsReportError -- see documentation in fblcs.h
void FblcsReportError(const char* format, FblcsLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// SymbolTag -- 
//   Enum used to distinguish among different kinds of symbols.
typedef enum {
  LOC_SYMBOL,
  ID_SYMBOL,
  TYPED_ID_SYMBOL,
  LINK_SYMBOL,
  DECL_SYMBOL
} SymbolTag;

// Symbol --
//   Common base structure for all kinds of symbols.
typedef struct {
  SymbolTag tag;
} Symbol;

// LocSymbol --
//   A symbol that stores information about a location only.
typedef struct {
  SymbolTag tag;
  FblcsLoc loc;
} LocSymbol;

// IdSymbol --
//   A symbol that stores information about a location and a name.
typedef struct {
  SymbolTag tag;
  FblcsNameL name;
} IdSymbol;

// TypedIdSymbol --
//   A symbol that stores location and name information for an id and it's
//   type.
typedef struct {
  SymbolTag tag;
  FblcsNameL name;
  FblcsNameL type;
} TypedIdSymbol;

// LinkSymbol --
//   A symbol that stores location and name information for a link actions
//   type, get port name, and put port name.
typedef struct {
  SymbolTag tag;
  FblcsNameL type;
  FblcsNameL get;
  FblcsNameL put;
} LinkSymbol;

// DeclSymbol --
//   A symbol that stores information for a declaration.
typedef struct {
  SymbolTag tag;
  FblcsNameL name;
  FblcDeclId decl_id;
} DeclSymbol;

// FblcsSymbols --
//   Information associated with each location id in a program.
//
// symbolc/symbolv - A vector of symbol information indexed by FblcLocId.
// declc/declv - A vector mapping FblcDeclId to corresponding FblcLocId.
struct FblcsSymbols {
  size_t symbolc;
  Symbol** symbolv;

  size_t declc;
  FblcLocId* declv;
};

FblcsSymbols* NewSymbols(FblcArena* arena)
{
  FblcsSymbols* symbols = arena->alloc(arena, sizeof(FblcsSymbols));
  FblcVectorInit(arena, symbols->symbolv, symbols->symbolc);
  FblcVectorInit(arena, symbols->declv, symbols->declc);
  return symbols;
}

static void SetLocSymbol(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, Symbol* symbol)
{
  while (loc_id >= symbols->symbolc) {
    FblcVectorAppend(arena, symbols->symbolv, symbols->symbolc, NULL);
  }
  assert(loc_id < symbols->symbolc);
  assert(symbols->symbolv[loc_id] == NULL && "TODO: Support out of order SetLoc?");
  symbols->symbolv[loc_id] = symbol;
}

static void SetLocLoc(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsLoc* loc)
{
  LocSymbol* symbol = arena->alloc(arena, sizeof(LocSymbol));
  symbol->tag = LOC_SYMBOL;
  symbol->loc.source = loc->source;
  symbol->loc.line = loc->line;
  symbol->loc.col = loc->col;
  SetLocSymbol(arena, symbols, loc_id, (Symbol*)symbol);
}

void SetLocExpr(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsLoc* loc)
{
  SetLocLoc(arena, symbols, loc_id, loc);
}

void SetLocActn(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsLoc* loc)
{
  SetLocLoc(arena, symbols, loc_id, loc);
}

void SetLocId(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* name)
{
  IdSymbol* symbol = arena->alloc(arena, sizeof(IdSymbol));
  symbol->tag = ID_SYMBOL;
  symbol->name.name = name->name;
  symbol->name.loc = name->loc;
  SetLocSymbol(arena, symbols, loc_id, (Symbol*)symbol);
}

void SetLocTypedId(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* type, FblcsNameL* name)
{
  TypedIdSymbol* symbol = arena->alloc(arena, sizeof(TypedIdSymbol));
  symbol->tag = TYPED_ID_SYMBOL;
  symbol->type.name = type->name;
  symbol->type.loc = type->loc;
  symbol->name.name = name->name;
  symbol->name.loc = name->loc;
  SetLocSymbol(arena, symbols, loc_id, (Symbol*)symbol);
}

void SetLocLink(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* type, FblcsNameL* get, FblcsNameL* put)
{
  LinkSymbol* symbol = arena->alloc(arena, sizeof(LinkSymbol));
  symbol->tag = LINK_SYMBOL;
  symbol->type.name = type->name;
  symbol->type.loc = type->loc;
  symbol->get.name = get->name;
  symbol->get.loc = get->loc;
  symbol->put.name = put->name;
  symbol->put.loc = put->loc;
  SetLocSymbol(arena, symbols, loc_id, (Symbol*)symbol);
}

void SetLocDecl(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* name, FblcDeclId decl_id)
{
  DeclSymbol* symbol = arena->alloc(arena, sizeof(DeclSymbol));
  symbol->tag = DECL_SYMBOL;
  symbol->name.name = name->name;
  symbol->name.loc = name->loc;
  symbol->decl_id = decl_id;
  SetLocSymbol(arena, symbols, loc_id, (Symbol*)symbol);

  while (decl_id >= symbols->declc) {
    FblcVectorAppend(arena, symbols->declv, symbols->declc, FBLC_NULL_ID);
  }
  assert(decl_id < symbols->declc);
  symbols->declv[decl_id] = loc_id;
}

FblcsLoc* LocIdLoc(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  Symbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == LOC_SYMBOL) {
    LocSymbol* loc_symbol = (LocSymbol*)symbol;
    return &loc_symbol->loc;
  }
  return LocIdName(symbols, loc_id)->loc;
}

FblcsNameL* LocIdName(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  Symbol* symbol = symbols->symbolv[loc_id];
  switch (symbol->tag) {
    case LOC_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case ID_SYMBOL: {
      IdSymbol* id_symbol = (IdSymbol*)symbol;
      return &id_symbol->name;
    }

    case TYPED_ID_SYMBOL: {
      TypedIdSymbol* typed_id_symbol = (TypedIdSymbol*)symbol;
      return &typed_id_symbol->name;
    }

    case LINK_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case DECL_SYMBOL: {
      DeclSymbol* decl_symbol = (DeclSymbol*)symbol;
      return &decl_symbol->name;
    }
  }
  assert(false && "Invalid tag");
  return NULL;
}

FblcsNameL* LocIdType(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  Symbol* symbol = symbols->symbolv[loc_id];
  switch (symbol->tag) {
    case LOC_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case ID_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case TYPED_ID_SYMBOL: {
      TypedIdSymbol* typed_id_symbol = (TypedIdSymbol*)symbol;
      return &typed_id_symbol->type;
    }

    case LINK_SYMBOL: {
      LinkSymbol* link_symbol = (LinkSymbol*)symbol;
      return &link_symbol->type;
    }

    case DECL_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }
  }
  assert(false && "TODO");
  return NULL;
}

FblcsNameL* LocIdLinkGet(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  Symbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == LINK_SYMBOL) {
    LinkSymbol* link_symbol = (LinkSymbol*)symbol;
    return &link_symbol->get;
  }
  assert(false && "unsupported tag");
  return NULL;
}

FblcsNameL* LocIdLinkPut(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  Symbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == LINK_SYMBOL) {
    LinkSymbol* link_symbol = (LinkSymbol*)symbol;
    return &link_symbol->put;
  }
  assert(false && "unsupported tag");
  return NULL;
}

// DeclName -- See documentation in fblcs.h
FblcsName DeclName(FblcsProgram* sprog, FblcDeclId decl_id)
{
  return LocIdName(sprog->symbols, DeclLocId(sprog, decl_id))->name;
}

// FieldName -- See documentation in fblcs.h
FblcsName FieldName(FblcsProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id)
{
  return LocIdName(sprog->symbols, DeclLocId(sprog, decl_id) + field_id + 1)->name;
}

FblcLocId DeclLocId(FblcsProgram* sprog, FblcDeclId decl_id)
{
  return sprog->symbols->declv[decl_id];
}

// LookupDecl -- See documentation in fblcs.h
FblcDeclId FblcsLookupDecl(FblcsProgram* sprog, FblcsName name)
{
  for (FblcDeclId i = 0; i < sprog->program->declc; ++i) {
    if (FblcsNamesEqual(DeclName(sprog, i), name)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

FblcFieldId SLookupField(FblcsProgram* sprog, FblcDeclId decl_id, FblcsName field)
{
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[decl_id];
  assert(type->tag == FBLC_STRUCT_DECL || type->tag == FBLC_UNION_DECL);
  for (FblcFieldId i = 0; i < type->fieldc; ++i) {
    if (FblcsNamesEqual(FieldName(sprog, decl_id, i), field)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}
