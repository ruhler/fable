// fblcs.c --
//   This file implements routines for manipulating fblc symbol information,
//   which maps source level names and locations to machine level program
//   constructs.

#include <assert.h>     // For assert
#include <stdarg.h>     // For va_start
#include <stdio.h>      // For fprintf
#include <string.h>     // For strcmp

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

// FblcsLookupType -- see documentation in fblcs.h
FblcsType* FblcsLookupType(FblcsProgram* prog, FblcsName name)
{
  for (size_t i = 0; i < prog->typev.size; ++i) {
    if (FblcsNamesEqual(prog->typev.xs[i]->name.name, name)) {
      return prog->typev.xs[i];
    }
  }
  return NULL;
}

// FblcsLookupFunc -- see documentation in fblcs.h
FblcsFunc* FblcsLookupFunc(FblcsProgram* prog, FblcsName name)
{
  for (size_t i = 0; i < prog->funcv.size; ++i) {
    if (FblcsNamesEqual(prog->funcv.xs[i]->name.name, name)) {
      return prog->funcv.xs[i];
    }
  }
  return NULL;
}

// FblcsLookupProc -- see documentation in fblcs.h
FblcsProc* FblcsLookupProc(FblcsProgram* prog, FblcsName name)
{
  for (size_t i = 0; i < prog->procv.size; ++i) {
    if (FblcsNamesEqual(prog->procv.xs[i]->name.name, name)) {
      return prog->procv.xs[i];
    }
  }
  return NULL;
}

// FblcsPrintValue -- see documentation in fblcs.h
void FblcsPrintValue(FILE* stream, FblcsProgram* prog, FblcsNameL* typename, FblcValue* value)
{
  FblcsType* type = FblcsLookupType(prog, typename->name);
  assert(type != NULL && "Could not find type");
  if (type->kind == FBLCS_STRUCT_KIND) {
    fprintf(stream, "%s(", type->name.name);
    for (size_t i = 0; i < type->fieldv.size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcsPrintValue(stream, prog, &type->fieldv.xs[i].name, value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLCS_UNION_KIND) {
    fprintf(stream, "%s:%s(", type->name.name, type->fieldv.xs[value->tag].name.name);
    FblcsPrintValue(stream, prog, &type->fieldv.xs[value->tag].name, value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

