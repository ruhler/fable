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
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv.xs[type_id];
  if (type->_base.tag == FBLC_STRUCT_DECL) {
    fprintf(stream, "%s(", FblcsDeclName(sprog, type_id));
    for (size_t i = 0; i < type->fieldv.size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcsPrintValue(stream, sprog, type->fieldv.xs[i], value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->_base.tag == FBLC_UNION_DECL) {
    fprintf(stream, "%s:%s(", FblcsDeclName(sprog, type_id), FblcsFieldName(sprog, type_id, value->tag));
    FblcsPrintValue(stream, sprog, type->fieldv.xs[value->tag], value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcsLookupDecl -- See documentation in fblcs.h
FblcDeclId FblcsLookupDecl(FblcsProgram* sprog, FblcsName name)
{
  for (FblcDeclId i = 0; i < sprog->program->declv.size; ++i) {
    if (FblcsNamesEqual(FblcsDeclName(sprog, i), name)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsLookupField -- See documentation in fblcs.h
FblcFieldId FblcsLookupField(FblcsProgram* sprog, FblcTypeId type_id, FblcsName field)
{
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv.xs[type_id];
  assert(type->_base.tag == FBLC_STRUCT_DECL || type->_base.tag == FBLC_UNION_DECL);
  for (FblcFieldId i = 0; i < type->fieldv.size; ++i) {
    if (FblcsNamesEqual(FblcsFieldName(sprog, type_id, i), field)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsLookupPort -- See documentation in fblcs.h
FblcFieldId FblcsLookupPort(FblcsProgram* sprog, FblcDeclId proc_id, FblcsName port)
{
  FblcProcDecl* proc = (FblcProcDecl*)sprog->program->declv.xs[proc_id];
  FblcsProcDecl* sproc = (FblcsProcDecl*)sprog->sdeclv.xs[proc_id];
  for (FblcFieldId i = 0; i < proc->portv.size; ++i) {
    if (FblcsNamesEqual(sproc->portv.xs[i].name.name, port)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsDeclName -- See documentation in fblcs.h
FblcsName FblcsDeclName(FblcsProgram* sprog, FblcDeclId decl_id)
{
  FblcsDecl* decl = sprog->sdeclv.xs[decl_id];
  return decl->name.name;
}

// FblcsFieldName  -- See documentation in fblcs.h
FblcsName FblcsFieldName(FblcsProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id)
{
  assert(decl_id < sprog->program->declv.size);
  FblcTypeDecl* type = (FblcTypeDecl*)sprog->program->declv.xs[decl_id];
  assert(type->_base.tag == FBLC_STRUCT_DECL || type->_base.tag == FBLC_UNION_DECL);
  assert(field_id < type->fieldv.size);
  FblcsTypeDecl* stype = (FblcsTypeDecl*)sprog->sdeclv.xs[decl_id];
  return stype->fieldv.xs[field_id].name.name;
}
