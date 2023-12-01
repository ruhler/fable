/**
 * @file unused.c
 * Checks for unused variables in fble code.
 */

#include "unused.h"

#include "unreachable.h"    // for FbleUnreachable

typedef struct Vars {
  FbleName name;      // Name of this variable.
  bool used;          // Whether this varible is used.
  struct Vars* next;  // The rest of the variables in scope.
} Vars;

static void Expr(FbleExpr* expr, Vars* vars);

// Expr --
//   Traverses the given expression.
//
// Args:
//   expr - the expression to traverse. May be NULL.
//   vars - vars in scope.
//
// Side effects
//   * Prints warnings for unused variables.
//   * Marks used variables as used.
static void Expr(FbleExpr* expr, Vars* vars)
{
  if (!expr) {
    return;
  }

  switch (expr->tag) {
    case FBLE_DATA_TYPE_EXPR: {
      FbleDataTypeExpr* e = (FbleDataTypeExpr*)expr;
      for (size_t i = 0; i < e->fields.size; ++i) {
        Expr(e->fields.xs[i].type, vars);
      }
      return;
    }

    case FBLE_FUNC_TYPE_EXPR: {
      FbleFuncTypeExpr* e = (FbleFuncTypeExpr*)expr;
      for (size_t i = 0; i < e->args.size; ++i) {
        Expr(e->args.xs[i], vars);
      }
      Expr(e->rtype, vars);
      return;
    }

    case FBLE_PACKAGE_TYPE_EXPR: return;

    case FBLE_TYPEOF_EXPR:
    {
      FbleTypeofExpr* e = (FbleTypeofExpr*)expr;
      Expr(e->expr, vars);
      return;
    }

    case FBLE_VAR_EXPR: {
      FbleVarExpr* var_expr = (FbleVarExpr*)expr;
      for (Vars* v = vars; v != NULL; v = v->next) {
        if (FbleNamesEqual(var_expr->var, v->name)) {
          v->used = true;
          break;
        }
      }
      return;
    }

    case FBLE_LET_EXPR: {
      FbleLetExpr* let_expr = (FbleLetExpr*)expr;

      size_t varc = let_expr->bindings.size;
      Vars nvars[varc];
      bool processed[varc];
      for (size_t i = 0; i < varc; ++i) {
        Expr(let_expr->bindings.xs[i].type, vars);
        nvars[i].name = let_expr->bindings.xs[i].name;
        nvars[i].used = let_expr->bindings.xs[i].name.name->str[0] == '_';
        nvars[i].next = (i == 0) ? vars : (nvars + i - 1);
        processed[i] = false;
      }

      Expr(let_expr->body, nvars + varc - 1);

      size_t v = 0;
      while (v < varc) {
        if (nvars[v].used && !processed[v]) {
          processed[v] = true;
          Expr(let_expr->bindings.xs[v].expr, nvars + varc - 1);
          v = 0;
          continue;
        }
        v++;
      }

      for (size_t i = 0; i < varc; ++i) {
        if (!nvars[i].used) {
          FbleReportWarning("variable '", nvars[i].name.loc);
          FblePrintName(stderr, nvars[i].name);
          fprintf(stderr, "' defined but not used\n");
        }
      }
      return;
    }

    case FBLE_STRUCT_VALUE_IMPLICIT_TYPE_EXPR: {
      FbleStructValueImplicitTypeExpr* struct_expr = (FbleStructValueImplicitTypeExpr*)expr;
      for (size_t i = 0; i < struct_expr->args.size; ++i) {
        Expr(struct_expr->args.xs[i].expr, vars);
      }
      return;
    }

    case FBLE_STRUCT_COPY_EXPR: {
      FbleStructCopyExpr* struct_expr = (FbleStructCopyExpr*)expr;
      Expr(struct_expr->src, vars);
      for (size_t i = 0; i < struct_expr->args.size; ++i) {
        Expr(struct_expr->args.xs[i].expr, vars);
      }
      return;
    }

    case FBLE_UNION_VALUE_EXPR: {
      FbleUnionValueExpr* union_value_expr = (FbleUnionValueExpr*)expr;
      Expr(union_value_expr->type, vars);
      Expr(union_value_expr->arg, vars);
      return;
    }

    case FBLE_UNION_SELECT_EXPR: {
      FbleUnionSelectExpr* select_expr = (FbleUnionSelectExpr*)expr;
      Expr(select_expr->condition, vars);
      for (size_t i = 0; i < select_expr->choices.size; ++i) {
        Expr(select_expr->choices.xs[i].expr, vars);
      }
      Expr(select_expr->default_, vars);
      return;
    }

    case FBLE_FUNC_VALUE_EXPR: {
      FbleFuncValueExpr* func_value_expr = (FbleFuncValueExpr*)expr;

      size_t argc = func_value_expr->args.size;
      Vars nvars[argc];
      for (size_t i = 0; i < argc; ++i) {
        Expr(func_value_expr->args.xs[i].type, vars);
        nvars[i].name = func_value_expr->args.xs[i].name;
        nvars[i].used = false;
        nvars[i].next = (i == 0) ? vars : (nvars + i - 1);
      }
      Expr(func_value_expr->body, nvars + argc - 1);
      return;
    }

    case FBLE_POLY_VALUE_EXPR: {
      FblePolyValueExpr* poly = (FblePolyValueExpr*)expr;

      Vars nvars = {
        .name = poly->arg.name,
        .used = false,
        .next = vars
      };
      Expr(poly->body, &nvars);
      return;
    }

    case FBLE_POLY_APPLY_EXPR: {
      FblePolyApplyExpr* apply = (FblePolyApplyExpr*)expr;
      Expr(apply->poly, vars);
      Expr(apply->arg, vars);
      return;
    }

    case FBLE_LIST_EXPR: {
      FbleListExpr* list_expr = (FbleListExpr*)expr;
      Expr(list_expr->func, vars);
      for (size_t i = 0; i < list_expr->args.size; ++i) {
        Expr(list_expr->args.xs[i], vars);
      }
      return;
    }

    case FBLE_LITERAL_EXPR: {
      FbleLiteralExpr* literal_expr = (FbleLiteralExpr*)expr;
      Expr(literal_expr->func, vars);
      return;
    }

    case FBLE_ABSTRACT_CAST_EXPR: {
      FbleAbstractCastExpr* cast_expr = (FbleAbstractCastExpr*)expr;
      Expr(cast_expr->package, vars);
      Expr(cast_expr->target, vars);
      Expr(cast_expr->value, vars);
      return;
    }

    case FBLE_ABSTRACT_ACCESS_EXPR: {
      FbleAbstractAccessExpr* access_expr = (FbleAbstractAccessExpr*)expr;
      Expr(access_expr->value, vars);
      return;
    }

    case FBLE_MODULE_PATH_EXPR: return;

    case FBLE_DATA_ACCESS_EXPR: {
      FbleDataAccessExpr* access_expr = (FbleDataAccessExpr*)expr;
      Expr(access_expr->object, vars);
      return;
    }

    case FBLE_MISC_APPLY_EXPR: {
      FbleApplyExpr* apply_expr = (FbleApplyExpr*)expr;
      Expr(apply_expr->misc, vars);
      for (size_t i = 0; i < apply_expr->args.size; ++i) {
        Expr(apply_expr->args.xs[i], vars);
      }
      return;
    }
  }

  FbleUnreachable("should already have returned");
}

// See documentation in unused.h
void FbleWarnAboutUnusedVars(FbleExpr* expr)
{
  Expr(expr, NULL);
}
