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

// FblcsPrintValue -- see documentation in fblcs.h.
void FblcsPrintValue(FILE* stream, FblcsProgram* sprog, FblcTypeId type_id, FblcValue* value)
{
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];
  if (type->tag == FBLC_STRUCT_DECL) {
    fprintf(stream, "%s(", DeclName(sprog, type_id));
    for (size_t i = 0; i < type->fieldc; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcsPrintValue(stream, sprog, type->fieldv[i], value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->tag == FBLC_UNION_DECL) {
    fprintf(stream, "%s:%s(", DeclName(sprog, type_id), FieldName(sprog, type_id, value->tag));
    FblcsPrintValue(stream, sprog, type->fieldv[value->tag], value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
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

// DeclName -- See documentation in fblcs.h
FblcsName DeclName(FblcsProgram* sprog, FblcDeclId decl_id)
{
  FblcLocId decl_loc_id = sprog->symbols->declv[decl_id];
  return LocIdName(sprog->symbols, decl_loc_id)->name;
}

// FieldName -- See documentation in fblcs.h
FblcsName FieldName(FblcsProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id)
{
  FblcLocId field_loc_id = sprog->symbols->declv[decl_id] + field_id + 1;
  return LocIdName(sprog->symbols, field_loc_id)->name;
}

// FblcsLookupDecl -- See documentation in fblcs.h
FblcDeclId FblcsLookupDecl(FblcsProgram* sprog, FblcsName name)
{
  for (FblcDeclId i = 0; i < sprog->program->declc; ++i) {
    if (FblcsNamesEqual(DeclName(sprog, i), name)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}
// FblcsLookupPort -- See documentation in fblcs.h
FblcFieldId FblcsLookupPort(FblcsProgram* sprog, FblcDeclId proc_id, FblcsName port)
{
  FblcLocId port_loc_id = sprog->symbols->declv[proc_id] + 1;
  FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv[proc_id];
  for (FblcFieldId i = 0; i < proc->portc; ++i) {
    if (FblcsNamesEqual(LocIdName(sprog->symbols, port_loc_id + i)->name, port)) {
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
