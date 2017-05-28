// check.c --
//   This file implements routines for checking fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL
#include <string.h>   // for strcmp

#include "fbld.h"

// Vars --
//   List of variables in scope.
typedef struct Vars {
  FbldName name;
  FbldDecl* type;
  struct Vars* next;
} Vars;

static FbldDecl* LookupDecl(FbldMDefnV* env, FbldMDefn* mctx, FbldQualifiedName* entity);
static FbldDecl* CheckExpr(FbldMDeclV* mdeclv, FbldMDefn* mdefn, Vars* vars, FbldExpr* expr);

// LookupDecl --
//   Look up the qualified declaration with the given name in the given program.
//
// Inputs:
//   env - The collection of modules to look up the declaration in.
//   mctx - Context to use for module resultion.
//   entity - The name of the entity to look up.
//
// Returns:
//   The declaration with the given name, or NULL if no such declaration
//   could be found.
//
// Side effects:
//   Prints a message to stderr if the declaration cannot be found.
static FbldDecl* LookupDecl(FbldMDefnV* env, FbldMDefn* mctx, FbldQualifiedName* entity)
{
  FbldDecl* decl = FbldLookupQDecl(env, mctx, entity);
  if (decl == NULL) {
    FbldReportError("%s not declared\n", entity->name->loc, entity->name->name);
  }
  return decl;
}


// CheckExpr --
//   Check that the given expression is well formed.
//
// Inputs:
//   mdeclv - The program environment.
//   mdefn - The current module.
//   vars - The list of variables currently in scope.
//   expr - The expression to check.
//
// Results:
//   The declaration for the type of the expression, or NULL if the expression
//   is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression is not well typed.
static FbldDecl* CheckExpr(FbldMDeclV* mdeclv, FbldMDefn* mdefn, Vars* vars, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
      FbldVarExpr* var_expr = (FbldVarExpr*)expr;
      while (vars != NULL) {
        if (strcmp(vars->name, var_expr->var->name) == 0) {
          return vars->type;
        }
        vars = vars->next;
      }
      FbldReportError("variable %s not in scope\n", var_expr->var->loc, var_expr->var->name);
      return NULL;
    }

    case FBLC_APP_EXPR: {
      FbldAppExpr* app_expr = (FbldAppExpr*)expr;
      FbldDecl* func = LookupDecl(mdeclv, mdefn, app_expr->func);
      if (func == NULL) {
        return NULL;
      }

      FbldDecl* return_type = NULL;
      FbldTypedNameV* argv = NULL;
      switch (func->tag) {
        case FBLD_STRUCT_DECL: {
          FbldStructDecl* struct_decl = (FbldStructDecl*)func;
          return_type = func;
          argv = struct_decl->fieldv;
          break;
        }

        case FBLD_FUNC_DECL: {
          FbldFuncDecl* func_decl = (FbldFuncDecl*)func;
          return_type = LookupDecl(mdeclv, mdefn, func_decl->return_type);
          if (return_type == NULL) {
            return NULL;
          }
          argv = func_decl->argv;
          break;
        }

        default:
          FbldReportError("%s is not a struct or func\n", app_expr->func->name->loc, app_expr->func->name->name);
          return NULL;
      }

      if (argv->size != app_expr->argv->size) {
        FbldReportError("expected %i args to %s, but only %i given\n",
            app_expr->_base.loc, argv->size, app_expr->func->name->name, app_expr->argv->size);
        return NULL;
      }

      for (size_t i = 0; i < argv->size; ++i) {
        FbldDecl* arg_type_expected = LookupDecl(mdeclv, mdefn, argv->xs[i]->type);
        if (arg_type_expected == NULL) {
          return NULL;
        }

        FbldDecl* arg_type_actual = CheckExpr(mdeclv, mdefn, vars, app_expr->argv->xs[i]);
        if (arg_type_actual == NULL) {
          return NULL;
        }

        if (arg_type_expected != arg_type_actual) {
          FbldReportError("Expected type %s, but found type %s\n",
              app_expr->argv->xs[i]->loc, arg_type_expected->name->name, arg_type_actual->name->name);
          return NULL;
        }
      }
      return return_type;
    }

    case FBLC_UNION_EXPR: {
      FbldUnionExpr* union_expr = (FbldUnionExpr*)expr;
      FbldDecl* type = LookupDecl(mdeclv, mdefn, union_expr->type);
      if (type == NULL) {
        return NULL;
      }
      if (type->tag != FBLD_UNION_DECL) {
        FbldReportError("%s does not refer to a union type\n",
            union_expr->type->name->loc, union_expr->type->name->name);
        return NULL;
      }

      FbldUnionDecl* union_decl = (FbldUnionDecl*)type;
      FbldDecl* arg_type_expected = NULL;
      for (size_t i = 0; i < union_decl->fieldv->size; ++i) {
        if (strcmp(union_expr->field->name, union_decl->fieldv->xs[i]->name->name) == 0) {
          arg_type_expected = LookupDecl(mdeclv, mdefn, union_decl->fieldv->xs[i]->type);
          if (arg_type_expected == NULL) {
            return NULL;
          }
          break;
        }
      }
      if (arg_type_expected == NULL) {
        FbldReportError("%s is not a field of type %s\n",
            union_expr->field->loc, union_expr->field->name, union_expr->type->name->name);
        return NULL;
      }

      FbldDecl* arg_type_actual = CheckExpr(mdeclv, mdefn, vars, union_expr->arg);
      if (arg_type_actual == NULL) {
        return NULL;
      }

      if (arg_type_expected != arg_type_actual) {
        FbldReportError("Expected type %s, but found type %s\n",
            union_expr->arg->loc, arg_type_expected->name->name, arg_type_actual->name->name);
        return NULL;
      }
      return type;
    }

    case FBLC_ACCESS_EXPR: {
      FbldAccessExpr* access_expr = (FbldAccessExpr*)expr;
      FbldDecl* type = CheckExpr(mdeclv, mdefn, vars, access_expr->obj);
      if (type == NULL) {
        return NULL;
      }
      if (type->tag == FBLD_STRUCT_DECL || type->tag == FBLD_UNION_DECL) {
        FbldConcreteTypeDecl* concrete_type = (FbldConcreteTypeDecl*)type;
        for (size_t i = 0; i < concrete_type->fieldv->size; ++i) {
          if (strcmp(access_expr->field->name, concrete_type->fieldv->xs[i]->name->name) == 0) {
            return LookupDecl(mdeclv, mdefn, concrete_type->fieldv->xs[i]->type);
          }
        }
        FbldReportError("%s is not a field of type %s\n", access_expr->field->loc, access_expr->field->name, type->name->name);
        return NULL;
      }
      FbldReportError("can only access fields of struct or union type objects\n", access_expr->field->loc);
      return NULL;
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* cond_expr = (FbldCondExpr*)expr;
      FbldDecl* select_type = CheckExpr(mdeclv, mdefn, vars, cond_expr->select);
      if (select_type->tag != FBLD_UNION_DECL) {
        FbldReportError("condition must be of union type, but found type %s\n", cond_expr->select->loc, select_type->name->name);
        FbldReportError("(%s defined here)\n", select_type->name->loc, select_type->name->name);
        return NULL;
      }
      FbldDecl* type = NULL;
      for (size_t i = 0; i < cond_expr->argv->size; ++i) {
        type = CheckExpr(mdeclv, mdefn, vars, cond_expr->argv->xs[i]);
        // TODO: Check that all argument types match.
        if (type == NULL) {
          return NULL;
        }
      }
      return type;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* let_expr = (FbldLetExpr*)expr;

      // TODO: Check that the variable type matches what was declared.
      FbldDecl* var_type = CheckExpr(mdeclv, mdefn, vars, let_expr->def);
      if (var_type == NULL) {
        return NULL;
      }

      Vars nvars = {
        .name = let_expr->var->name,
        .type = var_type,
        .next = vars
      };
      return CheckExpr(mdeclv, mdefn, &nvars, let_expr->body);
    }

    default:
      assert(false && "Invalid expr tag");
      return NULL;
  }
}

// FbldCheckMDefn -- see documentation in fbld.h
bool FbldCheckMDefn(FbldMDeclV* mdeclv, FbldMDefn* mdefn)
{
  for (size_t d = 0; d < mdefn->declv->size; ++d) {
    FbldDecl* decl = mdefn->declv->xs[d];
    switch (decl->tag) {
      case FBLD_IMPORT_DECL:
        // TODO: Verify the module has been listed as a dependency.
        // TODO: Verify all entities imported are exported by the module.
        break;

      case FBLD_ABSTRACT_TYPE_DECL:
        // There is nothing specific to check about abstract type
        // declarations.
        break;

      case FBLD_UNION_DECL:
      case FBLD_STRUCT_DECL:
        // TODO: Check that fields have unique names.
        // TODO: Check that fields refer to valid types.
        break;

      case FBLD_FUNC_DECL: {
        FbldFuncDecl* func_decl = (FbldFuncDecl*)decl;
        // TODO: Check that arguments have unique names.
        Vars nvars[func_decl->argv->size];
        Vars* vars = NULL;
        for (size_t i = 0; i < func_decl->argv->size; ++i) {
          nvars[i].name = func_decl->argv->xs[i]->name->name;
          nvars[i].type = LookupDecl(mdeclv, mdefn, func_decl->argv->xs[i]->type);
          if (nvars[i].type == NULL) {
            return false;
          }
          nvars[i].next = vars;
          vars = nvars + i;
        }

        FbldDecl* return_type_expected = LookupDecl(mdeclv, mdefn, func_decl->return_type);
        if (return_type_expected == NULL) {
          return false;
        }

        FbldDecl* return_type_actual = CheckExpr(mdeclv, mdefn, vars, func_decl->body);
        if (return_type_actual == NULL) {
          return false;
        }

        if (return_type_expected != return_type_actual) {
          FbldReportError("Expected type %s, but found type %s\n",
              func_decl->body->loc, return_type_expected->name->name, return_type_actual->name->name);
          return false;
        }
        break;
      }

      case FBLD_PROC_DECL:
        // TODO: Check that ports have unique names.
        // TODO: Check that ports refer to valid types.
        // TODO: Check that arguments have unique names.
        // TODO: Check that args refer to valid types.
        // TODO: Check that the return type refers to a valid type.
        // TODO: Check the body of the process.
        break;

      default:
        assert(false && "Invalid decl type.");
        return false;
    }
  }
  return true;
}
