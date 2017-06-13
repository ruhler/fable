// check.c --
//   This file implements routines for checking fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

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
static bool CheckModule(FbldMDeclV* mdeclv, FbldModule* module);

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
          if (FbldNamesEqual(entity->name->name, decl->namev->xs[j]->name)) {
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
        if (FbldNamesEqual(vars->name, var_expr->var->name)) {
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
        if (FbldNamesEqual(union_expr->field.name->name, union_decl->fieldv->xs[i]->name->name)) {
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
          if (FbldNamesEqual(access_expr->field.name->name, concrete_type->fieldv->xs[i]->name->name)) {
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

      FbldDecl* var_type_expected = LookupDecl(mdeclv, mdefn, let_expr->type);
      if (var_type_expected == NULL) {
        return NULL;
      }

      if (var_type_expected->tag != FBLD_STRUCT_DECL
          && var_type_expected->tag != FBLD_UNION_DECL
          && var_type_expected->tag != FBLD_ABSTRACT_TYPE_DECL) {
        FbldReportError("%s does not refer to a type.\n", let_expr->type->name->loc, let_expr->type->name->name);
        return NULL;
      }

      FbldDecl* var_type_actual = CheckExpr(mdeclv, mdefn, vars, let_expr->def);
      if (!TypesMatch(let_expr->def->loc, var_type_expected, var_type_actual)) {
        return NULL;
      }

      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FbldNamesEqual(curr->name, let_expr->var->name)) {
          FbldReportError("Redefinition of variable '%s'\n",
              let_expr->var->loc, let_expr->var->name);
          return NULL;
        }
      }

      Vars nvars = {
        .name = let_expr->var->name,
        .type = var_type_expected,
        .next = vars
      };
      return CheckExpr(mdeclv, mdefn, &nvars, let_expr->body);
    }

    default:
      assert(false && "Invalid expr tag");
      return NULL;
  }
}

// CheckModule --
//   Check that the given module is well formed and well typed. Works for both
//   module declarations and module definitions.
//
//   Does not check whether the module declaration and module definitions for
//   a given module are consistent.
//
// Inputs:
//   mdeclv - Already loaded and checked module declarations required by the
//            module declaration.
//   module - The module to check.
//
// Results:
//   true if the module is well formed and well typed in the
//   environment of the given module declarations, false otherwise.
//
// Side effects:
//   If the module is not well formed, an error message is printed to stderr
//   describing the problem with the module.
static bool CheckModule(FbldMDeclV* mdeclv, FbldModule* module)
{
  for (size_t d = 0; d < module->declv->size; ++d) {
    FbldDecl* decl = module->declv->xs[d];
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
          if (LookupDecl(mdeclv, module, type_decl->fieldv->xs[i]->type) == NULL) {
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
          nvars[i].type = LookupDecl(mdeclv, module, func_decl->argv->xs[i]->type);
          if (nvars[i].type == NULL) {
            return false;
          }
          nvars[i].next = vars;
          vars = nvars + i;
        }

        FbldDecl* return_type_expected = LookupDecl(mdeclv, module, func_decl->return_type);
        if (return_type_expected == NULL) {
          return false;
        }

        if (func_decl->body != NULL) {
          FbldDecl* return_type_actual = CheckExpr(mdeclv, module, vars, func_decl->body);
          if (!TypesMatch(func_decl->body->loc, return_type_expected, return_type_actual)) {
            return false;
          }
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

// FbldCheckMDecl -- see documentation in fbld.h
bool FbldCheckMDecl(FbldMDeclV* mdeclv, FbldMDecl* mdecl)
{
  return CheckModule(mdeclv, mdecl);
}

// FbldCheckMDefn -- see documentation in fbld.h
bool FbldCheckMDefn(FbldMDeclV* mdeclv, FbldMDefn* mdefn)
{
  if (!CheckModule(mdeclv, mdefn)) {
    return false;
  }

  // Verify the mdefn has everything it should according to the mdecl.
  FbldMDecl* mdecl = NULL;
  for (size_t i = 0; i < mdeclv->size; ++i) {
    if (FbldNamesEqual(mdefn->name->name, mdeclv->xs[i]->name->name)) {
      mdecl = mdeclv->xs[i];
      break;
    }
  }
  assert(mdecl != NULL && "mdecl for mdefn not loaded");

  FbldMDeclV empty_declv = {
    .size = 0,
    .xs = NULL
  };
  for (size_t d = 0; d < mdecl->declv->size; ++d) {
    FbldDecl* from_mdecl = mdecl->declv->xs[d];
    FbldQualifiedName name = {
      .name = from_mdecl->name,
      .module = mdecl->name
    };
    FbldDecl* from_mdefn = FbldLookupDecl(&empty_declv, mdefn, &name);

    // Verify the mdecl declaration has a matching declaration in the mdefn.
    if (from_mdecl->tag != FBLD_IMPORT_DECL && from_mdefn == NULL) {
      FbldReportError("%s not defined in %s.mdefn.\n",
          from_mdecl->name->loc, from_mdecl->name->name, mdecl->name->name);
      return false;
    }

    // Check that the matching declarations actually do match.
    switch (from_mdecl->tag) {
      case FBLD_IMPORT_DECL: break;

      case FBLD_ABSTRACT_TYPE_DECL: {
        if (from_mdefn->tag != FBLD_STRUCT_DECL && from_mdefn->tag != FBLD_UNION_DECL) {
          FbldReportError("Expected struct or union declaration.\n", from_mdefn->name->loc);
          FbldReportError("Based on abstract type declaration here.\n", from_mdecl->name->loc);
          return false;
        }
        break;
      }

      case FBLD_UNION_DECL:
      case FBLD_STRUCT_DECL: {
        if (from_mdecl->tag == FBLD_UNION_DECL && from_mdefn->tag != FBLD_UNION_DECL) {
          FbldReportError("Expected union declaration.\n", from_mdefn->name->loc);
          FbldReportError("Based on union declaration here.\n", from_mdecl->name->loc);
          return false;
        }

        if (from_mdecl->tag == FBLD_STRUCT_DECL && from_mdefn->tag != FBLD_STRUCT_DECL) {
          FbldReportError("Expected struct declaration.\n", from_mdefn->name->loc);
          FbldReportError("Based on struct declaration here.\n", from_mdecl->name->loc);
          return false;
        }

        FbldConcreteTypeDecl* type_mdecl = (FbldConcreteTypeDecl*)from_mdecl;
        FbldConcreteTypeDecl* type_mdefn = (FbldConcreteTypeDecl*)from_mdefn;

        if (type_mdecl->fieldv->size != type_mdefn->fieldv->size) {
          FbldReportError("type declaration doesn't match.\n", from_mdefn->name->loc);
          FbldReportError("what was declared here.\n", from_mdecl->name->loc);
          return false;
        }

        for (size_t i = 0; i < type_mdecl->fieldv->size; ++i) {
          FbldTypedName* field_mdecl = type_mdecl->fieldv->xs[i];
          FbldTypedName* field_mdefn = type_mdefn->fieldv->xs[i];
          if (!FbldNamesEqual(field_mdecl->type->name->name, field_mdefn->type->name->name)) {
            FbldReportError("type declaration doesn't match.\n", field_mdecl->type->name->loc);
            FbldReportError("what was declared here.\n", field_mdefn->type->name->loc);
            return false;
          }
          if (!FbldNamesEqual(field_mdecl->type->module->name, field_mdefn->type->module->name)) {
            FbldReportError("type declaration doesn't match.\n", field_mdecl->type->module->loc);
            FbldReportError("what was declared here.\n", field_mdefn->type->module->loc);
            return false;
          }
          if (!FbldNamesEqual(field_mdecl->name->name, field_mdefn->name->name)) {
            FbldReportError("type declaration doesn't match.\n", field_mdecl->name->loc);
            FbldReportError("what was declared here.\n", field_mdefn->name->loc);
            return false;
          }
        }
        break;
      }

      case FBLD_FUNC_DECL: {
        FbldFuncDecl* func_mdecl = (FbldFuncDecl*)from_mdecl;
        FbldFuncDecl* func_mdefn = (FbldFuncDecl*)from_mdefn;

        if (func_mdefn->_base.tag != FBLD_FUNC_DECL) {
          FbldReportError("expected func declaration.\n", from_mdefn->name->loc);
          FbldReportError("based on union declaration here.\n", from_mdecl->name->loc);
          return false;
        }


        if (func_mdecl->argv->size != func_mdefn->argv->size) {
          FbldReportError("type declaration doesn't match.\n", from_mdefn->name->loc);
          FbldReportError("what was declared here.\n", from_mdecl->name->loc);
          return false;
        }

        for (size_t i = 0; i < func_mdecl->argv->size; ++i) {
          FbldTypedName* arg_mdecl = func_mdecl->argv->xs[i];
          FbldTypedName* arg_mdefn = func_mdefn->argv->xs[i];
          if (!FbldNamesEqual(arg_mdecl->type->name->name, arg_mdefn->type->name->name)) {
            FbldReportError("func declaration doesn't match.\n", arg_mdefn->type->name->loc);
            FbldReportError("what was declared here.\n", arg_mdecl->type->name->loc);
            return false;
          }
          if (!FbldNamesEqual(arg_mdecl->type->module->name, arg_mdefn->type->module->name)) {
            FbldReportError("func declaration doesn't match.\n", arg_mdefn->type->module->loc);
            FbldReportError("what was declared here.\n", arg_mdecl->type->module->loc);
            return false;
          }
          if (!FbldNamesEqual(arg_mdecl->name->name, arg_mdefn->name->name)) {
            FbldReportError("func declaration doesn't match.\n", arg_mdefn->name->loc);
            FbldReportError("what was declared here.\n", arg_mdecl->name->loc);
            return false;
          }
        }

        FbldQualifiedName* ret_mdecl = func_mdecl->return_type;
        FbldQualifiedName* ret_mdefn = func_mdefn->return_type;
        if (!FbldNamesEqual(ret_mdecl->name->name, ret_mdefn->name->name)) {
          FbldReportError("return type doesn't match.\n", ret_mdefn->name->loc);
          FbldReportError("what was declared here.\n", ret_mdecl->name->loc);
          return false;
        }
        if (!FbldNamesEqual(ret_mdecl->module->name, ret_mdefn->module->name)) {
          FbldReportError("return type doesn't match.\n", ret_mdefn->module->loc);
          FbldReportError("what was declared here.\n", ret_mdecl->module->loc);
          return false;
        }
        break;
      }

      case FBLD_PROC_DECL:
        // TODO: Check that the proc matches as it should.
        break;

      default:
        assert(false && "Invalid decl type.");
        return false;
    }
  }
  return true;
}
