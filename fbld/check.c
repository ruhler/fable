// check.c --
//   This file implements routines for checking fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_start, va_end, vfprintf
#include <stdlib.h>   // for NULL
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fbld.h"

#define UNREACHABLE(x) assert(false && x)

// DeclList
//   A linked list of declarations.
typedef struct DeclList {
  FbldDecl* decl;
  struct DeclList* next;
} DeclList;

// Env --
//   An environment of declarations.
//
// Fields:
//   parent - The environment of the parent module. Null if this is the global
//            namespace.
//   mref - The module/interf for the current namespace. Null if this is the
//          global namespace.
//   interf - The interface declaration if the current environment is in an
//            interface. NULL otherwise.
//   prgm - The body of the current module/interf.
//   svars - Stack of declarations whose static parameters are
//           visible in the current context.
typedef struct Env {
  struct Env* parent;
  FbldQRef* mref;
  FbldInterf* interf;
  FbldProgram* prgm;
  DeclList* svars;
} Env;

// Context --
//   A global context for type checking.
//
// Fields:
//   arena - Arena to use for allocations.
//   error - Flag tracking whether any type errors have been encountered.
typedef struct {
  FblcArena* arena;
  bool error;
} Context;

// Vars --
//   List of variables in scope.
typedef struct Vars {
  FbldQRef* type;
  const char* name;
  struct Vars* next;
} Vars;

// Ports --
//   A mapping from port name to port type and polarity.
typedef struct Ports {
  FbldQRef* type;
  const char* name;
  FbldPolarity polarity;
  struct Ports* next;
} Ports;

// FailedR --
//   A static instance of FbldR that can be reused for all failed qrefs.
static FbldR FailedR = {
  .decl = NULL,
  .mref = NULL,
  .param = false,
  .interf = NULL
};

static void ReportError(const char* format, bool* error, FbldLoc* loc, ...);

static FbldR* ResolveQRef(Context* ctx, Env* env, FbldQRef* qref);
static bool CheckQRef(Context* ctx, Env* env, FbldQRef* qref);
static bool CheckEnv(Context* ctx, Env* env);
static void CheckInterf(Context* ctx, Env* env, FbldInterf* interf);
static bool CheckModule(Context* ctx, Env* env, FbldModule* module);
static bool CheckModuleHeader(Context* ctx, Env* env, FbldModule* module);
static void DefineName(Context* ctx, FbldName* name, FbldNameV* defined);
static void CheckProto(Context* ctx, Env* env, FbldDecl* decl);
static void CheckProtos(Context* ctx, Env* env);
static void CheckBodies(Context* ctx, Env* env);
static bool CheckType(Context* ctx, Env* env, FbldQRef* qref);
static Vars* CheckArgV(Context* ctx, Env* env, FbldArgV* argv, Vars* vars);
static FbldQRef* CheckExpr(Context* ctx, Env* env, Vars* vars, FbldExpr* expr);
static FbldQRef* CheckActn(Context* ctx, Env* env, Vars* vars, Ports* ports, FbldActn* actn);
static void CheckTypesMatch(FbldLoc* loc, FbldQRef* expected, FbldQRef* actual, bool* error);
static bool CheckValue(Context* ctx, Env* env, FbldValue* value);

static void CheckArgsMatch(Context* ctx, FbldQRef* src, FbldArgV* args_i, FbldArgV* args_m);
static void CheckDeclsMatch(Context* ctx, FbldQRef* src, FbldDecl* decl_i, FbldDecl* decl_m);

// ReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   error - A boolean variable to set to true.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
//   Sets 'error' to true.
static void ReportError(const char* format, bool* error, FbldLoc* loc, ...)
{
  *error = true;

  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// ResolveQRef --
//   Resolve an unresolved qualified reference.
//
// Inputs:
//   ctx - The context to check the qref in.
//   env - The current environment.
//   qref - The unresolved qref to resolve.
//
// Returns:
//   The resolution info for the qref, or NULL if it failed to resolve.
//
// Side effects:
//   Prints a message to stderr if the qref fails to resolve.
static FbldR* ResolveQRef(Context* ctx, Env* env, FbldQRef* qref)
{
  // Entity explicitly qualified?
  if (qref->mref != NULL) {
    // The entity is of the form foo@bar, and comes from a module bar in the
    // local scope. Resolve the module bar and find its interface.
    if (!CheckQRef(ctx, env, qref->mref)) {
      return NULL;
    }

    assert(qref->mref->r->decl != NULL);
    if (qref->mref->r->decl->tag != FBLD_MODULE_DECL) {
      ReportError("%s does not refer to a module\n", &ctx->error, qref->mref->name->loc, qref->mref->name->name);
      return NULL;
    }

    FbldModule* module = (FbldModule*)qref->mref->r->decl;

    // We should have already checked the module interface is correct, right?
    assert(module->iref->r != NULL);
    assert(module->iref->r->decl != NULL);
    assert(module->iref->r->decl->tag == FBLD_INTERF_DECL);
    FbldInterf* interf = (FbldInterf*)module->iref->r->decl;

    // Look for the entity declaration in the interface.
    for (size_t i = 0; i < interf->body->declv->size; ++i) {
      if (FbldNamesEqual(qref->name->name, interf->body->declv->xs[i]->name->name)) {
        FbldR* r = FBLC_ALLOC(ctx->arena, FbldR);
        r->decl = interf->body->declv->xs[i];
        r->mref = qref->mref;
        r->param = false;
        r->interf = interf;
        return r;
      }
    }

    ReportError("%s not found in interface for %s\n", &ctx->error,
        qref->name->loc, qref->name->name, qref->mref->name->name);
    return NULL;
  }

  // Entity imported?
  for (size_t i = 0; i < env->prgm->importv->size; ++i) {
    FbldImport* import = env->prgm->importv->xs[i];
    for (size_t j = 0; j < import->itemv->size; ++j) {
      if (FbldNamesEqual(qref->name->name, import->itemv->xs[j]->dest->name)) {
        FbldQRefV paramv = { .size = 0, .xs = NULL };
        FbldQRef imported_qref = {
          .name = import->itemv->xs[j]->source,
          .paramv = &paramv,
          .mref = import->mref,
          .r = NULL,
        };

        Env* import_env = import->mref == NULL ? env->parent : env;
        return ResolveQRef(ctx, import_env, &imported_qref);
      } 
    }
  }

  // Entity defined locally?
  for (size_t i = 0; i < env->prgm->declv->size; ++i) {
    if (FbldNamesEqual(qref->name->name, env->prgm->declv->xs[i]->name->name)) {
      FbldR* r = FBLC_ALLOC(ctx->arena, FbldR);
      r->decl = env->prgm->declv->xs[i];
      r->mref = env->mref;
      r->param = env->interf != NULL;
      r->interf = env->interf;
      return r;
    }
  }

  // Check whether the name refers to a static parameter in scope.
  for (DeclList* dl = env->svars; dl != NULL; dl = dl->next) {
    for (size_t i = 0; i < dl->decl->paramv->size; ++i) {
      if (FbldNamesEqual(qref->name->name, dl->decl->paramv->xs[i]->name->name)) {
        FbldR* r = FBLC_ALLOC(ctx->arena, FbldR);
        r->decl = dl->decl;
        r->mref = env->mref;
        r->param = true;
        r->interf = NULL;
        return r;
      }
    }
  }

  ReportError("%s not defined\n", &ctx->error, qref->name->loc, qref->name->name);
  return NULL;
}

// CheckQRef --
//   Check that the given qref is well formed.
//
// Inputs:
//   ctx - The context to check the qref in.
//   qref - The qref to check.
//
// Returns:
//   true if the qref is well formed, false otherwise.
//
// Side effects:
//   Loads and checks top-level module declarations and interfaces as needed
//   to check the qref.
//   Updates qref.r.* fields based on the result of qref resolution.
//   Prints a message to stderr if the qref is not well formed and has not
//   already been reported as being bad.
static bool CheckQRef(Context* ctx, Env* env, FbldQRef* qref)
{
  if (qref->r != NULL) {
    return qref->r->decl != NULL;
  }

  // By default assume the qref fails to resolve. We will overwrite this with
  // something more meaningful once we have successfully resolved the
  // reference and confirmed the qref is well formed.
  qref->r = &FailedR;

  FbldR* r = ResolveQRef(ctx, env, qref);
  if (r == NULL) {
    return false;
  }

  // Check the parameters we have so far. Not all parameters need be supplied
  // at this point.
  if (qref->paramv->size > r->decl->paramv->size) {
    ReportError("Too many static arguments to %s\n",
        &ctx->error, qref->name->loc, qref->name->name);
    // TODO: Free r
    return false;
  }

  for (size_t i = 0; i < qref->paramv->size; ++i) {
    // TODO: Check that the qref's kind matches the parameter declarations
    // kind.
    if (!CheckQRef(ctx, env, qref->paramv->xs[i])) {
      // TODO: Free r
      return false;
    }
  }
  
  qref->r = r;
  return true;
}

// CheckEnv --
//   Check that the entities from the given environment are well formed.
//
// Inputs:
//   ctx - The context to check the declaration in.
//   env - The environment to check.
//
// Result:
//   true if the environment is well formed, false otherwise.
//
// Side effects:
//   Resolves qrefs.
//   Prints a message to stderr if the environment is not well formed.
static bool CheckEnv(Context* ctx, Env* env)
{
  CheckProtos(ctx, env);

  // Bail out here if the prototypes failed to check properly, because
  // CheckBodies assumes the prototypes are well formed.
  if (!ctx->error) {
    CheckBodies(ctx, env);
  }
  return !ctx->error;
}

// CheckInterf --
//   Check that the given interf declaration is well formed.
//
// Inputs:
//   ctx - The context to check the declaration in.
//   interf - The interface declaration to check.
//
// Result:
//   none.
//
// Side effects:
//   Resolves qrefs.
//   Prints a message to stderr if the interface declaration is not well
//   formed.
static void CheckInterf(Context* ctx, Env* env, FbldInterf* interf)
{
  FbldR* r = FBLC_ALLOC(ctx->arena, FbldR);
  r->decl = &interf->_base;
  r->mref = env->mref;
  r->param = env->interf != NULL;
  r->interf = env->interf;

  FbldQRef* mref = FBLC_ALLOC(ctx->arena, FbldQRef);
  mref->name = interf->_base.name;
  mref->paramv = FBLC_ALLOC(ctx->arena, FbldQRefV);
  FblcVectorInit(ctx->arena, *mref->paramv);
  mref->mref = env->mref;
  mref->r = r;

  DeclList dl = {
    .decl = &interf->_base,
    .next = NULL,
  };

  Env interf_env = {
    .parent = env,
    .mref = mref,
    .interf = interf,
    .prgm = interf->body,
    .svars = &dl,
  };
  CheckProtos(ctx, &interf_env);
}

// CheckModule --
//   Check that the given module definition is well formed.
//
// Inputs:
//   ctx - The context to check the declaration in.
//   module - The module definition to check.
//
// Result:
//   true if the module is well formed, false otherwise.
//
// Side effects:
//   Loads and checks top-level module declarations and interfaces as needed
//   to check the interface declaration.
//   Resolves qrefs.
//   Prints a message to stderr if the module definition is not well
//   formed.
static bool CheckModule(Context* ctx, Env* env, FbldModule* module)
{
  if (!CheckModuleHeader(ctx, env, module)) {
    return false;
  }

  FbldR* r = FBLC_ALLOC(ctx->arena, FbldR);
  r->decl = &module->_base;
  r->mref = env->mref;
  r->param = false;
  r->interf = NULL;

  FbldQRef* mref = FBLC_ALLOC(ctx->arena, FbldQRef);
  mref->name = module->_base.name;
  mref->paramv = FBLC_ALLOC(ctx->arena, FbldQRefV);
  FblcVectorInit(ctx->arena, *mref->paramv);
  mref->mref = env->mref;
  mref->r = r;

  DeclList dl = {
    .decl = &module->_base,
    .next = NULL,
  };

  Env module_env = {
    .parent = env,
    .mref = mref,
    .interf = NULL,
    .prgm = module->body,
    .svars = &dl,
  };

  CheckEnv(ctx, &module_env);

  // Bail out here if the prototypes failed to check properly, because
  // we assume from here on out that the prototypes are well formed.
  if (ctx->error) {
    return false;
  }

  // Verify the module has everything it should according to its interface.
  assert(module->iref->r->decl->tag == FBLD_INTERF_DECL);
  FbldInterf* interf = (FbldInterf*)module->iref->r->decl;

  // Set up a qref we can reuse as src for for the module declarations.
  FbldQRefV paramv = { .size = 0, .xs = NULL };
  FbldR sr = {
    .decl = NULL,   // Will be overwritten per decl.
    .mref = mref,
    .param = false,
    .interf = NULL
  };
  FbldQRef src = {
    .name = NULL,   // Will be overwritten per decl.
    .paramv = &paramv,
    .mref = mref,
    .r = &sr
  };

  for (size_t i = 0; i < interf->body->declv->size; ++i) {
    FbldDecl* decl_i = interf->body->declv->xs[i];
    for (size_t m = 0; m < module->body->declv->size; ++m) {
      FbldDecl* decl_m = module->body->declv->xs[m];
      if (FbldNamesEqual(decl_i->name->name, decl_m->name->name)) {
        sr.decl = decl_m;
        src.name = decl_m->name;
        CheckDeclsMatch(ctx, &src, decl_i, decl_m);

        // Set type_i to NULL to indicate we found the matching type.
        decl_i = NULL;
        break;
      }
    }

    if (decl_i != NULL) {
      ReportError("No implementation found for %s from the interface\n", &ctx->error, module->_base.name->loc, decl_i->name->name);
    }
  }

  return !ctx->error;
}

// CheckModuleHeader --
//   Check that the given module header is well formed.
//
// Inputs:
//   ctx - The context to check the declaration in.
//   module - The module header to check.
//
// Result:
//   true if the module header is well formed, false otherwise.
//
// Side effects:
//   Loads and checks top-level module declarations and interfaces as needed
//   to check the interface declaration.
//   Resolves qrefs.
//   Prints a message to stderr if the module definition is not well
//   formed.
static bool CheckModuleHeader(Context* ctx, Env* env, FbldModule* module)
{
  if (!CheckQRef(ctx, env, module->iref)) {
    return false;
  }

  if (module->iref->r->decl->tag != FBLD_INTERF_DECL) {
    ReportError("%s does not refer to an interface\n", &ctx->error, module->iref->name->loc, module->iref->name->name);
    return false;
  }
  return true;
}

// CheckTypesMatch --
//   Check whether the given types match.
//   TODO: Change this function to take a context as input.
//
// Inputs:
//   loc - location to use for error reporting.
//   expected - the expected type.
//   actual - the actual type.
//   error - out parameter set to true on error.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr and sets error to true if the types
//   don't match. If either type is null, it is assumed an error has already
//   been printed, in which case no additional error message will be reported.
//
// TODO: Is it acceptable for expected and/or actual to be non-NULL failed
// resolved names? Should we require they be one or the other?
static void CheckTypesMatch(FbldLoc* loc, FbldQRef* expected, FbldQRef* actual, bool* error)
{
  if (expected == NULL || actual == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  assert(expected->r != NULL);
  assert(actual->r != NULL);

  if (expected->r->decl == NULL || actual->r->decl == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  if (!FbldQRefsEqual(expected, actual)) {
    ReportError("Expected type ", error, loc);
    FbldPrintQRef(stderr, expected);
    fprintf(stderr, ", but found type ");
    FbldPrintQRef(stderr, actual);
    fprintf(stderr, "\n");
  }
}

// CheckType --
//   Check that the given qref refers to a type.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The environment for type checking.
//   qref - The qref to check.
//
// Result:
//   true if the qref resolves and refers to a type, false otherwise.
//
// Side effects:
//   Loads program modules as needed to check the type.
//   Resolves qref if necessary.
//   Sets error to true and reports errors to stderr if the entity could not
//   be resolved or does not refer to a type.
static bool CheckType(Context* ctx, Env* env, FbldQRef* qref)
{
  CheckQRef(ctx, env, qref);
  if (qref->r->decl == NULL) {
    return false;
  }

  if (qref->r->decl->tag != FBLD_TYPE_DECL) {
    ReportError("%s does not refer to a type\n", &ctx->error, qref->name->loc, qref->name->name);
    return false;
  }
  return true;
}

// CheckExpr --
//   Check that the given expression is well formed.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The environment for type checking.
//   vars - The list of variables currently in scope.
//   expr - The expression to check.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression is not well typed and
//   ctx->error is set to true.
static FbldQRef* CheckExpr(Context* ctx, Env* env, Vars* vars, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLD_VAR_EXPR: {
      FbldVarExpr* var_expr = (FbldVarExpr*)expr;
      for (size_t i = 0; vars != NULL; ++i) {
        if (FbldNamesEqual(vars->name, var_expr->var.name->name)) {
          var_expr->var.id = i;
          return vars->type;
        }
        vars = vars->next;
      }
      ReportError("variable '%s' not defined\n", &ctx->error, var_expr->var.name->loc, var_expr->var.name->name);
      return NULL;
    }

    case FBLD_APP_EXPR: {
      FbldAppExpr* app_expr = (FbldAppExpr*)expr;

      FbldQRef* arg_types[app_expr->argv->size];
      for (size_t i = 0; i < app_expr->argv->size; ++i) {
        arg_types[i] = CheckExpr(ctx, env, vars, app_expr->argv->xs[i]);
      }

      CheckQRef(ctx, env, app_expr->func);

      FbldQRef* return_type = NULL;
      FbldArgV* argv = NULL;
      if (app_expr->func->r->decl == NULL) {
        return NULL;
      }

      if (app_expr->func->r->decl->tag == FBLD_FUNC_DECL) {
        FbldFunc* func = (FbldFunc*)app_expr->func->r->decl;
        argv = func->argv;
        return_type = FbldImportQRef(ctx->arena, app_expr->func, func->return_type);
      } else if (app_expr->func->r->decl->tag == FBLD_TYPE_DECL) {
        FbldType* type = (FbldType*)app_expr->func->r->decl;
        if (type->kind != FBLD_STRUCT_KIND) {
          ReportError("Cannot do application on type %s.\n", &ctx->error, app_expr->func->name->loc, app_expr->func->name->name);
          return NULL;
        }
        argv = type->fieldv;
        return_type = app_expr->func;
      } else {
        ReportError("'%s' does not refer to a type or function.\n", &ctx->error, app_expr->func->name->loc, app_expr->func->name->name);
        return NULL;
      }

      if (argv->size == app_expr->argv->size) {
        for (size_t i = 0; i < argv->size; ++i) {
          FbldQRef* expected = FbldImportQRef(ctx->arena, app_expr->func, argv->xs[i]->type);
          CheckTypesMatch(app_expr->argv->xs[i]->loc, expected, arg_types[i], &ctx->error);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", &ctx->error, app_expr->func->name->loc, argv->size, app_expr->func->name->name, app_expr->argv->size);
      }
      return return_type;
    }

    case FBLD_ACCESS_EXPR: {
      FbldAccessExpr* access_expr = (FbldAccessExpr*)expr;
      FbldQRef* qref = CheckExpr(ctx, env, vars, access_expr->obj);
      if (qref == NULL || qref->r->decl == NULL) {
        return NULL;
      }

      assert(qref->r->decl->tag == FBLD_TYPE_DECL);
      FbldType* type = (FbldType*)qref->r->decl;
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (FbldNamesEqual(access_expr->field.name->name, type->fieldv->xs[i]->name->name)) {
          access_expr->field.id = i;
          return FbldImportQRef(ctx->arena, qref, type->fieldv->xs[i]->type);
        }
      }
      ReportError("%s is not a field of type %s\n", &ctx->error, access_expr->field.name->loc, access_expr->field.name->name, type->_base.name->name);
      return NULL;
    }

    case FBLD_UNION_EXPR: {
      FbldUnionExpr* union_expr = (FbldUnionExpr*)expr;
      FbldQRef* arg_type = CheckExpr(ctx, env, vars, union_expr->arg);
      if (!CheckType(ctx, env, union_expr->type)) {
        return NULL;
      }

      assert(union_expr->type->r->decl->tag == FBLD_TYPE_DECL);
      FbldType* type_def = (FbldType*)union_expr->type->r->decl;
      if (type_def->kind != FBLD_UNION_KIND) {
        ReportError("%s does not refer to a union type.\n", &ctx->error, union_expr->type->name->loc, union_expr->type->name->name);
        return NULL;
      }


      for (size_t i = 0; i < type_def->fieldv->size; ++i) {
        if (FbldNamesEqual(union_expr->field.name->name, type_def->fieldv->xs[i]->name->name)) {
          union_expr->field.id = i;
          FbldQRef* expected = FbldImportQRef(ctx->arena, union_expr->type, type_def->fieldv->xs[i]->type);
          CheckTypesMatch(union_expr->arg->loc, expected, arg_type, &ctx->error);
          return union_expr->type;
        }
      }
      ReportError("%s is not a field of type %s\n", &ctx->error, union_expr->field.name->loc, union_expr->field.name->name, union_expr->type->name->name);
      return NULL;
    }

    case FBLD_LET_EXPR: {
      FbldLetExpr* let_expr = (FbldLetExpr*)expr;
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FbldNamesEqual(curr->name, let_expr->var->name)) {
          ReportError("Redefinition of variable '%s'\n", &ctx->error, let_expr->var->loc, let_expr->var->name);
          return NULL;
        }
      }

      CheckType(ctx, env, let_expr->type);
      FbldQRef* def_type = CheckExpr(ctx, env, vars, let_expr->def);
      CheckTypesMatch(let_expr->def->loc, let_expr->type, def_type, &ctx->error);

      Vars nvars = {
        .type = let_expr->type,
        .name = let_expr->var->name,
        .next = vars
      };
      return CheckExpr(ctx, env, &nvars, let_expr->body);
    }

    case FBLD_COND_EXPR: {
      FbldCondExpr* cond_expr = (FbldCondExpr*)expr;
      FbldQRef* type = CheckExpr(ctx, env, vars, cond_expr->select);
      if (type != NULL) {
        assert(type->r->decl->tag == FBLD_TYPE_DECL);
        FbldType* type_def = (FbldType*)type->r->decl;
        if (type_def->kind == FBLD_UNION_KIND) {
          if (type_def->fieldv->size != cond_expr->argv->size) {
            ReportError("Expected %d arguments, but %d were provided.\n", &ctx->error, cond_expr->_base.loc, type_def->fieldv->size, cond_expr->argv->size);
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", &ctx->error, cond_expr->select->loc, type_def->_base.name->name);
        }
      }

      FbldQRef* result_type = NULL;
      assert(cond_expr->argv->size > 0);
      for (size_t i = 0; i < cond_expr->argv->size; ++i) {
        FbldQRef* arg_type = CheckExpr(ctx, env, vars, cond_expr->argv->xs[i]);
        CheckTypesMatch(cond_expr->argv->xs[i]->loc, result_type, arg_type, &ctx->error);
        result_type = (result_type == NULL) ? arg_type : result_type;
      }
      return result_type;
    }

    default: {
      UNREACHABLE("invalid fbld expression tag");
      return NULL;
    }
  }
}

// CheckActn --
//   Check that the given action is well formed.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The environment for type checking.
//   vars - The list of variables currently in scope.
//   ports - The list of ports currently in scope.
//   actn - The action to check.
//
// Results:
//   The type of the action, or NULL if the action is not well typed.
//
// Side effects:
//   Prints a message to stderr if the action is not well typed and
//   ctx->error is set to true.
static FbldQRef* CheckActn(Context* ctx, Env* env, Vars* vars, Ports* ports, FbldActn* actn)
{
  switch (actn->tag) {
    case FBLD_EVAL_ACTN: {
      FbldEvalActn* eval_actn = (FbldEvalActn*)actn;
      return CheckExpr(ctx, env, vars, eval_actn->arg);
    }

    case FBLD_GET_ACTN: {
      FbldGetActn* get_actn = (FbldGetActn*)actn;
      for (size_t i = 0; ports != NULL; ++i) {
        if (FbldNamesEqual(ports->name, get_actn->port.name->name)) {
          if (ports->polarity == FBLD_GET_POLARITY) {
            get_actn->port.id = i;
            return ports->type;
          } else {
            ReportError("Port '%s' should have get polarity, but has put polarity.\n", &ctx->error, get_actn->port.name->loc, get_actn->port.name->name);
            return NULL;
          }
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", &ctx->error, get_actn->port.name->loc, get_actn->port.name->name);
      return NULL;
    }

    case FBLD_PUT_ACTN: {
      FbldPutActn* put_actn = (FbldPutActn*)actn;
      FbldQRef* arg_type = CheckExpr(ctx, env, vars, put_actn->arg);

      for (size_t i = 0; ports != NULL; ++i) {
        if (FbldNamesEqual(ports->name, put_actn->port.name->name)) {
          if (ports->polarity == FBLD_PUT_POLARITY) {
            put_actn->port.id = i;
            CheckTypesMatch(put_actn->arg->loc, ports->type, arg_type, &ctx->error);
            return ports->type;
          } else {
            ReportError("Port '%s' should have put polarity, but has get polarity.\n", &ctx->error, put_actn->port.name->loc, put_actn->port.name->name);
            return NULL;
          }
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", &ctx->error, put_actn->port.name->loc, put_actn->port.name->name);
      return NULL;
    }

    case FBLD_COND_ACTN: {
      FbldCondActn* cond_actn = (FbldCondActn*)actn;
      FbldQRef* type = CheckExpr(ctx, env, vars, cond_actn->select);
      if (type != NULL) {
        assert(type->r->decl->tag == FBLD_TYPE_DECL);
        FbldType* type_def = (FbldType*)type->r->decl;
        if (type_def->kind == FBLD_UNION_KIND) {
          if (type_def->fieldv->size != cond_actn->argv->size) {
            ReportError("Expected %d arguments, but %d were provided.\n", &ctx->error, cond_actn->_base.loc, type_def->fieldv->size, cond_actn->argv->size);
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", &ctx->error, cond_actn->select->loc, type_def->_base.name->name);
        }
      }

      FbldQRef* result_type = NULL;
      assert(cond_actn->argv->size > 0);
      for (size_t i = 0; i < cond_actn->argv->size; ++i) {
        FbldQRef* arg_type = CheckActn(ctx, env, vars, ports, cond_actn->argv->xs[i]);
        CheckTypesMatch(cond_actn->argv->xs[i]->loc, result_type, arg_type, &ctx->error);
        result_type = (result_type == NULL) ? arg_type : result_type;
      }
      return result_type;
    }

    case FBLD_CALL_ACTN: {
      FbldCallActn* call_actn = (FbldCallActn*)actn;

      Ports* port_types[call_actn->portv->size];
      for (size_t i = 0; i < call_actn->portv->size; ++i) {
        port_types[i] = NULL;
        Ports* curr = ports;
        for (size_t id = 0; curr != NULL; ++id) {
          if (FbldNamesEqual(curr->name, call_actn->portv->xs[i].name->name)) {
            call_actn->portv->xs[i].id = id;
            port_types[i] = curr;
            break;
          }
          curr = curr->next;
        }
        if (port_types[i] == NULL) {
          ReportError("Port '%s' not defined.\n", &ctx->error, call_actn->portv->xs[i].name->loc, call_actn->portv->xs[i].name->name);
        }
      }

      FbldQRef* arg_types[call_actn->argv->size];
      for (size_t i = 0; i < call_actn->argv->size; ++i) {
        arg_types[i] = CheckExpr(ctx, env, vars, call_actn->argv->xs[i]);
      }

      if (!CheckQRef(ctx, env, call_actn->proc)) {
        return NULL;
      }

      if (call_actn->proc->r->decl->tag != FBLD_PROC_DECL) {
        ReportError("%s does not refer to a proc.\n", &ctx->error, call_actn->proc->name->loc, call_actn->proc->name->name);
        return NULL;
      }

      FbldProc* proc = (FbldProc*)call_actn->proc->r->decl;
      if (proc->portv->size == call_actn->portv->size) {
        for (size_t i = 0; i < proc->portv->size; ++i) {
          if (port_types[i] != NULL) {
            if (port_types[i]->polarity != proc->portv->xs[i].polarity) {
                ReportError("Port '%s' has wrong polarity. Expected '%s', but found '%s'.\n", &ctx->error,
                    call_actn->portv->xs[i].name->loc, call_actn->portv->xs[i].name->name,
                    proc->portv->xs[i].polarity == FBLD_PUT_POLARITY ? "put" : "get",
                    port_types[i]->polarity == FBLD_PUT_POLARITY ? "put" : "get");
            }
            FbldQRef* expected = FbldImportQRef(ctx->arena, call_actn->proc, proc->portv->xs[i].type);
            CheckTypesMatch(call_actn->portv->xs[i].name->loc, expected, port_types[i]->type, &ctx->error);
          }
        }
      } else {
        ReportError("Expected %d port arguments to %s, but %d were provided.\n", &ctx->error, call_actn->proc->name->loc, proc->portv->size, call_actn->proc->name->name, call_actn->portv->size);
      }

      if (proc->argv->size == call_actn->argv->size) {
        for (size_t i = 0; i < call_actn->argv->size; ++i) {
          FbldQRef* expected = FbldImportQRef(ctx->arena, call_actn->proc, proc->argv->xs[i]->type);
          CheckTypesMatch(call_actn->argv->xs[i]->loc, expected, arg_types[i], &ctx->error);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", &ctx->error, call_actn->proc->name->loc, proc->argv->size, call_actn->proc->name->name, call_actn->argv->size);
      }
      return FbldImportQRef(ctx->arena, call_actn->proc, proc->return_type);
    }

    case FBLD_LINK_ACTN: {
      FbldLinkActn* link_actn = (FbldLinkActn*)actn;
      CheckType(ctx, env, link_actn->type);
      for (Ports* curr = ports; curr != NULL; curr = curr->next) {
        if (FbldNamesEqual(curr->name, link_actn->get->name)) {
          ReportError("Redefinition of port '%s'\n", &ctx->error, link_actn->get->loc, link_actn->get->name);
        } else if (FbldNamesEqual(curr->name, link_actn->put->name)) {
          ReportError("Redefinition of port '%s'\n", &ctx->error, link_actn->put->loc, link_actn->put->name);
        }
      }

      if (FbldNamesEqual(link_actn->put->name, link_actn->get->name)) {
        ReportError("Redefinition of port '%s'\n", &ctx->error, link_actn->get->loc, link_actn->get->name);
      }

      Ports getport = {
        .type = link_actn->type,
        .polarity = FBLD_GET_POLARITY,
        .name = link_actn->get->name,
        .next = ports
      };
      Ports putport = {
        .type = link_actn->type,
        .polarity = FBLD_PUT_POLARITY,
        .name = link_actn->put->name,
        .next = &getport
      };

      return CheckActn(ctx, env, vars, &putport, link_actn->body);
    }

    case FBLD_EXEC_ACTN: {
      FbldExecActn* exec_actn = (FbldExecActn*)actn;

      Vars vars_data[exec_actn->execv->size];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execv->size; ++i) {
        FbldExec* exec = exec_actn->execv->xs + i;
        CheckType(ctx, env, exec->type);
        FbldQRef* def_type = CheckActn(ctx, env, vars, ports, exec->actn);
        CheckTypesMatch(exec->actn->loc, exec->type, def_type, &ctx->error);
        vars_data[i].type = exec->type;
        vars_data[i].name = exec->name->name;
        vars_data[i].next = nvars;
        nvars = vars_data + i;
      }
      return CheckActn(ctx, env, nvars, ports, exec_actn->body);
    }

    default: {
      UNREACHABLE("invalid fbld action tag");
      return NULL;
    }
  }
}

// CheckArgV --
//   Check that the given vector of arguments is well typed and does not
//   redefine any arguments.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The environment for type checking.
//   argv - The vector of args to check.
//   vars - Space for argv->size vars to fill in based on the given args.
//
// Result:
//   A pointer into vars with the variable scope implied by the arguments.
//
// Side effects:
//   Fills in elements of vars.
//   Loads program modules as needed to check the arguments. In case
//   there is a problem, reports errors to stderr and sets error to true.
static Vars* CheckArgV(Context* ctx, Env* env, FbldArgV* argv, Vars* vars)
{
  Vars* next = NULL;
  for (size_t arg_id = 0; arg_id < argv->size; ++arg_id) {
    FbldArg* arg = argv->xs[arg_id];

    // Check whether an argument has already been declared with the same name.
    for (size_t i = 0; i < arg_id; ++i) {
      if (FbldNamesEqual(arg->name->name, argv->xs[i]->name->name)) {
        ReportError("Redefinition of %s\n", &ctx->error, arg->name->loc, arg->name->name);
        break;
      }
    }
    CheckType(ctx, env, arg->type);

    vars[arg_id].name = arg->name->name;
    vars[arg_id].type = CheckType(ctx, env, arg->type) ? arg->type : NULL;
    vars[arg_id].next = next;
    arg->type = vars[arg_id].type;
    next = vars + arg_id;
  }
  return next;
}

// DefineName --
//   Check if a given name has already been defined, and add it to the list of
//   defined names.
//
// Inputs:
//   ctx - The context for type checking.
//   name - The name to define.
//   defined - The list of already defined names to check and update.
//
// Results:
//   None.
//
// Side effects:
//   Reports an error if the name is already defined and adds the name to the
//   list of defined names.
static void DefineName(Context* ctx, FbldName* name, FbldNameV* defined)
{
  for (size_t i = 0; i < defined->size; ++i) {
    if (FbldNamesEqual(name->name, defined->xs[i]->name)) {
      ReportError("redefinition of %s\n", &ctx->error, name->loc, name->name);
      ReportError("previous definition was here\n", &ctx->error, defined->xs[i]->loc);
    }
  }
  FblcVectorAppend(ctx->arena, *defined, name);
}

// CheckProto --
//   Check that the given prototype is well formed and well typed.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The current environment.
//   decl - The prototype to check.
//
// Results:
//   None.
//
// Side effects:
//   Prints error messages to stderr and sets error to true if there are any
//   problems.
static void CheckProto(Context* ctx, Env* env, FbldDecl* decl)
{
  DeclList dl = {
    .decl = decl,
    .next = env->svars,
  };
  env->svars = &dl;

  // Check the static parameters.
  // TODO: The static parameters should only be visible to static parameters
  // before it in the list, not all of them.
  for (size_t i = 0; i < decl->paramv->size; ++i) {
    CheckProto(ctx, env, decl->paramv->xs[i]);
  }

  switch (decl->tag) {
    case FBLD_TYPE_DECL: {
      FbldType* type = (FbldType*)decl;
      assert(type->kind != FBLD_UNION_KIND || type->fieldv->size > 0);
      assert(type->kind != FBLD_ABSTRACT_KIND || type->fieldv == NULL);

      if (type->fieldv != NULL) {
        Vars unused[type->fieldv->size];
        CheckArgV(ctx, env, type->fieldv, unused);
      }
      break;
    }

    case FBLD_FUNC_DECL: {
      FbldFunc* func = (FbldFunc*)decl;
      Vars vars_data[func->argv->size];
      CheckArgV(ctx, env, func->argv, vars_data);
      CheckType(ctx, env, func->return_type);
      break;
    }

    case FBLD_PROC_DECL: {
      FbldProc* proc = (FbldProc*)decl;
      Ports ports_data[proc->portv->size];
      Ports* ports = NULL;
      for (size_t port_id = 0; port_id < proc->portv->size; ++port_id) {
        FbldPort* port = proc->portv->xs + port_id;
        for (Ports* curr = ports; curr != NULL; curr = curr->next) {
          if (FbldNamesEqual(curr->name, port->name->name)) {
            ReportError("Redefinition of port '%s'\n", &ctx->error, port->name->loc, port->name->name);
          }
        }

        ports_data[port_id].name = port->name->name;
        ports_data[port_id].type = CheckType(ctx, env, port->type) ? port->type : NULL;
        ports_data[port_id].polarity = port->polarity;
        ports_data[port_id].next = ports;
        ports = ports_data + port_id;
      }

      Vars vars_data[proc->argv->size];
      // TODO: Split CheckArgV into two functions, one for checking
      // prototypes (no vars needed) and one for checking bodies (not
      // redefinition or other checks needed)?
      CheckArgV(ctx, env, proc->argv, vars_data);
      CheckType(ctx, env, proc->return_type);
      break;
    }

    case FBLD_INTERF_DECL: {
      FbldInterf* interf = (FbldInterf*)decl;
      CheckInterf(ctx, env, interf);
      break;
    }

    case FBLD_MODULE_DECL: {
      FbldModule* module = (FbldModule*)decl;
      CheckModuleHeader(ctx, env, module);
      break;
    }

    default:
      UNREACHABLE("Invalid decl tag");
  }

  env->svars = dl.next;
}

// CheckProtos --
//   Check that the declarations in the environment are well formed and well
//   typed. Only the prototypes of the declarations are checked, not the
//   bodies.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The environment of declarations to check.
//
// Results:
//   None.
//
// Side effects:
//   Prints error messages to stderr and sets error to true if there are any
//   problems. Function and process declarations may be NULL to indicate these
//   declarations belong to an interface declaration; this is not considered
//   an error.
static void CheckProtos(Context* ctx, Env* env)
{
  FbldNameV defined;
  FblcVectorInit(ctx->arena, defined);

  // Check import statements.
  for (size_t import_id = 0; import_id < env->prgm->importv->size; ++import_id) {
    FbldImport* import = env->prgm->importv->xs[import_id];
    for (size_t i = 0; i < import->itemv->size; ++i) {
      DefineName(ctx, import->itemv->xs[i]->dest, &defined);

      FbldQRefV paramv = { .size = 0, .xs = NULL };

      FbldQRef entity = {
        .name = import->itemv->xs[i]->source,
        .paramv = &paramv,
        .mref = import->mref,
        .r = NULL
      };

      Env* import_env = import->mref == NULL ? env->parent : env;
      CheckQRef(ctx, import_env, &entity);
    }
  }

  for (size_t decl_id = 0; decl_id < env->prgm->declv->size; ++decl_id) {
    FbldDecl* decl = env->prgm->declv->xs[decl_id];
    DefineName(ctx, decl->name, &defined);
    CheckProto(ctx, env, decl);
  }
}

// CheckBodies --
//   Check that the declarations in the environment are well formed and well
//   typed. Only the declaration bodies are checked.
//
// Inputs:
//   ctx - The context for type checking.
//   env - The environment of declarations to check.
//
// Results:
//   None.
//
// Side effects:
//   Behavior is undefined if the declaration prototypes have not already been
//   determined to be well formed.
//   Prints error messages to stderr and sets error to true if there are any
//   problems. Function and process declarations may be NULL to indicate these
//   declarations belong to an interface declaration; this is not considered
//   an error.
//   Loads and checks required top-level interface declarations and module
//   headers (not module definitions), adding them to the context.
static void CheckBodies(Context* ctx, Env* env)
{
  for (size_t decl_id = 0; decl_id < env->prgm->declv->size; ++decl_id) {
    FbldDecl* decl = env->prgm->declv->xs[decl_id];

    DeclList dl = {
      .decl = decl,
      .next = env->svars,
    };
    env->svars = &dl;

    switch (decl->tag) {
      case FBLD_TYPE_DECL: {
        // Types do not have any bodies to check.
        break;
      }

      case FBLD_FUNC_DECL: {
        FbldFunc* func = (FbldFunc*)decl;
        Vars vars_data[func->argv->size];
        Vars* vars = CheckArgV(ctx, env, func->argv, vars_data);
        FbldQRef* body_type = CheckExpr(ctx, env, vars, func->body);
        CheckTypesMatch(func->body->loc, func->return_type, body_type, &ctx->error);
        break;
      }

      case FBLD_PROC_DECL: {
        FbldProc* proc = (FbldProc*)decl;
        Ports ports_data[proc->portv->size];
        Ports* ports = NULL;
        for (size_t port_id = 0; port_id < proc->portv->size; ++port_id) {
          FbldPort* port = proc->portv->xs + port_id;
          ports_data[port_id].name = port->name->name;
          ports_data[port_id].type = CheckType(ctx, env, port->type) ? port->type : NULL;
          ports_data[port_id].polarity = port->polarity;
          ports_data[port_id].next = ports;
          ports = ports_data + port_id;
        }

        Vars vars_data[proc->argv->size];
        Vars* vars = CheckArgV(ctx, env, proc->argv, vars_data);
        FbldQRef* body_type = CheckActn(ctx, env, vars, ports, proc->body);
        CheckTypesMatch(proc->body->loc, proc->return_type, body_type, &ctx->error);
        break;
      }

      case FBLD_INTERF_DECL: {
        FbldInterf* interf = (FbldInterf*)decl;
        CheckInterf(ctx, env, interf);
        break;
      }

      case FBLD_MODULE_DECL: {
        FbldModule* module = (FbldModule*)decl;
        CheckModule(ctx, env, module);
        break;
      }

      default:
        UNREACHABLE("Invalid decl tag");
    }

    env->svars = dl.next;
  }
}

// FbldCheckQRef -- see fblcs.h for documentation.
bool FbldCheckQRef(FblcArena* arena, FbldProgram* prgm, FbldQRef* qref)
{
  Context ctx = {
    .arena = arena,
    .error = false
  };

  Env env = {
    .parent = NULL,
    .mref = NULL,
    .interf = NULL,
    .prgm = prgm,
    .svars = NULL
  };

  CheckQRef(&ctx, &env, qref);
  return !ctx.error;
}

// CheckArgsMatch --
//   Check whether args from a module prototype match the interface prototype.
//
// Inputs:
//   ctx - The context for type checking.
//   src - The source qref used to import the foreign args from args_i
//   args_i - The interface arg vector.
//   args_m - The module arg vector.
//
// Result:
//   None.
//
// Side effects:
//   Prints an error message and sets ctx->error if the module arguments don't
//   match the interface arguments.
static void CheckArgsMatch(Context* ctx, FbldQRef* src, FbldArgV* args_i,
    FbldArgV* args_m)
{
  if (args_i->size != args_m->size) {
    ReportError("Wrong number of args, expected %i but found %i\n",
        &ctx->error, src->name->loc, args_i->size, args_m->size);
  }

  for (size_t i = 0; i < args_i->size && i < args_m->size; ++i) {
    CheckTypesMatch(args_m->xs[i]->type->name->loc, FbldImportQRef(ctx->arena, src, args_i->xs[i]->type), args_m->xs[i]->type, &ctx->error);
    if (!FbldNamesEqual(args_i->xs[i]->name->name, args_m->xs[i]->name->name)) {
      ReportError("Module name %s does not match interface name %s\n",
          &ctx->error, args_m->xs[i]->name->loc, args_m->xs[i]->name->name,
          args_i->xs[i]->name->name);
    }
  }
}

// CheckDeclsMatch --
//   Check that a module declaration matches its corresponding interface
//   declaration.
//
// Inputs:
//   ctx - The context for type checking.
//   src - The qref referring to decl_m in the current context.
//   decl_i - The declared in the interface.
//   decl_m - The declared in the module.
//
// Results:
//   None.
//
// Side effects:
//   Prints a message to stderr if the declarations don't match.
static void CheckDeclsMatch(Context* ctx, FbldQRef* src, FbldDecl* decl_i, FbldDecl* decl_m)
{
  if (decl_i->tag != decl_m->tag) {
    ReportError("%s does not match interface declaration\n", &ctx->error, decl_m->name->loc, decl_m->name->name);
    return;
  }

  switch (decl_i->tag) {
    case FBLD_TYPE_DECL: {
      FbldType* type_i = (FbldType*)decl_i;
      FbldType* type_m = (FbldType*)decl_m;
      switch (type_i->kind) {
        case FBLD_STRUCT_KIND: {
          if (type_m->kind != FBLD_STRUCT_KIND) {
            ReportError("%s previously declared as a struct\n", &ctx->error, decl_m->name->loc, decl_m->name->name);
          }
        } break;

        case FBLD_UNION_KIND: {
          if (type_m->kind != FBLD_UNION_KIND) {
            ReportError("%s previously declared as a union\n", &ctx->error, decl_m->name->loc, decl_m->name->name);
          }
        } break;

        case FBLD_ABSTRACT_KIND: {
          // Nothing to check.
        } break;
      }

      if (type_i->kind != FBLD_ABSTRACT_KIND) {
        CheckArgsMatch(ctx, src, type_i->fieldv, type_m->fieldv);
      }
    } break;

    case FBLD_FUNC_DECL: {
      FbldFunc* func_i = (FbldFunc*)decl_i;
      FbldFunc* func_m = (FbldFunc*)decl_m;
      CheckArgsMatch(ctx, src, func_i->argv, func_m->argv);
      CheckTypesMatch(func_m->return_type->name->loc, FbldImportQRef(ctx->arena, src, func_i->return_type), func_m->return_type, &ctx->error);
    } break;

    case FBLD_PROC_DECL: {
      FbldProc* proc_i = (FbldProc*)decl_i;
      FbldProc* proc_m = (FbldProc*)decl_m;
      if (proc_i->portv->size != proc_m->portv->size) {
        ReportError("Process %s does not match its interface declaration: expectd %i ports but found %i\n", &ctx->error, decl_m->name->loc, decl_m->name->name,
            proc_i->portv->size, proc_m->portv->size);
      }
    
      for (size_t i = 0; i < proc_i->portv->size && i < proc_m->portv->size; ++i) {
        FbldPort* port_i = proc_i->portv->xs + i;
        FbldPort* port_m = proc_m->portv->xs + i;
        CheckTypesMatch(port_m->type->name->loc, FbldImportQRef(ctx->arena, src, port_i->type), port_m->type, &ctx->error);
    
        if (!FbldNamesEqual(port_i->name->name, port_m->name->name)) {
          ReportError("Expected name %s, but found name %s\n", &ctx->error, decl_m->name->loc,
              port_i->name->name, port_m->name->name);
        }
    
        if (port_i->polarity != port_m->polarity) {
          ReportError("Expected opposite polarity", &ctx->error, port_m->name->loc);
        }
      }
    
      CheckArgsMatch(ctx, src, proc_i->argv, proc_m->argv);
      CheckTypesMatch(proc_m->return_type->name->loc, FbldImportQRef(ctx->arena, src, proc_i->return_type), proc_m->return_type, &ctx->error);
    }

    case FBLD_INTERF_DECL: {
      // TODO: Check interface declarations match.
    }

    case FBLD_MODULE_DECL: {
      // TODO: Check module declarations match.
    }
  }
}

// FbldCheckProgram -- see documentation in fbld.h
bool FbldCheckProgram(FblcArena* arena, FbldProgram* prgm)
{
  Context ctx = {
    .arena = arena,
    .error = false
  };

  Env env = {
    .parent = NULL,
    .mref = NULL,
    .interf = NULL,
    .prgm = prgm,
    .svars = NULL
  };
  CheckEnv(&ctx, &env);
  return !ctx.error;
}

// CheckValue --
//   Check that the value is well formed in the given context.
//
// Inputs:
//   ctx - The type checking context.
//   value - The value to check.
//
// Results:
//   true if the value is well formed, false otherwise.
//
// Side effects:
//   Resolves references in the value.
//   Prints a message to stderr if the value is not well formed.
static bool CheckValue(Context* ctx, Env* env, FbldValue* value)
{
  if (!CheckType(ctx, env, value->type)) {
    return false;
  }

  assert(value->type->r->decl->tag == FBLD_TYPE_DECL);
  FbldType* type = (FbldType*)value->type->r->decl;

  switch (value->kind) {
    case FBLD_STRUCT_KIND: {
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (!CheckValue(ctx, env, value->fieldv->xs[i])) {
          return false;
        }
      }

      // TODO: check that the types of the fields match what is expected.
      return true;
    }

    case FBLD_UNION_KIND: {
      if (!CheckValue(ctx, env, value->fieldv->xs[0])) {
        return false;
      }

      // TODO: check that the type of the argument matches what is expected.
      return true;
    }

    case FBLD_ABSTRACT_KIND: {
      ReportError("type %s is abstract\n", &ctx->error, value->type->name->loc, value->type->name->name);
      return false;
    }

    default: {
      UNREACHABLE("invalid value kind");
      return false;
    }
  }
}

// FblcCheckValue -- see documentation in fbld.h
bool FbldCheckValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  Context ctx = {
    .arena = arena,
    .error = false
  };

  Env env = {
    .parent = NULL,
    .mref = NULL,
    .interf = NULL,
    .prgm = prgm,
    .svars = NULL
  };

  CheckValue(&ctx, &env, value);
  return !ctx.error;
}
