// syntax.c --
//   This file implements the fble abstract syntax routines.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdio.h>    // for fprintf, vfprintf, stderr
#include <string.h>   // for strcmp

#include "fble-syntax.h"

#define UNREACHABLE(x) assert(false && x)

static void FreeExpr(FbleArena* arena, FbleExpr* expr);

// FreeExpr --
//   Free resources associated with an expression.
//
// Inputs:
//   arena - arena to use for allocations.
//   expr - expression to free. May be NULL.
//
// Side effect:
//   Frees resources associated with expr.
static void FreeExpr(FbleArena* arena, FbleExpr* expr)
{
  if (expr == NULL) {
    return;
  }

  switch (expr->tag) {
    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* e = (FbleTypeofExpr*)expr;
      FreeExpr(arena, e->expr);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* e = (FbleVarExpr*)expr;
      FbleFree(arena, (char*)e->var.name);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* e = (FbleLetExpr*)expr;
      for (size_t i = 0; i < e->bindings.size; ++i) {
        FbleBinding* binding = e->bindings.xs + i;
        FbleKindRelease(arena, binding->kind);
        FreeExpr(arena, binding->type);
        FbleFree(arena, (char*)binding->name.name);
        FreeExpr(arena, binding->expr);
      }
      FbleFree(arena, e->bindings.xs);
      FreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MODULE_REF_EXPR: {
      FbleModuleRefExpr* e = (FbleModuleRefExpr*)expr;
      for (size_t i = 0; i < e->ref.path.size; ++i) {
        FbleFree(arena, (char*)e->ref.path.xs[i].name);
      }
      FbleFree(arena, e->ref.path.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_STRUCT_TYPE_EXPR: {
      FbleStructTypeExpr* e = (FbleStructTypeExpr*)expr;
      for (size_t i = 0; i < e->fields.size; ++i) {
        FreeExpr(arena, e->fields.xs[i].type);
        FbleFree(arena, (char*)e->fields.xs[i].name.name);
      }
      FbleFree(arena, e->fields.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* e = (FbleStructValueImplicitTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFree(arena, (char*)e->args.xs[i].name.name);
        FreeExpr(arena, e->args.xs[i].expr);
      }
      FbleFree(arena, e->args.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_UNION_TYPE_EXPR: {
      FbleUnionTypeExpr* e = (FbleUnionTypeExpr*)expr;
      for (size_t i = 0; i < e->fields.size; ++i) {
        FreeExpr(arena, e->fields.xs[i].type);
        FbleFree(arena, (char*)e->fields.xs[i].name.name);
      }
      FbleFree(arena, e->fields.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* e = (FbleUnionValueExpr*)expr;
      FreeExpr(arena, e->type);
      FbleFree(arena, (char*)e->field.name);
      FreeExpr(arena, e->arg);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* e = (FbleUnionSelectExpr*)expr;
      FreeExpr(arena, e->condition);
      for (size_t i = 0; i < e->choices.size; ++i) {
        FbleFree(arena, (char*)e->choices.xs[i].name.name);
        FreeExpr(arena, e->choices.xs[i].expr);
      }
      FbleFree(arena, e->choices.xs);
      FreeExpr(arena, e->default_);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncTypeExpr* e = (FbleFuncTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FreeExpr(arena, e->args.xs[i]);
      }
      FbleFree(arena, e->args.xs);
      FreeExpr(arena, e->rtype);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* e = (FbleFuncValueExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FreeExpr(arena, e->args.xs[i].type);
        FbleFree(arena, (char*)e->args.xs[i].name.name);
      }
      FbleFree(arena, e->args.xs);
      FreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcTypeExpr* e = (FbleProcTypeExpr*)expr;
      FreeExpr(arena, e->type);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* e = (FbleEvalExpr*)expr;
      FreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* e = (FbleLinkExpr*)expr;
      FreeExpr(arena, e->type);
      FbleFree(arena, (char*)e->get.name);
      FbleFree(arena, (char*)e->put.name);
      FreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* e = (FbleExecExpr*)expr;
      for (size_t i = 0; i < e->bindings.size; ++i) {
        FbleBinding* binding = e->bindings.xs + i;
        FbleKindRelease(arena, binding->kind);
        FreeExpr(arena, binding->type);
        FbleFree(arena, (char*)binding->name.name);
        FreeExpr(arena, binding->expr);
      }
      FbleFree(arena, e->bindings.xs);
      FreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_POLY_EXPR: {
      FblePolyExpr* e = (FblePolyExpr*)expr;
      FbleKindRelease(arena, e->arg.kind);
      FbleFree(arena, (char*)e->arg.name.name);
      FreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* e = (FblePolyApplyExpr*)expr;
      FreeExpr(arena, e->poly);
      FreeExpr(arena, e->arg);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* e = (FbleListExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FreeExpr(arena, e->args.xs[i]);
      }
      FbleFree(arena, e->args.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* e = (FbleLiteralExpr*)expr;
      FreeExpr(arena, e->spec);
      FbleFree(arena, (char*)e->word);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MISC_ACCESS_EXPR: {
      FbleMiscAccessExpr* e = (FbleMiscAccessExpr*)expr;
      FreeExpr(arena, e->object);
      FbleFree(arena, (char*)e->field.name);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleMiscApplyExpr* e = (FbleMiscApplyExpr*)expr;
      FbleFree(arena, e->misc);
      for (size_t i = 0; i < e->args.size; ++i) {
        FreeExpr(arena, e->args.xs[i]);
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
  FbleString* string = FbleAlloc(arena, FbleString);
  string->refcount = 1;
  string->str = str;
  return string;
}

// FbleStringRetain -- see documentation in fble-syntax.h
FbleString* FbleStringRetain(FbleString* string)
{
  string->refcount++;
  return string;
}

// FbleStringRelease -- see documentation in fble-syntax.h
void FbleStringRelease(FbleArena* arena, FbleString* string)
{
  if (--string->refcount == 0) {
    FbleFree(arena, (char*)string->str);
    FbleFree(arena, string);
  }
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

// FbleKindRetain -- see documentation in fble-syntax.h
FbleKind* FbleKindRetain(FbleArena* arena, FbleKind* kind)
{
  assert(kind != NULL);
  kind->refcount++;
  return kind;
}

// FbleKindRelease -- see documentation in fble-syntax.h
void FbleKindRelease(FbleArena* arena, FbleKind* kind)
{
  if (kind != NULL) {
    assert(kind->refcount > 0);
    kind->refcount--;
    if (kind->refcount == 0) {
      switch (kind->tag) {
        case FBLE_BASIC_KIND: {
          FbleFree(arena, kind);
          break;
        }

        case FBLE_POLY_KIND: {
          FblePolyKind* poly = (FblePolyKind*)kind;
          FbleKindRelease(arena, poly->arg);
          FbleKindRelease(arena, poly->rkind);
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
    FbleFree(arena, (char*)program->modules.xs[i].name.name);
    FreeExpr(arena, program->modules.xs[i].value);
  }
  FbleFree(arena, program->modules.xs);
  FreeExpr(arena, program->main);
  FbleFree(arena, program);
}
