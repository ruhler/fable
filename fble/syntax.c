// syntax.c --
//   This file implements the fble abstract syntax routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr
#include <string.h>   // for strcmp

#include "fble-syntax.h"

#define UNREACHABLE(x) assert(false && x)

static size_t Checksum(const char* str);


// Checksum --
//   Compute a checksum for the given string. Used to help detect double free
//   of FbleString.
//
// Inputs:
//   str - the string to get the checksum for
//
// Results:
//   A checksum of the string.
//
// Side effects:
//   None.
static size_t Checksum(const char* str)
{
  size_t checksum = 4321;
  while (*str != '\0') {
    checksum = checksum * 27 + *str;
    str++;
  }
  return checksum;
}

// FbleFreeExpr -- see documentation in fble-syntax.h
void FbleFreeExpr(FbleArena* arena, FbleExpr* expr)
{
  if (expr == NULL) {
    return;
  }

  FbleFreeLoc(arena, expr->loc);
  switch (expr->tag) {
    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* e = (FbleTypeofExpr*)expr;
      FbleFreeExpr(arena, e->expr);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* e = (FbleVarExpr*)expr;
      FbleFreeName(arena, e->var);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* e = (FbleLetExpr*)expr;
      for (size_t i = 0; i < e->bindings.size; ++i) {
        FbleBinding* binding = e->bindings.xs + i;
        FbleReleaseKind(arena, binding->kind);
        FbleFreeExpr(arena, binding->type);
        FbleFreeName(arena, binding->name);
        FbleFreeExpr(arena, binding->expr);
      }
      FbleFree(arena, e->bindings.xs);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MODULE_REF_EXPR: {
      FbleModuleRefExpr* e = (FbleModuleRefExpr*)expr;
      for (size_t i = 0; i < e->ref.path.size; ++i) {
        FbleFreeName(arena, e->ref.path.xs[i]);
      }
      FbleFree(arena, e->ref.path.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* e = (FbleStructTypeExpr*)expr;
      for (size_t i = 0; i < e->fields.size; ++i) {
        FbleFreeExpr(arena, e->fields.xs[i].type);
        FbleFreeName(arena, e->fields.xs[i].name);
      }
      FbleFree(arena, e->fields.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* e = (FbleStructValueImplicitTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeName(arena, e->args.xs[i].name);
        FbleFreeExpr(arena, e->args.xs[i].expr);
      }
      FbleFree(arena, e->args.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_UNION_TYPE_EXPR: {
      FbleUnionTypeExpr* e = (FbleUnionTypeExpr*)expr;
      for (size_t i = 0; i < e->fields.size; ++i) {
        FbleFreeExpr(arena, e->fields.xs[i].type);
        FbleFreeName(arena, e->fields.xs[i].name);
      }
      FbleFree(arena, e->fields.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* e = (FbleUnionValueExpr*)expr;
      FbleFreeExpr(arena, e->type);
      FbleFreeName(arena, e->field);
      FbleFreeExpr(arena, e->arg);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* e = (FbleUnionSelectExpr*)expr;
      FbleFreeExpr(arena, e->condition);
      for (size_t i = 0; i < e->choices.size; ++i) {
        FbleFreeName(arena, e->choices.xs[i].name);
        FbleFreeExpr(arena, e->choices.xs[i].expr);
      }
      FbleFree(arena, e->choices.xs);
      FbleFreeExpr(arena, e->default_);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncTypeExpr* e = (FbleFuncTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(arena, e->args.xs[i]);
      }
      FbleFree(arena, e->args.xs);
      FbleFreeExpr(arena, e->rtype);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* e = (FbleFuncValueExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(arena, e->args.xs[i].type);
        FbleFreeName(arena, e->args.xs[i].name);
      }
      FbleFree(arena, e->args.xs);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcTypeExpr* e = (FbleProcTypeExpr*)expr;
      FbleFreeExpr(arena, e->type);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* e = (FbleEvalExpr*)expr;
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* e = (FbleLinkExpr*)expr;
      FbleFreeExpr(arena, e->type);
      FbleFreeName(arena, e->get);
      FbleFreeName(arena, e->put);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* e = (FbleExecExpr*)expr;
      for (size_t i = 0; i < e->bindings.size; ++i) {
        FbleBinding* binding = e->bindings.xs + i;
        FbleReleaseKind(arena, binding->kind);
        FbleFreeExpr(arena, binding->type);
        FbleFreeName(arena, binding->name);
        FbleFreeExpr(arena, binding->expr);
      }
      FbleFree(arena, e->bindings.xs);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* e = (FblePolyExpr*)expr;
      FbleReleaseKind(arena, e->arg.kind);
      FbleFreeName(arena, e->arg.name);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* e = (FblePolyApplyExpr*)expr;
      FbleFreeExpr(arena, e->poly);
      FbleFreeExpr(arena, e->arg);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* e = (FbleListExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(arena, e->args.xs[i]);
      }
      FbleFree(arena, e->args.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* e = (FbleLiteralExpr*)expr;
      FbleFreeExpr(arena, e->spec);
      FbleFreeLoc(arena, e->word_loc);
      FbleFree(arena, (char*)e->word);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      FbleMiscAccessExpr* e = (FbleMiscAccessExpr*)expr;
      FbleFreeExpr(arena, e->object);
      FbleFreeName(arena, e->field);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* e = (FbleMiscApplyExpr*)expr;
      FbleFreeExpr(arena, e->misc);
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(arena, e->args.xs[i]);
      }
      FbleFree(arena, e->args.xs);
      FbleFree(arena, expr);
      return;
    }
  }

  UNREACHABLE("should never get here");
}

// FbleNewString -- see documentation in fble-syntax.h
FbleString* FbleNewString(FbleArena* arena, const char* str)
{
  char* str_copy = FbleArrayAlloc(arena, char, strlen(str) + 1);
  strcpy(str_copy, str);

  FbleString* string = FbleAlloc(arena, FbleString);
  string->refcount = 1;
  string->str = str_copy;
  string->checksum = Checksum(str);
  return string;
}

// FbleRetainString -- see documentation in fble-syntax.h
FbleString* FbleRetainString(FbleString* string)
{
  string->refcount++;
  return string;
}

// FbleReleaseString -- see documentation in fble-syntax.h
void FbleReleaseString(FbleArena* arena, FbleString* string)
{
  // If the string checksum doesn't match, the string is corrupted. That
  // suggests we have already freed this string, and that something is messed
  // up with tracking FbleString refcounts. 
  // Of course, if the string is corrupt, we'll probably get a segfault
  // computing the checksum instead of this nice assert failure. Oh well. The
  // debugger should make it obvious what's wrong in that case.
  assert(string->checksum == Checksum(string->str) && "corrupt FbleString");
  if (--string->refcount == 0) {
    FbleFree(arena, (char*)string->str);
    FbleFree(arena, string);
  }
}

// FbleCopyLoc -- see documentation in fble-syntax.h
FbleLoc FbleCopyLoc(FbleLoc loc)
{
  FbleRetainString(loc.source);
  return loc;
}

// FbleFreeLoc -- see documentation in fble-syntax.h
void FbleFreeLoc(FbleArena* arena, FbleLoc loc)
{
  FbleReleaseString(arena, loc.source);
}

// FbleReportWarning -- see documentation in fble-syntax.h
void FbleReportWarning(const char* format, FbleLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: warning: ", loc->source->str, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// FbleReportError -- see documentation in fble-syntax.h
void FbleReportError(const char* format, FbleLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source->str, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// FbleNamesEqual -- see documentation in fble-syntax.h
bool FbleNamesEqual(FbleName* a, FbleName* b)
{
  return a->space == b->space && strcmp(a->name, b->name) == 0;
}

// FbleFreeName -- see documentation in fble-syntax.h
void FbleFreeName(FbleArena* arena, FbleName name)
{
  FbleFreeLoc(arena, name.loc);
  FbleFree(arena, (char*)name.name);
}

// FblePrintName -- see documentation in fble-syntax.h
void FblePrintName(FILE* stream, FbleName* name)
{
  fprintf(stream, name->name);
  switch (name->space) {
    case FBLE_NORMAL_NAME_SPACE:
      // Nothing to add here
      break;

    case FBLE_TYPE_NAME_SPACE:
      fprintf(stream, "@");
      break;

    case FBLE_MODULE_NAME_SPACE:
      fprintf(stream, "%%");
      break;
  }
}

// FbleRetainKind -- see documentation in fble-syntax.h
FbleKind* FbleRetainKind(FbleArena* arena, FbleKind* kind)
{
  assert(kind != NULL);
  kind->refcount++;
  return kind;
}

// FbleReleaseKind -- see documentation in fble-syntax.h
void FbleReleaseKind(FbleArena* arena, FbleKind* kind)
{
  if (kind != NULL) {
    assert(kind->refcount > 0);
    kind->refcount--;
    if (kind->refcount == 0) {
      FbleFreeLoc(arena, kind->loc);
      switch (kind->tag) {
        case FBLE_BASIC_KIND: {
          FbleFree(arena, kind);
          break;
        }

        case FBLE_POLY_KIND: {
          FblePolyKind* poly = (FblePolyKind*)kind;
          FbleReleaseKind(arena, poly->arg);
          FbleReleaseKind(arena, poly->rkind);
          FbleFree(arena, poly);
          break;
        }
      }
    }
  }
}

// FbleFreeProgram -- see documentation in fble-syntax.h
void FbleFreeProgram(FbleArena* arena, FbleProgram* program)
{
  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleFreeName(arena, program->modules.xs[i].name);
    FbleFreeExpr(arena, program->modules.xs[i].value);
  }
  FbleFree(arena, program->modules.xs);
  FbleFreeExpr(arena, program->main);
  FbleFree(arena, program);
}
