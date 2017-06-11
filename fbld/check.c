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

static bool TypesMatch(FbldLoc* loc, FbldDecl* expected, FbldDecl* actual);
static void ResolveModule(FbldMDefn* mctx, FbldQualifiedName* entity);
static FbldDecl* LookupDecl(FbldMDefnV* env, FbldMDefn* mctx, FbldQualifiedName* entity);
static FbldDecl* CheckExpr(FbldMDeclV* mdeclv, FbldMDefn* mdefn, Vars* vars, FbldExpr* expr);

// TypesMatch --
//   Check whether the given types match.
//
// Inputs:
//   loc - location to use for error reporting.
//   expected - the expected type.
//   actual - the actual type.
//
// Results:
//   true if the expected and actual types match, false otherwise.
//
// Side effects:
//   Prints an error message to stderr i case the types don't match.
//   If either type is null, it is assumed an error has already been printed,
//   in which case no additional error message will be reported.
static bool TypesMatch(FbldLoc* loc, FbldDecl* expected, FbldDecl* actual)
{
  if (expected == NULL || actual == NULL) {
    return false;
  }

  if (expected != actual) {
    FbldReportError("Expected type %s, but found type %s\n", loc, expected->name->name, actual->name->name);
    return false;
  }
  return true;
}

// ResolveModule --
//   Resolve the module part of the entity if needed.
//
// Inputs:
//   mctx - the module in which the entity reference occurs.
//   entity - the entity reference, possibly with NULL module.
//
// Results:
//   None.
//
// Side effects:
//   Sets entity->module->name to the properly resolved module if it is NULL.
static void ResolveModule(FbldMDefn* mctx, FbldQualifiedName* entity)
{
  if (entity->module->name == NULL) {
    // Tentatively set the current module as the module to use.
    entity->module->name = mctx->name->name;

    // Override the module to use if the entity has been imported.
    for (size_t i = 0; i < mctx->declv->size; ++i) {
      if (mctx->declv->xs[i]->tag == FBLD_IMPORT_DECL) {
        FbldImportDecl* decl = (FbldImportDecl*)mctx->declv->xs[i];
        for (size_t j = 0; j < decl->namev->size; ++j) {
          if (strcmp(entity->name->name, decl->namev->xs[j]->name) == 0) {
            entity->module->name = decl->_base.name->name;
            break;
          }
        }
      }
    }
  }
}

// LookupDecl --
//   Look up the qualified declaration with the given name in the given program.
//
// Inputs:
//   env - The collection of modules to look up the declaration in.
//   mctx - The current module used for resolving references.
//   entity - The name of the entity to look up.
//
// Returns:
//   The declaration with the given name, or NULL if no such declaration
//   could be found.
//
// Side effects:
//   Resolves module references as needed.
//   Prints a message to stderr if the declaration cannot be found.
static FbldDecl* LookupDecl(FbldMDefnV* env, FbldMDefn* mctx, FbldQualifiedName* entity)
{
  ResolveModule(mctx, entity);
  FbldDecl* decl = FbldLookupDecl(env, mctx, entity);
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
      for (size_t i = 0; vars != NULL; ++i) {
        if (strcmp(vars->name, var_expr->var->name) == 0) {
          var_expr->id = i;
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
        FbldDecl* arg_type_actual = CheckExpr(mdeclv, mdefn, vars, app_expr->argv->xs[i]);
        if (!TypesMatch(app_expr->argv->xs[i]->loc, arg_type_expected, arg_type_actual)) {
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
        if (strcmp(union_expr->field.name->name, union_decl->fieldv->xs[i]->name->name) == 0) {
          union_expr->field.id = i;
          arg_type_expected = LookupDecl(mdeclv, mdefn, union_decl->fieldv->xs[i]->type);
          if (arg_type_expected == NULL) {
            return NULL;
          }
          break;
        }
      }
      if (arg_type_expected == NULL) {
        FbldReportError("%s is not a field of type %s\n",
            union_expr->field.name->loc, union_expr->field.name->name, union_expr->type->name->name);
        return NULL;
      }

      FbldDecl* arg_type_actual = CheckExpr(mdeclv, mdefn, vars, union_expr->arg);
      if (!TypesMatch(union_expr->arg->loc, arg_type_expected, arg_type_actual)) {
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
          if (strcmp(access_expr->field.name->name, concrete_type->fieldv->xs[i]->name->name) == 0) {
            access_expr->field.id = i;
            return LookupDecl(mdeclv, mdefn, concrete_type->fieldv->xs[i]->type);
          }
        }
        FbldReportError("%s is not a field of type %s\n", access_expr->field.name->loc, access_expr->field.name->name, type->name->name);
        return NULL;
      }
      FbldReportError("can only access fields of struct or union type objects\n", access_expr->field.name->loc);
      return NULL;
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* cond_expr = (FbldCondExpr*)expr;
      FbldConcreteTypeDecl* select_type = (FbldConcreteTypeDecl*)CheckExpr(mdeclv, mdefn, vars, cond_expr->select);
      if (select_type == NULL) {
        return NULL;
      }

      if (select_type->_base.tag != FBLD_UNION_DECL) {
        FbldReportError("condition must be of union type, but found type %s\n", cond_expr->select->loc, select_type->_base.name->name);
        FbldReportError("(%s defined here)\n", select_type->_base.name->loc, select_type->_base.name->name);
        return NULL;
      }

      if (select_type->fieldv->size != cond_expr->argv->size) {
        FbldReportError("Expected %d arguments, but %d were provided.\n", cond_expr->_base.loc, select_type->fieldv->size, cond_expr->argv->size);
        return NULL;
      }

      assert(cond_expr->argv->size > 0);
      FbldDecl* result_type = NULL;
      for (size_t i = 0; i < cond_expr->argv->size; ++i) {
        FbldDecl* arg_type = CheckExpr(mdeclv, mdefn, vars, cond_expr->argv->xs[i]);
        if (i == 0) {
          result_type = arg_type;
        }

        if (!TypesMatch(cond_expr->argv->xs[i]->loc, result_type, arg_type)) {
          return NULL;
        }
      }
      return result_type;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* let_expr = (FbldLetExpr*)expr;

      // TODO: Check that the variable type matches what was declared.
      if (LookupDecl(mdeclv, mdefn, let_expr->type) == NULL) {
        return NULL;
      }

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
      case FBLD_STRUCT_DECL: {
        // TODO: Check that fields have unique names.
        FbldConcreteTypeDecl* type_decl = (FbldConcreteTypeDecl*)decl;
        for (size_t i = 0; i < type_decl->fieldv->size; ++i) {
          if (LookupDecl(mdeclv, mdefn, type_decl->fieldv->xs[i]->type) == NULL) {
            return NULL;
          }
        }
        break;
      }

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
        FbldDecl* return_type_actual = CheckExpr(mdeclv, mdefn, vars, func_decl->body);
        if (!TypesMatch(func_decl->body->loc, return_type_expected, return_type_actual)) {
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
