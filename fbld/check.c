// check.c --
//   This file implements routines for checking fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_start, va_end, vfprintf
#include <stdlib.h>   // for NULL
#include <stdio.h>    // for FILE, fprintf, stderr

#include "fbld.h"

#define UNREACHABLE(x) assert(false && x)

// Env --
//   The environment of types and declarations.
//   This is used as a union type for mtype + mdefn.
//   In the case of mtype, the margv and iref fields are set to NULL.
typedef FbldMDefn Env;

// Context --
//   A context for type checking.
//
// Fields:
//   arena - Arena to use for allocations.
//   prgm - Program to add any loaded modules to.
//   path - The module search path.
//   env - The local environment.
//   error - Flag tracking whether any type errors have been encountered.
typedef struct {
  FblcArena* arena;
  FbldProgram* prgm;
  FbldStringV* path;
  Env* env;
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

static void ReportError(const char* format, bool* error, FbldLoc* loc, ...);
static void PrintType(FILE* stream, FbldQRef* type);
static void PrintMRef(FILE* stream, FbldMRef* mref);

static FbldType* LookupType(Context* ctx, FbldQRef* entity);
static FbldFunc* LookupFunc(Context* ctx, FbldQRef* entity);
// static FbldProc* LookupProc(Context* ctx, FbldQRef* entity);
static FbldMType* LookupMType(Context* ctx, FbldMRef* mref);

static void CheckTypesMatch(FbldLoc* loc, FbldQRef* expected, FbldQRef* actual, bool* error);
static bool ResolveQRef(Context* ctx, FbldQRef* qref);
static FbldQRef* ImportQRef(Context* ctx, FbldMRef* mref, FbldQRef* qref);
static bool CheckType(Context* ctx, FbldQRef* qref);
static bool CheckMRef(Context* ctx, FbldMRef* mref);
static bool CheckIRef(Context* ctx, FbldIRef* iref);
static FbldQRef* CheckExpr(Context* ctx, Vars* vars, FbldExpr* expr);
static FbldQRef* CheckActn(Context* ctx, Vars* vars, Ports* ports, FbldActn* actn);
static Vars* CheckArgV(Context* ctx, FbldArgV* argv, Vars* vars);
static void CheckDecls(Context* ctx);
static bool ArgsEqual(FbldArgV* a, FbldArgV* b);
static bool CheckTypeDeclsMatch(Context* ctx, FbldType* mtype_type, FbldType* mdefn_type);
static bool CheckFuncDeclsMatch(Context* ctx, FbldFunc* mtype_type, FbldFunc* mdefn_type);
static bool CheckValue(Context* ctx, FbldValue* value);

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

// PrintType --
//   Print a type in human readable format to the given stream.
//
// Inputs:
//   stream - Stream to print the type to.
//   type - The type to print.
//
// Results:
//   None.
//
// Side effects:
//   Prints the type to the stream.
static void PrintType(FILE* stream, FbldQRef* type)
{
  fprintf(stream, "%s", type->uname->name);
  if (type->umref != NULL) {
    fprintf(stream, "@");
    PrintMRef(stream, type->umref);
  }
}

// PrintMRef --
//   Print an mref in human readable format to the given stream.
//
// Inputs:
//   stream - Stream to print the type to.
//   mref - The mref to print. Must not be null.
//
// Results:
//   None.
//
// Side effects:
//   Prints the mref to the stream.
static void PrintMRef(FILE* stream, FbldMRef* mref)
{
  assert(mref != NULL);
  fprintf(stream, "%s<", mref->name->name);

  if (mref->targv != NULL) {
    for (size_t i = 0; i < mref->targv->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintType(stream, mref->targv->xs[i]);
    }
    fprintf(stream, ";");

    for (size_t i = 0; i < mref->margv->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintMRef(stream, mref->margv->xs[i]);
    }
  }
  fprintf(stream, ">");
}

// CheckTypesMatch --
//   Check whether the given types match.
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
static void CheckTypesMatch(FbldLoc* loc, FbldQRef* expected, FbldQRef* actual, bool* error)
{
  if (expected == NULL || actual == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  if (!FbldQRefsEqual(expected, actual)) {
    ReportError("Expected type ", error, loc);
    PrintType(stderr, expected);
    fprintf(stderr, ", but found type ");
    PrintType(stderr, actual);
    fprintf(stderr, "\n");
  }
}

// LookupType --
//   Look up the FbldType* for the given resolved entity.
//
// Inputs:
//   ctx - The context for type checking.
//   entity - The type to look up.
//
// Results:
//   The FbldType* for the given resolved type, or NULL if no such type could
//   be found.
//
// Side effects:
//   Loads program modules as needed to find the type definition.
//   Prints a message to stderr and sets error to true if the program modules
//   needed to find the type definition could not be loaded.
//   It is not considered an error if the type cannot be found.
static FbldType* LookupType(Context* ctx, FbldQRef* entity)
{
  if (entity->rmref == NULL) {
    // Check if this is a type parameter.
    for (size_t i = 0; i < ctx->env->targv->size; ++i) {
      if (FbldNamesEqual(entity->rname->name, ctx->env->targv->xs[i]->name)) {
        // TODO: Don't leak this allocated memory.
        FbldType* type = FBLC_ALLOC(ctx->arena, FbldType);
        type->name = entity->rname;
        type->kind = FBLD_ABSTRACT_KIND;
        type->fieldv = NULL;
        return type;
      }
    }

    // Check if this is a locally defined type.
    for (size_t i = 0; i < ctx->env->typev->size; ++i) {
      if (FbldNamesEqual(entity->rname->name, ctx->env->typev->xs[i]->name->name)) {
        return ctx->env->typev->xs[i];
      }
    }

    // We couldn't find the definition for the local type.
    return NULL;
  }

  // The entity is in a foreign module. Load the mtype for that module and
  // check for the type declaration in there.
  FbldMType* mtype = LookupMType(ctx, entity->rmref);
  if (mtype != NULL) {
    for (size_t i = 0; i < mtype->typev->size; ++i) {
      if (FbldNamesEqual(entity->rname->name, mtype->typev->xs[i]->name->name)) {
        return mtype->typev->xs[i];
      }
    }
  }
  return NULL;
}

// LookupFunc --
//   Look up the FbldFunc* for the given resolved entity.
//
// Inputs:
//   ctx - The context for type checking.
//   entity - The function to look up.
//
// Results:
//   The FbldFunc* for the given resolved function, or NULL if no such
//   function could be found.
//
// Side effects:
//   Loads program modules as needed to find function definition.
//   Prints a message to stderr and sets error to true if the program modules
//   needed to find the function definition could not be loaded.
//   It is not considered an error if the function cannot be found.
static FbldFunc* LookupFunc(Context* ctx, FbldQRef* entity)
{
  if (entity->rmref == NULL) {
    for (size_t i = 0; i < ctx->env->funcv->size; ++i) {
      if (FbldNamesEqual(entity->rname->name, ctx->env->funcv->xs[i]->name->name)) {
        return ctx->env->funcv->xs[i];
      }
    }

    // We couldn't find the definition for the local function.
    return NULL;
  }

  // The entity is in a foreign module. Load the mtype for that module and
  // check for the func declaration in there.
  FbldMType* mtype = LookupMType(ctx, entity->rmref);
  if (mtype != NULL) {
    for (size_t i = 0; i < mtype->funcv->size; ++i) {
      if (FbldNamesEqual(entity->rname->name, mtype->funcv->xs[i]->name->name)) {
        return mtype->funcv->xs[i];
      }
    }
  }
  return NULL;
}

// LookupProc --
//   Look up the FbldProc* for the given resolved entity.
//
// Inputs:
//   ctx - The context for type checking.
//   entity - The process to look up.
//
// Results:
//   The FbldProc* for the given resolved process, or NULL if no such
//   process could be found.
//
// Side effects:
//   Loads program modules as needed to find process definition.
//   Prints a message to stderr and sets error to true if the program modules
//   needed to find the process definition could not be loaded.
//   It is not considered an error if the process cannot be found.
//static FbldProc* LookupProc(Context* ctx, FbldQRef* entity)
//{
//  if (entity->rmref == NULL) {
//    for (size_t i = 0; i < ctx->env->procv->size; ++i) {
//      if (FbldNamesEqual(entity->rname->name, ctx->env->procv->xs[i]->name->name)) {
//        return ctx->env->procv->xs[i];
//      }
//    }
//
//    // We couldn't find the definition for the local process.
//    return NULL;
//  }
//
//  // The entity is in a foreign module. Load the mtype for that module and
//  // check for the proc declaration in there.
//  FbldMType* mtype = LookupMType(ctx, entity->rmref);
//  if (mtype != NULL) {
//    for (size_t i = 0; i < mtype->procv->size; ++i) {
//      if (FbldNamesEqual(entity->rname->name, mtype->procv->xs[i]->name->name)) {
//        return mtype->procv->xs[i];
//      }
//    }
//  }
//  return NULL;
//}

// LookupMType --
//   Look up the FbldMType* for the given resolved mref.
//
// Inputs:
//   ctx - The context for type checking.
//   mref - A reference to the mtype to look up.
//
// Results:
//   The FbldMType* for the given resolved mref, or NULL if no such mtype
//   could be found.
//
// Side effects:
//   Loads the referenced mtype declaration if it has not already been loaded.
//   Sets error to true and prints a message to stderr if the mtype
//   declaration can not be loaded.
static FbldMType* LookupMType(Context* ctx, FbldMRef* mref)
{
  if (mref->targv == NULL) {
    // This is a module parameter.
    for (size_t i = 0; i < ctx->env->margv->size; ++i) {
      if (FbldNamesEqual(ctx->env->margv->xs[i]->name->name, mref->name->name)) {
        return FbldLoadMType(ctx->arena, ctx->path, ctx->env->margv->xs[i]->iref->name->name, ctx->prgm);
      }
    }

    ReportError("module %s not defined\n", &ctx->error, mref->name->loc, mref->name->name);
    return NULL;
  }

  // This is a global module.
  FbldMDefn* decl = FbldLoadMDecl(ctx->arena, ctx->path, mref->name->name, ctx->prgm);
  if (decl == NULL) {
    return NULL;
  }
  return FbldLoadMType(ctx->arena, ctx->path, decl->iref->name->name, ctx->prgm);
}

// ResolveQRef --
//   Resolve the given qref if necessary.
//   This includes checking and resolving the qref's module reference, if
//   any, and inlining all references to local names brought in to the module
//   context with 'using' declarations.
//
// Inputs:
//   ctx - The context for type checking.
//   qref - The qref to resolve.
//
// Result:
//   true if the entity could be resolved, false otherwise.
//
// Side effects:
//   Loads program modules as needed to check module references.
//   Update qref->rmref and qref->rname as appropriate.
//   Prints an error message to stderr and sets error to true if the entity's
//   module reference is not well formed.
//   This function does not check whether the resolved entity actually exists.
static bool ResolveQRef(Context* ctx, FbldQRef* qref)
{
  if (qref->rname != NULL) {
    return true;
  }

  // TODO: Have some way to know if we already attempted and failed to resolve
  // the module, and avoid trying to re-resolve it?

  if (qref->umref == NULL) {
    // Check if the entity has a local name but is imported from a foreign
    // module with a 'using' declaration.
    for (size_t i = 0; i < ctx->env->usingv->size; ++i) {
      FbldUsing* using = ctx->env->usingv->xs[i];
      for (size_t j = 0; j < using->itemv->size; ++j) {
        if (FbldNamesEqual(qref->uname->name, using->itemv->xs[j]->dest->name)) {
          if (!CheckMRef(ctx, using->mref)) {
            return false;
          }
          qref->rmref = using->mref;
          qref->rname = using->itemv->xs[j]->source;
          return true;
        }
      }
    }

    // Otherwise this is a local entity.
    qref->rname = qref->uname;
    return true;
  }

  if (!CheckMRef(ctx, qref->umref)) {
    return false;
  }
  qref->rmref = qref->umref;
  qref->rname = qref->uname;
  return true;
}

// ImportQRef --
//   Import a qref into this module. If the qref is local to this module, it
//   is resolved. Otherwise the qref is expected to already be resolved and
//   local to some other module.
//   Substitutes all references to local type parameters and module parameters
//   with the arguments supplied in the given module reference context.
//
// Inputs:
//   ctx - The context for type checking.
//   mref - The context the entity is being referred to from.
//   qref - The qref to import.
//
// Results:
//   The qref imported into the given context.
//
// Side effects:
//   Resolves the qref if it is local.
static FbldQRef* ImportQRef(Context* ctx, FbldMRef* mref, FbldQRef* qref)
{
  if (mref == NULL) {
    return ResolveQRef(ctx, qref) ? qref : NULL;
  }
  assert(qref->rname != NULL && "foreign qref not already resolved");
  return FbldImportQRef(ctx->arena, ctx->prgm, mref, qref);
}

// CheckType --
//   Check that the given qref refers to a type.
//
// Inputs:
//   ctx - The context for type checking.
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
static bool CheckType(Context* ctx, FbldQRef* qref)
{
  if (!ResolveQRef(ctx, qref)) {
    return false;
  }

  FbldType* type = LookupType(ctx, qref);
  if (type == NULL) {
    ReportError("type %s not defined\n", &ctx->error, qref->uname->loc, qref->uname->name);
    return false;
  }
  return true;
}

// CheckMRef --
//   Verify that the given mref is well formed.
//
// Inputs:
//   ctx - The context for type checking.
//   mref - The module reference to check.
//
// Result:
//   True if the mref is well formed, false otherwise.
//   be resolved.
//
// Side effects:
//   Loads program modules as needed to check the module reference.
//   Sets mref->resolved to the resolved mref.
//   In case there is a problem, reports errors to stderr and sets ctx->error
//   to true.
static bool CheckMRef(Context* ctx, FbldMRef* mref)
{
  // Check if this refers to a module parameter.
  if (ctx->env->margv != NULL) {
    for (size_t i = 0; i < ctx->env->margv->size; ++i) {
      if (FbldNamesEqual(ctx->env->margv->xs[i]->name->name, mref->name->name)) {
        if (mref->targv == NULL && mref->margv == NULL) {
          return true;
        }
        ReportError("arguments to '%s' not allowed\n", &ctx->error, mref->name->loc, mref->name->name);
        return false;
      }
    }
  }

  for (size_t i = 0; i < mref->targv->size; ++i) {
    if (!CheckType(ctx, mref->targv->xs[i])) {
      return false;
    }
  }

  for (size_t i = 0; i < mref->margv->size; ++i) {
    if (!CheckMRef(ctx, mref->margv->xs[i])) {
      return false;
    }
  }

  FbldMDefn* mdecl = FbldLoadMDecl(ctx->arena, ctx->path, mref->name->name, ctx->prgm);
  if (mdecl == NULL) {
    ReportError("Unable to load declaration of module %s\n", &ctx->error, mref->name->loc, mref->name->name);
    return false;
  }

  if (mdecl->targv->size != mref->targv->size) {
    ReportError("expected %i type arguments to %s, but found %i\n", &ctx->error,
        mref->name->loc, mdecl->targv->size, mref->name->name, mref->targv->size);
    return false;
  }

  if (mdecl->margv->size == mref->margv->size) {
    for (size_t i = 0; i < mdecl->margv->size; ++i) {
      assert(false && "TODO: Check module args are of correct mtype");
    }
  } else {
    ReportError("expected %i module arguments to %s, but found %i\n", &ctx->error,
        mref->name->loc, mdecl->margv->size, mref->name->name, mref->margv->size);
    return false;
  }
  return true;
}

// CheckIRef --
//   Check and resolve the give interface reference.
//
// Inputs:
//   ctx - The context for type checking.
//   iref - The interface reference to check.
//
// Result:
//   true if the interface reference was valid, false otherwise.
//
// Side effects:
//   Loads program modules as needed to check the interface reference.
//   Resolves the interface reference.
//   In case there is a problem, reports errors to stderr and sets error to
//   true.
static bool CheckIRef(Context* ctx, FbldIRef* iref)
{
  for (size_t i = 0; i < iref->targv->size; ++i) {
    if (!CheckType(ctx, iref->targv->xs[i])) {
      return false;
    }
  }

  FbldMType* mtype = FbldLoadMType(ctx->arena, ctx->path, iref->name->name, ctx->prgm);
  if (mtype == NULL) {
    ReportError("Unable to load mtype declaration for %s\n", &ctx->error, iref->name->loc, iref->name->name);
    return false;
  }

  if (mtype->targv->size != iref->targv->size) {
    ReportError("expected %i type arguments to %s, but found %i\n", &ctx->error,
        iref->name->loc, mtype->targv->size, iref->name->name, iref->targv->size);
    return false;
  }
  return true;
}

// CheckExpr --
//   Check that the given expression is well formed.
//
// Inputs:
//   ctx - The context for type checking.
//   vars - The list of variables currently in scope.
//   expr - The expression to check.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression is not well typed and
//   ctx->error is set to true.
static FbldQRef* CheckExpr(Context* ctx, Vars* vars, FbldExpr* expr)
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
        arg_types[i] = CheckExpr(ctx, vars, app_expr->argv->xs[i]);
      }

      FbldQRef* return_type = NULL;
      FbldArgV* argv = NULL;

      if (!ResolveQRef(ctx, app_expr->func)) {
        return NULL;
      }

      FbldFunc* func = LookupFunc(ctx, app_expr->func);
      if (func != NULL) {
        argv = func->argv;
        return_type = ImportQRef(ctx, app_expr->func->rmref, func->return_type);
      } else {
        FbldType* type = LookupType(ctx, app_expr->func);
        if (type != NULL) {
          if (type->kind != FBLD_STRUCT_KIND) {
            ReportError("Cannot do application on type %s.\n", &ctx->error, app_expr->func->uname->loc, app_expr->func->uname->name);
            return NULL;
          }
          argv = type->fieldv;
          return_type = app_expr->func;
        } else {
          ReportError("'%s' not defined.\n", &ctx->error, app_expr->func->uname->loc, app_expr->func->uname->name);
          return NULL;
        }
      }

      if (argv->size == app_expr->argv->size) {
        for (size_t i = 0; i < argv->size; ++i) {
          FbldQRef* expected = ImportQRef(ctx, app_expr->func->rmref, argv->xs[i]->type);
          CheckTypesMatch(app_expr->argv->xs[i]->loc, expected, arg_types[i], &ctx->error);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", &ctx->error, app_expr->func->uname->loc, argv->size, app_expr->func->uname->name, app_expr->argv->size);
      }
      return return_type;
    }

    case FBLD_ACCESS_EXPR: {
      FbldAccessExpr* access_expr = (FbldAccessExpr*)expr;
      FbldQRef* qref = CheckExpr(ctx, vars, access_expr->obj);
      if (qref == NULL) {
        return NULL;
      }

      FbldType* type = LookupType(ctx, qref);
      assert(type != NULL);
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (FbldNamesEqual(access_expr->field.name->name, type->fieldv->xs[i]->name->name)) {
          access_expr->field.id = i;
          return ImportQRef(ctx, qref->rmref, type->fieldv->xs[i]->type);
        }
      }
      ReportError("%s is not a field of type %s\n", &ctx->error, access_expr->field.name->loc, access_expr->field.name->name, type->name->name);
      return NULL;
    }

    case FBLD_UNION_EXPR: {
      FbldUnionExpr* union_expr = (FbldUnionExpr*)expr;
      FbldQRef* arg_type = CheckExpr(ctx, vars, union_expr->arg);
      if (!CheckType(ctx, union_expr->type)) {
        return NULL;
      }

      FbldType* type_def = LookupType(ctx, union_expr->type);
      assert(type_def != NULL);
      if (type_def->kind != FBLD_UNION_KIND) {
        ReportError("%s does not refer to a union type.\n", &ctx->error, union_expr->type->uname->loc, union_expr->type->uname->name);
        return NULL;
      }

      for (size_t i = 0; i < type_def->fieldv->size; ++i) {
        if (FbldNamesEqual(union_expr->field.name->name, type_def->fieldv->xs[i]->name->name)) {
          union_expr->field.id = i;
          FbldQRef* expected = ImportQRef(ctx, union_expr->type->rmref, type_def->fieldv->xs[i]->type);
          CheckTypesMatch(union_expr->arg->loc, expected, arg_type, &ctx->error);
          return union_expr->type;
        }
      }
      ReportError("%s is not a field of type %s\n", &ctx->error, union_expr->field.name->loc, union_expr->field.name->name, union_expr->type->uname->name);
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

      CheckType(ctx, let_expr->type);
      FbldQRef* def_type = CheckExpr(ctx, vars, let_expr->def);
      CheckTypesMatch(let_expr->def->loc, let_expr->type, def_type, &ctx->error);

      Vars nvars = {
        .type = let_expr->type,
        .name = let_expr->var->name,
        .next = vars
      };
      return CheckExpr(ctx, &nvars, let_expr->body);
    }

    case FBLD_COND_EXPR: {
      FbldCondExpr* cond_expr = (FbldCondExpr*)expr;
      FbldQRef* type = CheckExpr(ctx, vars, cond_expr->select);
      if (type != NULL) {
        FbldType* type_def = LookupType(ctx, type);
        assert(type_def != NULL);
        if (type_def->kind == FBLD_UNION_KIND) {
          if (type_def->fieldv->size != cond_expr->argv->size) {
            ReportError("Expected %d arguments, but %d were provided.\n", &ctx->error, cond_expr->_base.loc, type_def->fieldv->size, cond_expr->argv->size);
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", &ctx->error, cond_expr->select->loc, type_def->name->name);
        }
      }

      FbldQRef* result_type = NULL;
      assert(cond_expr->argv->size > 0);
      for (size_t i = 0; i < cond_expr->argv->size; ++i) {
        FbldQRef* arg_type = CheckExpr(ctx, vars, cond_expr->argv->xs[i]);
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
static FbldQRef* CheckActn(Context* ctx, Vars* vars, Ports* ports, FbldActn* actn)
{
  switch (actn->tag) {
    case FBLD_EVAL_ACTN: {
      FbldEvalActn* eval_actn = (FbldEvalActn*)actn;
      return CheckExpr(ctx, vars, eval_actn->arg);
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
      FbldQRef* arg_type = CheckExpr(ctx, vars, put_actn->arg);

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
      FbldQRef* type = CheckExpr(ctx, vars, cond_actn->select);
      if (type != NULL) {
        FbldType* type_def = LookupType(ctx, type);
        assert(type_def != NULL);
        if (type_def->kind == FBLD_UNION_KIND) {
          if (type_def->fieldv->size != cond_actn->argv->size) {
            ReportError("Expected %d arguments, but %d were provided.\n", &ctx->error, cond_actn->_base.loc, type_def->fieldv->size, cond_actn->argv->size);
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", &ctx->error, cond_actn->select->loc, type_def->name->name);
        }
      }

      FbldQRef* result_type = NULL;
      assert(cond_actn->argv->size > 0);
      for (size_t i = 0; i < cond_actn->argv->size; ++i) {
        FbldQRef* arg_type = CheckActn(ctx, vars, ports, cond_actn->argv->xs[i]);
        CheckTypesMatch(cond_actn->argv->xs[i]->loc, result_type, arg_type, &ctx->error);
        result_type = (result_type == NULL) ? arg_type : result_type;
      }
      return result_type;
    }

    case FBLD_CALL_ACTN: {
      assert(false && "TODO");
      return NULL;
    }

    case FBLD_LINK_ACTN: {
      assert(false && "TODO");
      return NULL;
    }

    case FBLD_EXEC_ACTN: {
      assert(false && "TODO");
      return NULL;
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
static Vars* CheckArgV(Context* ctx, FbldArgV* argv, Vars* vars)
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
    CheckType(ctx, arg->type);

    vars[arg_id].name = arg->name->name;
    vars[arg_id].type = CheckType(ctx, arg->type) ? arg->type : NULL;
    vars[arg_id].next = next;
    arg->type = vars[arg_id].type;
    next = vars + arg_id;
  }
  return next;
}

// CheckDecls --
//   Check that the declarations in the environment of the context are well
//   formed and well typed.
//
// Inputs:
//   ctx - The context for type declaration with the environment to check.
//
// Results:
//   None.
//
// Side effects:
//   Prints error messages to stderr and sets error to true if there are any
//   problems. Function and process declarations may be NULL to indicate these
//   declarations belong to an mtype declaration; this is not considered an
//   error.
//   Loads and checks required mtype and mdecls (not mdefns), adding them to
//   the context.
static void CheckDecls(Context* ctx)
{
  // localv is a list of all local names for types, funcs, and procs.
  // TODO: Don't leak this allocated vector.
  FbldNameV localv;
  FblcVectorInit(ctx->arena, localv);

  // Add type parameters to localv.
  for (size_t i = 0; i < ctx->env->targv->size; ++i) {
    FblcVectorAppend(ctx->arena, localv, ctx->env->targv->xs[i]);
  }

  // Add imported entities to localv.
  for (size_t i = 0; i < ctx->env->usingv->size; ++i) {
    FbldUsing* using = ctx->env->usingv->xs[i];
    for (size_t j = 0; j < using->itemv->size; ++j) {
      FblcVectorAppend(ctx->arena, localv, using->itemv->xs[j]->dest);
    }
  }

  // Add locally declared types to localv.
  for (size_t i = 0; i < ctx->env->typev->size; ++i) {
    FblcVectorAppend(ctx->arena, localv, ctx->env->typev->xs[i]->name);
  }

  // Add locally declared funcs to localv.
  for (size_t i = 0; i < ctx->env->funcv->size; ++i) {
    FblcVectorAppend(ctx->arena, localv, ctx->env->funcv->xs[i]->name);
  }

  // Add locally declared procs to localv.
  for (size_t i = 0; i < ctx->env->procv->size; ++i) {
    FblcVectorAppend(ctx->arena, localv, ctx->env->procv->xs[i]->name);
  }

  // Check that the local names are unique.
  for (size_t i = 0; i < localv.size; ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (FbldNamesEqual(localv.xs[i]->name, localv.xs[j]->name)) {
        ReportError("Redefinition of %s\n", &ctx->error, localv.xs[i]->loc, localv.xs[i]->name);
        break;
      }
    }
  }

  // Check using declarations.
  for (size_t using_id = 0; using_id < ctx->env->usingv->size; ++using_id) {
    FbldUsing* using = ctx->env->usingv->xs[using_id];
    if (CheckMRef(ctx, using->mref)) {
      for (size_t i = 0; i < using->itemv->size; ++i) {
        // TODO: Don't leak this allocation.
        FbldQRef* entity = FBLC_ALLOC(ctx->arena, FbldQRef);
        entity->uname = using->itemv->xs[i]->source;
        entity->umref = using->mref;
        entity->rname = entity->uname;
        entity->rmref = entity->umref;
        if ((LookupType(ctx, entity) == NULL && LookupFunc(ctx, entity) == NULL)) {
          ReportError("%s is not exported by %s\n", &ctx->error,
              using->itemv->xs[i]->source->loc, using->itemv->xs[i]->source->name,
              using->mref->name->name);
        }
      }
    }
  }

  // Check type declarations.
  for (size_t type_id = 0; type_id < ctx->env->typev->size; ++type_id) {
    FbldType* type = ctx->env->typev->xs[type_id];
    assert(type->kind != FBLD_UNION_KIND || type->fieldv->size > 0);
    assert(type->kind != FBLD_ABSTRACT_KIND || type->fieldv == NULL);

    if (type->fieldv != NULL) {
      Vars unused[type->fieldv->size];
      CheckArgV(ctx, type->fieldv, unused);
    }
  }

  // Check func declarations
  for (size_t func_id = 0; func_id < ctx->env->funcv->size; ++func_id) {
    FbldFunc* func = ctx->env->funcv->xs[func_id];
    Vars vars_data[func->argv->size];
    Vars* vars = CheckArgV(ctx, func->argv, vars_data);
    CheckType(ctx, func->return_type);

    if (func->body != NULL) {
      FbldQRef* body_type = CheckExpr(ctx, vars, func->body);
      CheckTypesMatch(func->body->loc, func->return_type, body_type, &ctx->error);
    }
  }

  // Check proc declarations
  for (size_t proc_id = 0; proc_id < ctx->env->procv->size; ++proc_id) {
    FbldProc* proc = ctx->env->procv->xs[proc_id];
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
      ports_data[port_id].type = CheckType(ctx, port->type) ? port->type : NULL;
      ports_data[port_id].polarity = port->polarity;
      ports_data[port_id].next = ports;
      ports = ports_data + port_id;
    }

    Vars vars_data[proc->argv->size];
    Vars* vars = CheckArgV(ctx, proc->argv, vars_data);
    CheckType(ctx, proc->return_type);

    if (proc->body != NULL) {
      FbldQRef* body_type = CheckActn(ctx, vars, ports, proc->body);
      CheckTypesMatch(proc->body->loc, proc->return_type, body_type, &ctx->error);
    }
  }
}

// FbldCheckMType -- see fblcs.h for documentation.
bool FbldCheckMType(FblcArena* arena, FbldStringV* path, FbldMType* mtype, FbldProgram* prgm)
{
  Env env = {
    .name = mtype->name,
    .targv = mtype->targv,
    .margv = NULL,
    .iref = NULL,
    .usingv = mtype->usingv,
    .typev = mtype->typev,
    .funcv = mtype->funcv,
    .procv = mtype->procv
  };

  Context ctx = {
    .arena = arena,
    .prgm = prgm,
    .path = path,
    .env = &env,
    .error = false
  };
  CheckDecls(&ctx);
  return !ctx.error;
}

// FbldCheckMDecl -- see documentation in fbld.h
bool FbldCheckMDecl(FblcArena* arena, FbldStringV* path, FbldMDefn* mdefn, FbldProgram* prgm)
{
  FbldUsingV usingv = { .xs = NULL, .size = 0};
  FbldTypeV typev = { .xs = NULL, .size = 0};
  FbldFuncV funcv = { .xs = NULL, .size = 0};
  FbldProcV procv = { .xs = NULL, .size = 0};

  Env env = {
    .name = mdefn->name,
    .targv = mdefn->targv,
    .margv = mdefn->margv,
    .usingv = &usingv,
    .typev = &typev,
    .funcv = &funcv,
    .procv = &procv
  };

  Context ctx = {
    .arena = arena,
    .prgm = prgm,
    .path = path,
    .env = &env,
    .error = false
  };

  CheckIRef(&ctx, mdefn->iref);
  return !ctx.error;
}

// ArgsEqual --
//   Test whether two arg vectors are the same.
//
// Result:
//   true if the two argument vectors have the same number, resolved type, and
//   names of arguments, false otherwise.
//
// Side effects:
//   None.
static bool ArgsEqual(FbldArgV* a, FbldArgV* b)
{
  if (a->size != b->size) {
    return false;
  }

  for (size_t i = 0; i < a->size; ++i) {
    if (!FbldQRefsEqual(a->xs[i]->type, b->xs[i]->type)) {
      return false;
    }

    if (!FbldNamesEqual(a->xs[i]->name->name, b->xs[i]->name->name)) {
      return false;
    }
  }
  return true;
}

// CheckTypeDeclsMatch --
//   Check that a type declared in an mdefn matches its declaration in the
//   mtype.
//
// Inputs:
//   ctx - The context for type checking.
//   mtype_type - The type as declared in the mtype.
//   mdefn_type - The type as declared in the mdefn.
//
// Returns:
//   true if the mdefn type matches the mtype type, false otherwise.
//
// Side effects:
//   Prints a message to stderr if the types don't match.
static bool CheckTypeDeclsMatch(Context* ctx, FbldType* mtype_type, FbldType* mdefn_type)
{
  switch (mtype_type->kind) {
    case FBLD_STRUCT_KIND: {
      if (mdefn_type->kind != FBLD_STRUCT_KIND) {
        ReportError("%s previously declared as a struct\n", &ctx->error, mdefn_type->name->loc, mdefn_type->name->name);
        return false;
      }
    } break;

    case FBLD_UNION_KIND: {
      if (mdefn_type->kind != FBLD_UNION_KIND) {
        ReportError("%s previously declared as a union\n", &ctx->error, mdefn_type->name->loc, mdefn_type->name->name);
        return false;
      }
    } break;

    case FBLD_ABSTRACT_KIND: {
      // Nothing to check.
    } break;
  }

  if (mtype_type->kind != FBLD_ABSTRACT_KIND) {
    if (!ArgsEqual(mtype_type->fieldv, mdefn_type->fieldv)) {
      ReportError("Type %s does not match its interface declaration\n", &ctx->error, mdefn_type->name->loc, mdefn_type->name->name);
      return false;
    }
  }
  return true;
}

// CheckFuncDeclsMatch --
//   Check that a function declared in an mdefn matches its declaration in the
//   mtype.
//
// Inputs:
//   ctx - The context for type checking.
//   mtype_func - The func as declared in the mtype.
//   mdefn_func - The func as declared in the mdefn.
//
// Returns:
//   true if the mdefn func matches the mtype func, false otherwise.
//
// Side effects:
//   Prints a message to stderr if the functions don't match.
static bool CheckFuncDeclsMatch(Context* ctx, FbldFunc* mtype_func, FbldFunc* mdefn_func)
{
  if (!ArgsEqual(mtype_func->argv, mdefn_func->argv)) {
    ReportError("Function %s does not match its interface declaration\n", &ctx->error, mdefn_func->name->loc, mdefn_func->name->name);
    return false;
  }

  if (!FbldQRefsEqual(mtype_func->return_type, mdefn_func->return_type)) {
    ReportError("Function %s does not match its interface declaration\n", &ctx->error, mdefn_func->name->loc, mdefn_func->name->name);
    return false;
  }
  return true;
}

// FbldCheckMDefn -- see documentation in fbld.h
bool FbldCheckMDefn(FblcArena* arena, FbldStringV* path, FbldMDefn* mdefn, FbldProgram* prgm)
{
  Context ctx = {
    .arena = arena,
    .prgm = prgm,
    .path = path,
    .env = mdefn,
    .error = false
  };
  CheckDecls(&ctx);

  if (ctx.error) {
    return false;
  }

  // Verify the mdefn has everything it should according to its interface.
  FbldMType* mtype = FbldLoadMType(arena, path, mdefn->iref->name->name, prgm);
  if (mtype == NULL) {
    return false;
  }

  for (size_t mtype_type_id = 0; mtype_type_id < mtype->typev->size; ++mtype_type_id) {
    FbldType* mtype_type = mtype->typev->xs[mtype_type_id];
    for (size_t mdefn_type_id = 0; mdefn_type_id < mdefn->typev->size; ++mdefn_type_id) {
      FbldType* mdefn_type = mdefn->typev->xs[mdefn_type_id];
      if (FbldNamesEqual(mtype_type->name->name, mdefn_type->name->name)) {
        CheckTypeDeclsMatch(&ctx, mtype_type, mdefn_type);

        // Set mtype_type to NULL to indicate we found the matching type.
        mtype_type = NULL;
        break;
      }
    }

    if (mtype_type != NULL) {
      ReportError("No implementation found for type %s from the interface\n", &ctx.error, mdefn->name->loc, mtype_type->name->name);
    }
  }

  for (size_t mtype_func_id = 0; mtype_func_id < mtype->funcv->size; ++mtype_func_id) {
    FbldFunc* mtype_func = mtype->funcv->xs[mtype_func_id];
    for (size_t mdefn_func_id = 0; mdefn_func_id < mdefn->funcv->size; ++mdefn_func_id) {
      FbldFunc* mdefn_func = mdefn->funcv->xs[mdefn_func_id];
      if (FbldNamesEqual(mtype_func->name->name, mdefn_func->name->name)) {
        CheckFuncDeclsMatch(&ctx, mtype_func, mdefn_func);

        // Set mtype_func to NULL to indicate we found the matching type.
        mtype_func = NULL;
        break;
      }
    }

    if (mtype_func != NULL) {
      ReportError("No implementation found for func %s from the interface\n", &ctx.error, mdefn->name->loc, mtype_func->name->name);
    }
  }

  // TODO: Check proc decls match.

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
static bool CheckValue(Context* ctx, FbldValue* value)
{
  if (!ResolveQRef(ctx, value->type)) {
    return false;
  }
  FbldType* type = LookupType(ctx, value->type);
  if (type == NULL) {
    ReportError("type %s not defined\n", &ctx->error, value->type->uname->loc, value->type->uname->name);
    return false;
  }

  switch (value->kind) {
    case FBLD_STRUCT_KIND: {
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (!CheckValue(ctx, value->fieldv->xs[i])) {
          return false;
        }
      }

      // TODO: check that the types of the fields match what is expected.
      return true;
    }

    case FBLD_UNION_KIND: {
      if (!CheckValue(ctx, value->fieldv->xs[0])) {
        return false;
      }

      // TODO: check that the type of the argument matches what is expected.
      return true;
    }

    case FBLD_ABSTRACT_KIND: {
      ReportError("type %s is abstract\n", &ctx->error, value->type->uname->loc, value->type->uname->name);
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
  FbldStringV path = { .size = 0, .xs = NULL };

  // Context an empty context representing the global context.
  FbldName name = {
    .name = "(global)",
    .loc = value->type->uname->loc
  };
  FbldNameV targv = { .size = 0, .xs = NULL };
  FbldUsingV usingv = { .size = 0, .xs = NULL };
  FbldTypeV typev = { .size = 0, .xs = NULL };
  FbldFuncV funcv = { .size = 0, .xs = NULL };
  FbldProcV procv = { .size = 0, .xs = NULL };
  FbldMDefn mdefn = {
    .name = &name,
    .targv = &targv,
    .margv = NULL,
    .iref = NULL,
    .usingv = &usingv,
    .typev = &typev,
    .funcv = &funcv,
    .procv = &procv
  };

  Context ctx = {
    .arena = arena,
    .prgm = prgm,
    .path = &path,
    .env = &mdefn,
    .error = false
  };
  return CheckValue(&ctx, value);
}
