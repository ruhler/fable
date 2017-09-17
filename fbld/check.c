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
//   In the case of mtype, the margs and iref fields are set to NULL.
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

// Entity --
//   A resolved entity (type, func, or proc).
//
// A resolved entity is an entity in the context of a module after the using
// declarations have been inlined. It can be one of:
//  - A type parameter: mref is NULL.
//  - An entity defined in this module: mref is NULL.
//  - An entity from a module parameter: mref.targs and mref.margs are NULL.
//  - An entity from a global module: mref.targs and mref.margs contain only
//    resolved types.
// Other notes:
//  * The entity is called a "foreign" entity if mref is non-NULL.
//  * There is a 1-to-1 mapping between equal values of Entity and equal
//    concrete type/func/procs.
//  * A resolved entity is not guaranteed to exist.
typedef FbldQName Entity;

// MRef --
//   A module reference for a resolved entity in the context of a module after
//   the using declarations have been inlined. It can be one of:
//  - The local module: mref is NULL.
//  - A module parameter: mref.targs and mref.margs are NULL.
//  - A global module: mref.targs and mref.margs contain only resolved types.
typedef FbldMRef MRef;

// IRef --
//   An interface reference with resolved arguments.
typedef FbldIRef IRef;

// Type --
//   Alias for Entity where the entity is supposed to refer to a type.
typedef Entity Type;

// Vars --
//   List of variables in scope.
typedef struct Vars {
  Type* type;
  const char* name;
  struct Vars* next;
} Vars;

static void ReportError(const char* format, bool* error, FbldLoc* loc, ...);
static void PrintType(FILE* stream, Type* type);
static void PrintMRef(FILE* stream, MRef* mref);

static bool TypesEqual(Type* a, Type* b);
static void CheckTypesMatch(FbldLoc* loc, Type* expected, Type* actual, bool* error);

static FbldType* LookupType(Context* ctx, Entity* entity);
static FbldFunc* LookupFunc(Context* ctx, Entity* entity);
static FbldMType* LookupMType(Context* ctx, FbldMRef* mref);

static Entity* ResolveEntity(Context* ctx, FbldQName* qname);
static Entity* ResolveForeignEntity(Context* ctx, MRef* mref, FbldName* name);
static Type* CheckType(Context* ctx, FbldQName* qname);
static Type* CheckForeignType(Context* ctx, MRef* mref, FbldName* name);
static MRef* CheckMRef(Context* ctx, FbldMRef* mref);
static IRef* CheckIRef(Context* ctx, FbldIRef* iref);
static Type* CheckExpr(Context* ctx, Vars* vars, FbldExpr* expr);
static Vars* CheckArgV(Context* ctx, FbldArgV* argv, Vars* vars);
static void CheckDecls(Context* ctx);

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
static void PrintType(FILE* stream, Type* type)
{
  fprintf(stream, "%s", type->name->name);
  if (type->mref != NULL) {
    fprintf(stream, "@");
    PrintMRef(stream, type->mref);
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
static void PrintMRef(FILE* stream, MRef* mref)
{
  assert(mref != NULL);
  fprintf(stream, "%s<", mref->name->name);

  if (mref->targs != NULL) {
    for (size_t i = 0; i < mref->targs->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintType(stream, mref->targs->xs[i]);
    }
    fprintf(stream, ";");

    for (size_t i = 0; i < mref->margs->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintMRef(stream, mref->margs->xs[i]);
    }
  }
  fprintf(stream, ">");
}

// TypesEqual --
//   Return true if two types are the same.
//
// Inputs:
//   a - The first type.
//   b - The second type.
//
// Result:
//   true if the two types are the same, false otherwise.
//
// Side effects:
//   None.
static bool TypesEqual(Type* a, Type* b)
{
  return FbldQNamesEqual(a, b);
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
static void CheckTypesMatch(FbldLoc* loc, Type* expected, Type* actual, bool* error)
{
  if (expected == NULL || actual == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return;
  }

  if (!TypesEqual(expected, actual)) {
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
static FbldType* LookupType(Context* ctx, Entity* entity)
{
  if (entity->mref == NULL) {
    // Check if this is a type parameter.
    for (size_t i = 0; i < ctx->env->targs->size; ++i) {
      if (FbldNamesEqual(entity->name->name, ctx->env->targs->xs[i]->name)) {
        // TODO: Don't leak this allocated memory.
        FbldType* type = FBLC_ALLOC(ctx->arena, FbldType);
        type->name = entity->name;
        type->kind = FBLD_ABSTRACT_KIND;
        type->fieldv = NULL;
        return type;
      }
    }

    // Check if this is a locally defined type.
    for (size_t i = 0; i < ctx->env->typev->size; ++i) {
      if (FbldNamesEqual(entity->name->name, ctx->env->typev->xs[i]->name->name)) {
        return ctx->env->typev->xs[i];
      }
    }

    // We couldn't find the definition for the local type.
    return NULL;
  }

  // The entity is in a foreign module. Load the mtype for that module and
  // check for the type declaration in there.
  FbldMType* mtype = LookupMType(ctx, entity->mref);
  if (mtype != NULL) {
    for (size_t i = 0; i < mtype->typev->size; ++i) {
      if (FbldNamesEqual(entity->name->name, mtype->typev->xs[i]->name->name)) {
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
static FbldFunc* LookupFunc(Context* ctx, Entity* entity)
{
  if (entity->mref == NULL) {
    for (size_t i = 0; i < ctx->env->funcv->size; ++i) {
      if (FbldNamesEqual(entity->name->name, ctx->env->funcv->xs[i]->name->name)) {
        return ctx->env->funcv->xs[i];
      }
    }

    // We couldn't find the definition for the local function.
    return NULL;
  }

  // The entity is in a foreign module. Load the mtype for that module and
  // check for the func declaration in there.
  FbldMType* mtype = LookupMType(ctx, entity->mref);
  if (mtype != NULL) {
    for (size_t i = 0; i < mtype->funcv->size; ++i) {
      if (FbldNamesEqual(entity->name->name, mtype->funcv->xs[i]->name->name)) {
        return mtype->funcv->xs[i];
      }
    }
  }
  return NULL;
}

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
  if (mref->targs == NULL) {
    // This is a module parameter.
    for (size_t i = 0; i < ctx->env->margs->size; ++i) {
      if (FbldNamesEqual(ctx->env->margs->xs[i]->name->name, mref->name->name)) {
        return FbldLoadMType(ctx->arena, ctx->path, ctx->env->margs->xs[i]->iref->name->name, ctx->prgm);
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


// ResolveEntity --
//   Resolve the entity for the given qualified name.
//
// Inputs:
//   ctx - The context for type checking.
//   qname - The entity to resolve.
//
// Result:
//   The resolved entity, or NULL if the entity could not be resolved.
//
// Side effects:
//   Loads program modules as needed to check the type. Sets error
//   to true and reports errors to stderr if the entity could not be resolved.
//   Note: it is not an error if the entity doesn't refer to an actual
//   type/func/proc declaration. It's only an error if the module the entity
//   belongs to could not be determined or loaded with error.
static Entity* ResolveEntity(Context* ctx, FbldQName* qname)
{
  if (qname->mref == NULL) {
    // Check if the entity has a local name but is imported from a foreign
    // module.
    for (size_t i = 0; i < ctx->env->usingv->size; ++i) {
      FbldUsing* using = ctx->env->usingv->xs[i];
      for (size_t j = 0; j < using->itemv->size; ++j) {
        if (FbldNamesEqual(qname->name->name, using->itemv->xs[j]->dest->name)) {
          using->mref = CheckMRef(ctx, using->mref);
          if (using->mref == NULL) {
            return NULL;
          }

          // TODO: Don't leak this allocated memory.
          Entity* entity = FBLC_ALLOC(ctx->arena, Entity);
          entity->mref = using->mref;
          entity->name = using->itemv->xs[j]->source;
          return entity;
        }
      }
    }

    // Otherwise this is a local entity.
    return qname;
  }

  MRef* mref = CheckMRef(ctx, qname->mref);
  if (mref == NULL) {
    return NULL;
  }

  // TODO: Don't leak this allocated memory.
  Entity* entity = FBLC_ALLOC(ctx->arena, Entity);
  entity->mref = mref;
  entity->name = qname->name;
  return entity;
}

// ResolveForeignEntity --
//   Resolve the entity for the given name in the context of a foreign module.
//
// Inputs:
//   ctx - The context for type checking.
//   mref - The foreign module context.
//   name - The entity to resolve.
//
// Result:
//   The entity resolved with respect to the local context, or NULL if the
//   entity could not be resolved.
//
// Side effects:
//   Loads program modules as needed to check the type. Sets error
//   to true and reports errors to stderr if the entity could not be resolved.
//   Note: it is not an error if the entity doesn't refer to an actual
//   type/func/proc declaration. It's only an error if the module the entity
//   belongs to could not be determined or loaded with error.
static Entity* ResolveForeignEntity(Context* ctx, MRef* mref, FbldName* name)
{
  // TODO: Don't leak this allocated memory.
  Entity* qname = FBLC_ALLOC(ctx->arena, Entity);

  if (mref == NULL) {
    qname->name = name;
    qname->mref = NULL;
    return ResolveEntity(ctx, qname);
  } 

  // If the name is defined in the mtype for the foreign entity, return mref +
  // name. If the name is a type parameter of the foreign entity, look up its
  // value in the mref. If name is imported with 'using' in the foreign
  // entity, then recursively resolve the 'using' statement with a foreign
  // resolve?
  assert(false && "TODO: ResolveForeignEntity");
  return NULL;
}

// CheckType --
//   Check that the given entity refers to a type.
//
// Inputs:
//   ctx - The context for type checking.
//   qname - The entity to check.
//
// Result:
//   The resolved type if the entity refers to a type. NULL if the entity
//   could not be resolved or does not refer to a type.
//
// Side effects:
//   Loads program modules as needed to check the type. Sets error
//   to true and reports errors to stderr if the entity could not be resolved
//   or does not refer to a type.
static Type* CheckType(Context* ctx, FbldQName* qname)
{
  Entity* entity = ResolveEntity(ctx, qname);
  if (entity == NULL) {
    return NULL;
  }

  FbldType* type = LookupType(ctx, entity);
  if (type == NULL) {
    ReportError("type %s not defined\n", &ctx->error, qname->name->loc, qname->name->name);
    return NULL;
  }
  return entity;
}

// CheckForeignType --
//   Check a type from a foreign module.
//
// Inputs:
//   ctx - The context for type checking.
//   mref - The foreign module, may be null to indicate the local module.
//   name - The name of the type in the foreign module.
//
// Result:
//   The type resolved in the local context (not the foreign context). NULL if
//   the type could not be resolved or does not refer to a type.
//
// Side effects:
//   Loads program modules as needed to check the type. Sets error
//   to true and reports errors to stderr if the entity could not be resolved
//   or does not refer to a type.
static Type* CheckForeignType(Context* ctx, MRef* mref, FbldName* name)
{
  Entity* entity = ResolveForeignEntity(ctx, mref, name);
  if (entity == NULL) {
    return NULL;
  }

  FbldType* type = LookupType(ctx, entity);
  if (type == NULL) {
    ReportError("type %s not defined\n", &ctx->error, name->loc, name->name);
    return NULL;
  }
  return entity;
}

// CheckMRef --
//   Verify that the given module reference makes sense.
//
// Inputs:
//   ctx - The context for type checking.
//   mref - The module reference to check.
//
// Result:
//   The resolved module reference, or NULL if the module reference could not
//   be resolved.
//
// Side effects:
//   Loads program modules as needed to check the module reference. In case
//   there is a problem, reports errors to stderr and sets ctx->error to true.
static MRef* CheckMRef(Context* ctx, FbldMRef* mref)
{
  // Check if this refers to a module parameter.
  if (ctx->env->margs != NULL) {
    for (size_t i = 0; i < ctx->env->margs->size; ++i) {
      if (FbldNamesEqual(ctx->env->margs->xs[i]->name->name, mref->name->name)) {
        if (mref->targs == NULL && mref->margs == NULL) {
          return mref;
        }
        ReportError("arguments to '%s' not allowed\n", &ctx->error, mref->name->loc, mref->name->name);
        return NULL;
      }
    }
  }

  // TODO: Don't leak this allocated memory.
  MRef* resolved = FBLC_ALLOC(ctx->arena, MRef);
  resolved->name = mref->name;
  resolved->targs = FBLC_ALLOC(ctx->arena, FbldQNameV);
  resolved->margs = FBLC_ALLOC(ctx->arena, FbldMRefV);

  FblcVectorInit(ctx->arena, *resolved->targs);
  for (size_t i = 0; i < mref->targs->size; ++i) {
    Type* targ = CheckType(ctx, mref->targs->xs[i]);
    if (targ == NULL) {
      return NULL;
    }
    FblcVectorAppend(ctx->arena, *resolved->targs, targ);
  }

  FblcVectorInit(ctx->arena, *resolved->margs);
  for (size_t i = 0; i < mref->margs->size; ++i) {
    MRef* marg = CheckMRef(ctx, mref->margs->xs[i]);
    if (marg == NULL) {
      return NULL;
    }
    FblcVectorAppend(ctx->arena, *resolved->margs, marg);
  }

  FbldMDefn* mdecl = FbldLoadMDecl(ctx->arena, ctx->path, mref->name->name, ctx->prgm);
  if (mdecl == NULL) {
    ReportError("Unable to load declaration of module %s\n", &ctx->error, mref->name->loc, mref->name->name);
    return NULL;
  }

  if (mdecl->targs->size != mref->targs->size) {
    ReportError("expected %i type arguments to %s, but found %i\n", &ctx->error,
        mref->name->loc, mdecl->targs->size, mref->name->name, mref->targs->size);
  }

  if (mdecl->margs->size == mref->margs->size) {
    for (size_t i = 0; i < mdecl->margs->size; ++i) {
      assert(false && "TODO: Check module args are of correct mtype");
    }
  } else {
    ReportError("expected %i module arguments to %s, but found %i\n", &ctx->error,
        mref->name->loc, mdecl->margs->size, mref->name->name, mref->margs->size);
  }
  return resolved;
}

// CheckIRef --
//   Check and resolve the give interface reference.
//
// Inputs:
//   ctx - The context for type checking.
//   iref - The interface reference to check.
//
// Result:
//   The resolved interface reference, or NULL if the interface reference was
//   not valid.
//
// Side effects:
//   Loads program modules as needed to check the interface reference. In case
//   there is a problem, reports errors to stderr and sets error to true.
static IRef* CheckIRef(Context* ctx, FbldIRef* iref)
{
  // TODO: Don't leak this allocated memory.
  IRef* resolved = FBLC_ALLOC(ctx->arena, IRef);
  resolved->name = iref->name;
  resolved->targs = FBLC_ALLOC(ctx->arena, FbldQNameV);
  FblcVectorInit(ctx->arena, *resolved->targs);
  for (size_t i = 0; i < iref->targs->size; ++i) {
    Type* targ = CheckType(ctx, iref->targs->xs[i]);
    if (targ == NULL) {
      return NULL;
    }
    FblcVectorAppend(ctx->arena, *resolved->targs, targ);
  }

  FbldMType* mtype = FbldLoadMType(ctx->arena, ctx->path, iref->name->name, ctx->prgm);
  if (mtype == NULL) {
    ReportError("Unable to load mtype declaration for %s\n", &ctx->error, iref->name->loc, iref->name->name);
    return NULL;
  }

  if (mtype->targs->size != iref->targs->size) {
    ReportError("expected %i type arguments to %s, but found %i\n", &ctx->error,
        iref->name->loc, mtype->targs->size, iref->name->name, iref->targs->size);
    return NULL;
  }
  return resolved;
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
static Type* CheckExpr(Context* ctx, Vars* vars, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLC_VAR_EXPR: {
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

    case FBLC_APP_EXPR: {
      FbldAppExpr* app_expr = (FbldAppExpr*)expr;

      Type* arg_types[app_expr->argv->size];
      for (size_t i = 0; i < app_expr->argv->size; ++i) {
        arg_types[i] = CheckExpr(ctx, vars, app_expr->argv->xs[i]);
      }

      Type* return_type = NULL;
      FbldArgV* argv = NULL;

      Entity* entity = ResolveEntity(ctx, app_expr->func);
      if (entity == NULL) {
        return NULL;
      }
      app_expr->func = entity;

      FbldFunc* func = LookupFunc(ctx, entity);
      if (func != NULL) {
        argv = func->argv;
        return_type = CheckForeignType(ctx, entity->mref, func->return_type->name);
      } else {
        FbldType* type = LookupType(ctx, entity);
        if (type != NULL) {
          if (type->kind != FBLD_STRUCT_KIND) {
            ReportError("Cannot do application on type %s.\n", &ctx->error, app_expr->func->name->loc, app_expr->func->name->name);
            return NULL;
          }
          argv = type->fieldv;
          return_type = entity;
        } else {
          ReportError("'%s' not defined.\n", &ctx->error, app_expr->func->name->loc, app_expr->func->name->name);
          return NULL;
        }
      }

      if (argv->size == app_expr->argv->size) {
        for (size_t i = 0; i < argv->size; ++i) {
          Type* expected = CheckForeignType(ctx, entity->mref, argv->xs[i]->type->name);
          CheckTypesMatch(app_expr->argv->xs[i]->loc, expected, arg_types[i], &ctx->error);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", &ctx->error, app_expr->func->name->loc, argv->size, app_expr->func->name, app_expr->argv->size);
      }
      return return_type;
    }

    case FBLC_ACCESS_EXPR: {
      FbldAccessExpr* access_expr = (FbldAccessExpr*)expr;
      Type* entity = CheckExpr(ctx, vars, access_expr->obj);
      if (entity == NULL) {
        return NULL;
      }

      FbldType* type = LookupType(ctx, entity);
      assert(type != NULL);
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (FbldNamesEqual(access_expr->field.name->name, type->fieldv->xs[i]->name->name)) {
          access_expr->field.id = i;
          return ResolveForeignEntity(ctx, entity->mref, type->fieldv->xs[i]->type->name);
        }
      }
      ReportError("%s is not a field of type %s\n", &ctx->error, access_expr->field.name->loc, access_expr->field.name->name, type->name->name);
      return NULL;
    }

    case FBLC_UNION_EXPR: {
      FbldUnionExpr* union_expr = (FbldUnionExpr*)expr;
      Type* arg_type = CheckExpr(ctx, vars, union_expr->arg);
      Type* type = CheckType(ctx, union_expr->type);
      if (type == NULL) {
        return NULL;
      }
      union_expr->type = type;

      FbldType* type_def = LookupType(ctx, type);
      assert(type_def != NULL);
      if (type_def->kind != FBLD_UNION_KIND) {
        ReportError("%s does not refer to a union type.\n", &ctx->error, union_expr->type->name->loc, union_expr->type->name->name);
        return NULL;
      }

      for (size_t i = 0; i < type_def->fieldv->size; ++i) {
        if (FbldNamesEqual(union_expr->field.name->name, type_def->fieldv->xs[i]->name->name)) {
          union_expr->field.id = i;
          Type* expected = ResolveForeignEntity(ctx, type->mref, type_def->fieldv->xs[i]->type->name);
          CheckTypesMatch(union_expr->arg->loc, expected, arg_type, &ctx->error);
          return type;
        }
      }
      ReportError("%s is not a field of type %s\n", &ctx->error, union_expr->field.name->loc, union_expr->field.name->name, type->name->name);
      return NULL;
    }

    case FBLC_LET_EXPR: {
      FbldLetExpr* let_expr = (FbldLetExpr*)expr;
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FbldNamesEqual(curr->name, let_expr->var->name)) {
          ReportError("Redefinition of variable '%s'\n", &ctx->error, let_expr->var->loc, let_expr->var->name);
          return NULL;
        }
      }

      let_expr->type = CheckType(ctx, let_expr->type);
      Type* def_type = CheckExpr(ctx, vars, let_expr->def);
      CheckTypesMatch(let_expr->def->loc, let_expr->type, def_type, &ctx->error);

      Vars nvars = {
        .type = let_expr->type,
        .name = let_expr->var->name,
        .next = vars
      };
      return CheckExpr(ctx, &nvars, let_expr->body);
    }

    case FBLC_COND_EXPR: {
      FbldCondExpr* cond_expr = (FbldCondExpr*)expr;
      Type* type = CheckExpr(ctx, vars, cond_expr->select);
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

      Type* result_type = NULL;
      assert(cond_expr->argv->size > 0);
      for (size_t i = 0; i < cond_expr->argv->size; ++i) {
        Type* arg_type = CheckExpr(ctx, vars, cond_expr->argv->xs[i]);
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

    vars[arg_id].name = arg->name->name;
    vars[arg_id].type = CheckType(ctx, arg->type);
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
  for (size_t i = 0; i < ctx->env->targs->size; ++i) {
    FblcVectorAppend(ctx->arena, localv, ctx->env->targs->xs[i]);
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
    using->mref = CheckMRef(ctx, using->mref);
    for (size_t i = 0; i < using->itemv->size; ++i) {
      // TODO: Don't leak this allocation.
      Entity* entity = FBLC_ALLOC(ctx->arena, Entity);
      entity->name = using->itemv->xs[i]->source;
      entity->mref = using->mref;
      if ((LookupType(ctx, entity) == NULL && LookupFunc(ctx, entity) == NULL)) {
        ReportError("%s is not exported by %s\n", &ctx->error,
            using->itemv->xs[i]->source->loc, using->itemv->xs[i]->source->name,
            using->mref->name->name);
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
    func->return_type = CheckType(ctx, func->return_type);

    if (func->body != NULL) {
      Type* body_type = CheckExpr(ctx, vars, func->body);
      CheckTypesMatch(func->body->loc, func->return_type, body_type, &ctx->error);
    }
  }
}
// FbldCheckMType -- see fblcs.h for documentation.
bool FbldCheckMType(FblcArena* arena, FbldStringV* path, FbldMType* mtype, FbldProgram* prgm)
{
  Env env = {
    .name = mtype->name,
    .targs = mtype->targs,
    .margs = NULL,
    .iref = NULL,
    .usingv = mtype->usingv,
    .typev = mtype->typev,
    .funcv = mtype->funcv
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

  Env env = {
    .name = mdefn->name,
    .targs = mdefn->targs,
    .margs = mdefn->margs,
    .usingv = &usingv,
    .typev = &typev,
    .funcv = &funcv
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

  // TODO: Verify the mdefn has everything it should according to its interface.
  return true;
}
