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

// FblcsSymbolTag -- 
//   Enum used to distinguish among different kinds of symbols.
typedef enum {
  FBLCS_LOC_SYMBOL,
  FBLCS_ID_SYMBOL,
  FBLCS_TYPED_ID_SYMBOL,
  FBLCS_LINK_SYMBOL,
  FBLCS_DECL_SYMBOL
} FblcsSymbolTag;

// FblcsSymbol --
//   Common base structure for all kinds of symbols.
typedef struct {
  FblcsSymbolTag tag;
} FblcsSymbol;

// FblcsLocSymbol --
//   A symbol that stores information about a location only.
typedef struct {
  FblcsSymbolTag tag;
  FblcsLoc loc;
} FblcsLocSymbol;

// FblcsIdSymbol --
//   A symbol that stores information about a location and a name.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL name;
} FblcsIdSymbol;

// FblcsTypedIdSymbol --
//   A symbol that stores location and name information for an id and it's
//   type.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL name;
  FblcsNameL type;
} FblcsTypedIdSymbol;

// FblcsLinkSymbol --
//   A symbol that stores location and name information for a link actions
//   type, get port name, and put port name.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL type;
  FblcsNameL get;
  FblcsNameL put;
} FblcsLinkSymbol;

// FblcsDeclSymbol --
//   A symbol that stores information for a declaration.
typedef struct {
  FblcsSymbolTag tag;
  FblcsNameL name;
  FblcDeclId decl_id;
} FblcsDeclSymbol;

// FblcsSymbols --
//   Information associated with each location id in a program.
//
// symbolc/symbolv - A vector of symbol information indexed by FblcLocId.
// declc/declv - A vector mapping FblcDeclId to corresponding FblcLocId.
struct FblcsSymbols {
  size_t symbolc;
  FblcsSymbol** symbolv;

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

static void SetLocSymbol(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsSymbol* symbol)
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
  FblcsLocSymbol* symbol = arena->alloc(arena, sizeof(FblcsLocSymbol));
  symbol->tag = FBLCS_LOC_SYMBOL;
  symbol->loc.source = loc->source;
  symbol->loc.line = loc->line;
  symbol->loc.col = loc->col;
  SetLocSymbol(arena, symbols, loc_id, (FblcsSymbol*)symbol);
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
  FblcsIdSymbol* symbol = arena->alloc(arena, sizeof(FblcsIdSymbol));
  symbol->tag = FBLCS_ID_SYMBOL;
  symbol->name.name = name->name;
  symbol->name.loc = name->loc;
  SetLocSymbol(arena, symbols, loc_id, (FblcsSymbol*)symbol);
}

void SetLocTypedId(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* type, FblcsNameL* name)
{
  FblcsTypedIdSymbol* symbol = arena->alloc(arena, sizeof(FblcsTypedIdSymbol));
  symbol->tag = FBLCS_TYPED_ID_SYMBOL;
  symbol->type.name = type->name;
  symbol->type.loc = type->loc;
  symbol->name.name = name->name;
  symbol->name.loc = name->loc;
  SetLocSymbol(arena, symbols, loc_id, (FblcsSymbol*)symbol);
}

void SetLocLink(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* type, FblcsNameL* get, FblcsNameL* put)
{
  FblcsLinkSymbol* symbol = arena->alloc(arena, sizeof(FblcsLinkSymbol));
  symbol->tag = FBLCS_LINK_SYMBOL;
  symbol->type.name = type->name;
  symbol->type.loc = type->loc;
  symbol->get.name = get->name;
  symbol->get.loc = get->loc;
  symbol->put.name = put->name;
  symbol->put.loc = put->loc;
  SetLocSymbol(arena, symbols, loc_id, (FblcsSymbol*)symbol);
}

void SetLocDecl(FblcArena* arena, FblcsSymbols* symbols, FblcLocId loc_id, FblcsNameL* name, FblcDeclId decl_id)
{
  FblcsDeclSymbol* symbol = arena->alloc(arena, sizeof(FblcsDeclSymbol));
  symbol->tag = FBLCS_DECL_SYMBOL;
  symbol->name.name = name->name;
  symbol->name.loc = name->loc;
  symbol->decl_id = decl_id;
  SetLocSymbol(arena, symbols, loc_id, (FblcsSymbol*)symbol);

  while (decl_id >= symbols->declc) {
    FblcVectorAppend(arena, symbols->declv, symbols->declc, FBLC_NULL_ID);
  }
  assert(decl_id < symbols->declc);
  symbols->declv[decl_id] = loc_id;
}

FblcsLoc* LocIdLoc(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == FBLCS_LOC_SYMBOL) {
    FblcsLocSymbol* loc_symbol = (FblcsLocSymbol*)symbol;
    return &loc_symbol->loc;
  }
  return LocIdName(symbols, loc_id)->loc;
}

FblcsNameL* LocIdName(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  switch (symbol->tag) {
    case FBLCS_LOC_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case FBLCS_ID_SYMBOL: {
      FblcsIdSymbol* id_symbol = (FblcsIdSymbol*)symbol;
      return &id_symbol->name;
    }

    case FBLCS_TYPED_ID_SYMBOL: {
      FblcsTypedIdSymbol* typed_id_symbol = (FblcsTypedIdSymbol*)symbol;
      return &typed_id_symbol->name;
    }

    case FBLCS_LINK_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case FBLCS_DECL_SYMBOL: {
      FblcsDeclSymbol* decl_symbol = (FblcsDeclSymbol*)symbol;
      return &decl_symbol->name;
    }
  }
  assert(false && "Invalid tag");
  return NULL;
}

FblcsNameL* LocIdType(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  switch (symbol->tag) {
    case FBLCS_LOC_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case FBLCS_ID_SYMBOL: {
      assert(false && "TODO: Unsupported tag?");
      return NULL;
    }

    case FBLCS_TYPED_ID_SYMBOL: {
      FblcsTypedIdSymbol* typed_id_symbol = (FblcsTypedIdSymbol*)symbol;
      return &typed_id_symbol->type;
    }

    case FBLCS_LINK_SYMBOL: {
      FblcsLinkSymbol* link_symbol = (FblcsLinkSymbol*)symbol;
      return &link_symbol->type;
    }

    case FBLCS_DECL_SYMBOL: {
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
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == FBLCS_LINK_SYMBOL) {
    FblcsLinkSymbol* link_symbol = (FblcsLinkSymbol*)symbol;
    return &link_symbol->get;
  }
  assert(false && "unsupported tag");
  return NULL;
}

FblcsNameL* LocIdLinkPut(FblcsSymbols* symbols, FblcLocId loc_id)
{
  assert(loc_id < symbols->symbolc);
  FblcsSymbol* symbol = symbols->symbolv[loc_id];
  if (symbol->tag == FBLCS_LINK_SYMBOL) {
    FblcsLinkSymbol* link_symbol = (FblcsLinkSymbol*)symbol;
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
