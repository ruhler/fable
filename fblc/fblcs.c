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

// FblcsPrintValue -- see documentation in fblcs.h.
void FblcsPrintValue(FILE* stream, FblcsProgram* sprog, FblcType* type, FblcValue* value)
{
  if (type->kind == FBLC_STRUCT_KIND) {
    fprintf(stream, "%s(", FblcsTypeName(sprog, type));
    for (size_t i = 0; i < type->fieldv.size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FblcsPrintValue(stream, sprog, type->fieldv.xs[i], value->fields[i]);
    }
    fprintf(stream, ")");
  } else if (type->kind == FBLC_UNION_KIND) {
    fprintf(stream, "%s:%s(", FblcsTypeName(sprog, type), FblcsFieldName(sprog, type, value->tag));
    FblcsPrintValue(stream, sprog, type->fieldv.xs[value->tag], value->fields[0]);
    fprintf(stream, ")");
  } else {
    assert(false && "Invalid Kind");
  }
}

// FblcsLookupType -- See documentation in fblcs.h
FblcType* FblcsLookupType(FblcsProgram* sprog, FblcsName name)
{
  for (size_t i = 0; i < sprog->program->typev.size; ++i) {
    FblcType* type = sprog->program->typev.xs[i];
    if (FblcsNamesEqual(FblcsTypeName(sprog, type), name)) {
      return type;
    }
  }
  return NULL;
}

// FblcsLookupFunc -- See documentation in fblcs.h
FblcFunc* FblcsLookupFunc(FblcsProgram* sprog, FblcsName name)
{
  for (size_t i = 0; i < sprog->program->funcv.size; ++i) {
    FblcFunc* func = sprog->program->funcv.xs[i];
    if (FblcsNamesEqual(FblcsFuncName(sprog, func), name)) {
      return func;
    }
  }
  return NULL;
}

// FblcsLookupProc -- See documentation in fblcs.h
FblcProc* FblcsLookupProc(FblcsProgram* sprog, FblcsName name)
{
  for (size_t i = 0; i < sprog->program->procv.size; ++i) {
    FblcProc* proc = sprog->program->procv.xs[i];
    if (FblcsNamesEqual(FblcsProcName(sprog, proc), name)) {
      return proc;
    }
  }
  return NULL;
}

// FblcsLookupEntry -- See documentation in fblcs.h
FblcProc* FblcsLookupEntry(FblcArena* arena, FblcsProgram* sprog, FblcsName name)
{
  FblcProc* proc = FblcsLookupProc(sprog, name);
  if (proc == NULL) {
    FblcFunc* func = FblcsLookupFunc(sprog, name);
    if (func == NULL) {
      return NULL;
    }

    // Make a proc wrapper for the function.
    FblcEvalActn* body = arena->alloc(arena, sizeof(FblcEvalActn));
    body->_base.tag = FBLC_EVAL_ACTN;
    body->arg = func->body;

    proc = arena->alloc(arena, sizeof(FblcProc));
    proc->portv.size = 0;
    proc->portv.xs = NULL;
    proc->argv.size = func->argv.size;
    proc->argv.xs = func->argv.xs;
    proc->return_type = func->return_type;
    proc->body = &body->_base;
  }
  return proc;
}

// FblcsLookupField -- See documentation in fblcs.h
FblcFieldId FblcsLookupField(FblcsProgram* sprog, FblcType* type, FblcsName field)
{
  for (FblcFieldId i = 0; i < type->fieldv.size; ++i) {
    if (FblcsNamesEqual(FblcsFieldName(sprog, type, i), field)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsLookupPort -- See documentation in fblcs.h
FblcFieldId FblcsLookupPort(FblcsProgram* sprog, FblcProc* proc, FblcsName port)
{
  FblcsProc* sproc = sprog->sprocv.xs[proc->id];
  for (FblcFieldId i = 0; i < proc->portv.size; ++i) {
    if (FblcsNamesEqual(sproc->portv.xs[i].name.name, port)) {
      return i;
    }
  }
  return FBLC_NULL_ID;
}

// FblcsTypeName -- See documentation in fblcs.h
FblcsName FblcsTypeName(FblcsProgram* sprog, FblcType* type)
{
  return sprog->stypev.xs[type->id]->name.name;
}

// FblcsFuncName -- See documentation in fblcs.h
FblcsName FblcsFuncName(FblcsProgram* sprog, FblcFunc* func)
{
  return sprog->sfuncv.xs[func->id]->name.name;
}

// FblcsProcName -- See documentation in fblcs.h
FblcsName FblcsProcName(FblcsProgram* sprog, FblcProc* proc)
{
  return sprog->sprocv.xs[proc->id]->name.name;
}

// FblcsFieldName  -- See documentation in fblcs.h
FblcsName FblcsFieldName(FblcsProgram* sprog, FblcType* type, FblcFieldId field_id)
{
  assert(field_id < type->fieldv.size);
  FblcsType* stype = sprog->stypev.xs[type->id];
  return stype->fieldv.xs[field_id].name.name;
}
