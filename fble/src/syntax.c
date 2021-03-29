// syntax.c --
//   This file implements the fble abstract syntax routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, vfprintf, stderr
#include <string.h>   // for strcmp

#include "fble.h"
#include "syntax.h"

#define UNREACHABLE(x) assert(false && x)


// FbleFreeExpr -- see documentation in syntax.h
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
        FbleFreeKind(arena, binding->kind);
        FbleFreeExpr(arena, binding->type);
        FbleFreeName(arena, binding->name);
        FbleFreeExpr(arena, binding->expr);
      }
      FbleFree(arena, e->bindings.xs);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MODULE_PATH_EXPR: {
      FbleModulePathExpr* e = (FbleModulePathExpr*)expr;
      FbleFreeModulePath(arena, e->path);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_DATA_TYPE_EXPR: {
      FbleDataTypeExpr* e = (FbleDataTypeExpr*)expr;
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
        if (e->choices.xs[i].expr != NULL) {
          FbleFreeName(arena, e->choices.xs[i].name);
          FbleFreeExpr(arena, e->choices.xs[i].expr);
        }
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
        FbleFreeKind(arena, binding->kind);
        FbleFreeExpr(arena, binding->type);
        FbleFreeName(arena, binding->name);
        FbleFreeExpr(arena, binding->expr);
      }
      FbleFree(arena, e->bindings.xs);
      FbleFreeExpr(arena, e->body);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_POLY_VALUE_EXPR: {
      FblePolyValueExpr* e = (FblePolyValueExpr*)expr;
      FbleFreeKind(arena, e->arg.kind);
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
      FbleFreeExpr(arena, e->func);
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(arena, e->args.xs[i]);
      }
      FbleFree(arena, e->args.xs);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* e = (FbleLiteralExpr*)expr;
      FbleFreeExpr(arena, e->func);
      FbleFreeLoc(arena, e->word_loc);
      FbleFree(arena, (char*)e->word);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_DATA_ACCESS_EXPR: {
      FbleDataAccessExpr* e = (FbleDataAccessExpr*)expr;
      FbleFreeExpr(arena, e->object);
      FbleFreeName(arena, e->field);
      FbleFree(arena, expr);
      return;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleApplyExpr* e = (FbleApplyExpr*)expr;
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

// FbleCopyKind -- see documentation in syntax.h
FbleKind* FbleCopyKind(FbleArena* arena, FbleKind* kind)
{
  assert(kind != NULL);
  kind->refcount++;
  return kind;
}

// FbleFreeKind -- see documentation in syntax.h
void FbleFreeKind(FbleArena* arena, FbleKind* kind)
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
          FbleFreeKind(arena, poly->arg);
          FbleFreeKind(arena, poly->rkind);
          FbleFree(arena, poly);
          break;
        }
      }
    }
  }
}
