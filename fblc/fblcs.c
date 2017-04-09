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
void FblcsPrintValue(FILE* stream, FblcsProgram* sprog, FblcTypeDecl* type, FblcValue* value)
{
  if (type->_base.tag == FBLC_STRUCT_DECL) {
    fprintf(stream, "%s(", FblcsDeclName(sprog, &type->_base));
    for (size_t i = 0; i < type->fieldv.size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcsPrintValue(stream, sprog, type->fieldv.xs[i], value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->_base.tag == FBLC_UNION_DECL) {
    fprintf(stream, "%s:%s(", FblcsDeclName(sprog, &type->_base), FblcsFieldName(sprog, type, value->tag));
    FblcsPrintValue(stream, sprog, type->fieldv.xs[value->tag], value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcsLookupDecl -- See documentation in fblcs.h
FblcDecl* FblcsLookupDecl(FblcsProgram* sprog, FblcsName name)
{
  for (size_t i = 0; i < sprog->program->declv.size; ++i) {
    FblcDecl* decl = sprog->program->declv.xs[i];
    if (FblcsNamesEqual(FblcsDeclName(sprog, decl), name)) {
      return decl;
    }
  }
  return NULL;
}

// FblcsLookupField -- See documentation in fblcs.h
FblcFieldId FblcsLookupField(FblcsProgram* sprog, FblcTypeDecl* type, FblcsName field)
{
  assert(type->_base.tag == FBLC_STRUCT_DECL || type->_base.tag == FBLC_UNION_DECL);
  for (FblcFieldId i = 0; i < type->fieldv.size; ++i) {
    if (FblcsNamesEqual(FblcsFieldName(sprog, type, i), field)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsLookupPort -- See documentation in fblcs.h
FblcFieldId FblcsLookupPort(FblcsProgram* sprog, FblcProcDecl* proc, FblcsName port)
{
  FblcsProcDecl* sproc = (FblcsProcDecl*)sprog->sdeclv.xs[proc->_base.id];
  for (FblcFieldId i = 0; i < proc->portv.size; ++i) {
    if (FblcsNamesEqual(sproc->portv.xs[i].name.name, port)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsDeclName -- See documentation in fblcs.h
FblcsName FblcsDeclName(FblcsProgram* sprog, FblcDecl* decl)
{
  return sprog->sdeclv.xs[decl->id]->name.name;
}

// FblcsFieldName  -- See documentation in fblcs.h
FblcsName FblcsFieldName(FblcsProgram* sprog, FblcTypeDecl* type, FblcFieldId field_id)
{
  assert(type->_base.tag == FBLC_STRUCT_DECL || type->_base.tag == FBLC_UNION_DECL);
  assert(field_id < type->fieldv.size);
  FblcsTypeDecl* stype = (FblcsTypeDecl*)sprog->sdeclv.xs[type->_base.id];
  return stype->fieldv.xs[field_id].name.name;
}
