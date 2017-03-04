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
    fprintf(stream, "%s(", FblcsDeclName(sprog, type_id));
    for (size_t i = 0; i < type->fieldc; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcsPrintValue(stream, sprog, type->fieldv[i], value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->tag == FBLC_UNION_DECL) {
    fprintf(stream, "%s:%s(", FblcsDeclName(sprog, type_id), FblcsFieldName(sprog, type_id, value->tag));
    FblcsPrintValue(stream, sprog, type->fieldv[value->tag], value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcsLookupDecl -- See documentation in fblcs.h
FblcDeclId FblcsLookupDecl(FblcsProgram* sprog, FblcsName name)
{
  for (FblcDeclId i = 0; i < sprog->program->declc; ++i) {
    if (FblcsNamesEqual(FblcsDeclName(sprog, i), name)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsLookupField -- See documentation in fblcs.h
FblcFieldId FblcsLookupField(FblcsProgram* sprog, FblcTypeId type_id, FblcsName field)
{
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv[type_id];
  assert(type->tag == FBLC_STRUCT_DECL || type->tag == FBLC_UNION_DECL);
  for (FblcFieldId i = 0; i < type->fieldc; ++i) {
    if (FblcsNamesEqual(FblcsFieldName(sprog, type_id, i), field)) {
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
    FblcsTypedIdSymbol* port_i = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[port_loc_id + i];
    assert(port_i->tag == FBLCS_TYPED_ID_SYMBOL);
    if (FblcsNamesEqual(port_i->name.name, port)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsDeclName -- See documentation in fblcs.h
FblcsName FblcsDeclName(FblcsProgram* sprog, FblcDeclId decl_id)
{
  FblcLocId decl_loc_id = sprog->symbols->declv[decl_id];
  FblcsDeclSymbol* decl = (FblcsDeclSymbol*)sprog->symbols->symbolv[decl_loc_id];
  assert(decl->tag == FBLCS_DECL_SYMBOL);
  return decl->name.name;
}

// FblcsFieldName  -- See documentation in fblcs.h
FblcsName FblcsFieldName(FblcsProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id)
{
  FblcLocId field_loc_id = sprog->symbols->declv[decl_id] + field_id + 1;
  FblcsTypedIdSymbol* field = (FblcsTypedIdSymbol*)sprog->symbols->symbolv[field_loc_id];
  assert(field->tag == FBLCS_TYPED_ID_SYMBOL);
  return field->name.name;
}
