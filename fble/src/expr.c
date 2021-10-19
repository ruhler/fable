// expr.c --
//   This file implements the fble abstract syntax routines.

#include "expr.h"

#include <assert.h>       // for assert

#include "fble-alloc.h"   // for FbleFree

#define UNREACHABLE(x) assert(false && x)

// FbleFreeExpr -- see documentation in expr.h
void FbleFreeExpr(FbleExpr* expr)
{
  if (expr == NULL) {
    return;
  }

  FbleFreeLoc(expr->loc);
  switch (expr->tag) {
    case FBLE_TYPEOF_EXPR: {
      FbleTypeofExpr* e = (FbleTypeofExpr*)expr;
      FbleFreeExpr(e->expr);
      FbleFree(expr);
      return;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* e = (FbleVarExpr*)expr;
      FbleFreeName(e->var);
      FbleFree(expr);
      return;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* e = (FbleLetExpr*)expr;
      for (size_t i = 0; i < e->bindings.size; ++i) {
        FbleBinding* binding = e->bindings.xs + i;
        FbleFreeKind(binding->kind);
        FbleFreeExpr(binding->type);
        FbleFreeName(binding->name);
        FbleFreeExpr(binding->expr);
      }
      FbleFree(e->bindings.xs);
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_DATA_TYPE_EXPR: {
      FbleDataTypeExpr* e = (FbleDataTypeExpr*)expr;
      for (size_t i = 0; i < e->fields.size; ++i) {
        FbleFreeExpr(e->fields.xs[i].type);
        FbleFreeName(e->fields.xs[i].name);
      }
      FbleFree(e->fields.xs);
      FbleFree(expr);
      return;
    }

    case FBLE_DATA_ACCESS_EXPR: {
      FbleDataAccessExpr* e = (FbleDataAccessExpr*)expr;
      FbleFreeExpr(e->object);
      FbleFreeName(e->field);
      FbleFree(expr);
      return;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* e = (FbleStructValueImplicitTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeName(e->args.xs[i].name);
        FbleFreeExpr(e->args.xs[i].expr);
      }
      FbleFree(e->args.xs);
      FbleFree(expr);
      return;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* e = (FbleUnionValueExpr*)expr;
      FbleFreeExpr(e->type);
      FbleFreeName(e->field);
      FbleFreeExpr(e->arg);
      FbleFree(expr);
      return;
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* e = (FbleUnionSelectExpr*)expr;
      FbleFreeExpr(e->condition);
      for (size_t i = 0; i < e->choices.size; ++i) {
        if (e->choices.xs[i].expr != NULL) {
          FbleFreeName(e->choices.xs[i].name);
          FbleFreeExpr(e->choices.xs[i].expr);
        }
      }
      FbleFree(e->choices.xs);
      FbleFreeExpr(e->default_);
      FbleFree(expr);
      return;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncTypeExpr* e = (FbleFuncTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(e->args.xs[i]);
      }
      FbleFree(e->args.xs);
      FbleFreeExpr(e->rtype);
      FbleFree(expr);
      return;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* e = (FbleFuncValueExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(e->args.xs[i].type);
        FbleFreeName(e->args.xs[i].name);
      }
      FbleFree(e->args.xs);
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_PROC_TYPE_EXPR: {
      FbleProcTypeExpr* e = (FbleProcTypeExpr*)expr;
      FbleFreeExpr(e->type);
      FbleFree(expr);
      return;
    }

    case FBLE_EVAL_EXPR: {
      FbleEvalExpr* e = (FbleEvalExpr*)expr;
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_LINK_EXPR: {
      FbleLinkExpr* e = (FbleLinkExpr*)expr;
      FbleFreeExpr(e->type);
      FbleFreeName(e->get);
      FbleFreeName(e->put);
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_EXEC_EXPR: {
      FbleExecExpr* e = (FbleExecExpr*)expr;
      for (size_t i = 0; i < e->bindings.size; ++i) {
        FbleBinding* binding = e->bindings.xs + i;
        FbleFreeKind(binding->kind);
        FbleFreeExpr(binding->type);
        FbleFreeName(binding->name);
        FbleFreeExpr(binding->expr);
      }
      FbleFree(e->bindings.xs);
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_POLY_VALUE_EXPR: {
      FblePolyValueExpr* e = (FblePolyValueExpr*)expr;
      FbleFreeKind(e->arg.kind);
      FbleFreeName(e->arg.name);
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* e = (FblePolyApplyExpr*)expr;
      FbleFreeExpr(e->poly);
      FbleFreeExpr(e->arg);
      FbleFree(expr);
      return;
    }

    case FBLE_ABSTRACT_EXPR: {
      FbleAbstractExpr* e = (FbleAbstractExpr*)expr;
      FbleFreeName(e->name);
      FbleFreeExpr(e->body);
      FbleFree(expr);
      return;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* e = (FbleListExpr*)expr;
      FbleFreeExpr(e->func);
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(e->args.xs[i]);
      }
      FbleFree(e->args.xs);
      FbleFree(expr);
      return;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* e = (FbleLiteralExpr*)expr;
      FbleFreeExpr(e->func);
      FbleFreeLoc(e->word_loc);
      FbleFree((char*)e->word);
      FbleFree(expr);
      return;
    }

    case FBLE_MODULE_PATH_EXPR: {
      FbleModulePathExpr* e = (FbleModulePathExpr*)expr;
      FbleFreeModulePath(e->path);
      FbleFree(expr);
      return;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleApplyExpr* e = (FbleApplyExpr*)expr;
      FbleFreeExpr(e->misc);
      for (size_t i = 0; i < e->args.size; ++i) {
        FbleFreeExpr(e->args.xs[i]);
      }
      FbleFree(e->args.xs);
      FbleFree(expr);
      return;
    }
  }

  UNREACHABLE("should never get here");
}
