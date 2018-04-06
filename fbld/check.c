// check.c --
//   This file implements routines for checking fbld module declarations and
//   definitions.

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_start, va_end, vfprintf
#include <stdlib.h>   // for NULL
#include <stdio.h>    // for FILE, fprintf, stderr
#include <string.h>   // for strcmp

#include "fbld.h"

#define UNREACHABLE(x) assert(false && x)

// SVar --
//   Mapping from variable name to resolved static parameter.
typedef struct SVar {
  FbldName* name;
  FbldR* var;
  struct SVar* next;
} SVar;

// DeclProgress --
//   The current progress of checking a prototype or declaration.
typedef enum {
  DP_NEW,       // The declaration has not yet started being checked.
  DP_PENDING,   // Checking is currently in progress.
  DP_SUCCESS,   // Check succeeded.
  DP_FAILED     // Check failed.
} DeclProgress;

// DeclStatus --
//   Status of checking a declaration.
//
// Fiels:
//   params - The progress of checking the params.
//   proto - The progress of checking the proto.
//   decl - The progress of checking the full declaration.
typedef struct {
  DeclProgress params;
  DeclProgress proto;
  DeclProgress decl;
} DeclStatus;

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
//   svars - List of static parameters in scope.
//   decl_status - Map from prgm declv index to the status of that
//                 declaration.
typedef struct Env {
  struct Env* parent;
  FbldQRef* mref;
  FbldInterf* interf;
  FbldProgram* prgm;
  SVar* svars;
  DeclStatus* decl_status;
} Env;

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

static void Require(bool x, bool* success);
static void ReportError(const char* format, FbldLoc* loc, ...);

static bool IsAbstract(FbldQRef* type, FbldQRef* mref);
static FbldQRef* DeclQRef(FblcArena* arena, FbldQRef* mref, FbldInterf* interf, FbldDecl* decl);
static FbldR* ResolveQRef(FblcArena* arena, Env* env, FbldQRef* qref);
static bool CheckPartialQRef(FblcArena* arena, Env* env, FbldQRef* qref);
static bool CheckQRef(FblcArena* arena, Env* env, FbldQRef* qref);
static bool CheckEnv(FblcArena* arena, Env* env);
static bool CheckInterf(FblcArena* arena, Env* env, FbldInterf* interf);
static bool CheckModule(FblcArena* arena, Env* env, FbldModule* module);
static bool NotRedefinedHelper(FbldName* redef, FbldName* prev);
static bool NotRedefined(Env* env, FbldName* name);
static void CheckDecl(FblcArena* arena, Env* env, FbldDecl* decl, DeclStatus* status);
static bool CheckType(FblcArena* arena, Env* env, FbldQRef* qref);
static bool CheckTypeInProto(FblcArena* arena, Env* env, FbldQRef* proto, FbldQRef* qref);
static Vars* CheckArgV(FblcArena* arena, Env* env, FbldQRef* proto, FbldArgV* argv, Vars* vars, bool* success);
static FbldQRef* CheckExpr(FblcArena* arena, Env* env, Vars* vars, FbldExpr* expr);
static FbldQRef* CheckActn(FblcArena* arena, Env* env, Vars* vars, Ports* ports, FbldActn* actn);
static bool CheckTypesMatch(FbldLoc* loc, FbldQRef* expected, FbldQRef* actual);
static bool CheckValue(FblcArena* arena, Env* env, FbldValue* value);

static bool CheckProtoArgs(FblcArena* arena, FbldQRef* qref, FbldArgV* args, FbldQRef* proto_src, FbldArgV* proto_args, bool match_names);
static bool CheckProto(FblcArena* arena, Env* env, FbldQRef* qref, FbldQRef* proto_src, FbldDecl* proto);
static bool EnsureParams(FblcArena* arena, Env* env, FbldDecl* decl);
static bool EnsureProto(FblcArena* arena, Env* env, FbldDecl* decl);
static bool EnsureDecl(FblcArena* arena, Env* env, FbldLoc* loc, FbldDecl* decl);

// Require --
//   Convenience function for requiring some condition to hold.
//
// Inputs:
//   x - required condition.
//   success - variable set to false if x does not hold.
//
// Results:
//   None.
//
// Side effects:
//   If x is false, success is set to false.
static void Require(bool x, bool* success)
{
  if (!x) {
    *success = false;
  }
}

// ReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
static void ReportError(const char* format, FbldLoc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// IsAbstract --
//   Check whether the given type is abstract when accessed from the context
//   of mref.
//
// Inputs:
//   type - Resolved qref for the type to check.
//   mref - Resolved qref for the context from which the type is accessed.
//
// Results:
//   True if the type is considered abstract in the context of mref, false
//   otherwise.
//
// Side effects:
//   None.
static bool IsAbstract(FbldQRef* type, FbldQRef* mref)
{
  assert(type->r != NULL);
  assert(type->r->decl != NULL);

  // If the type has abstract kind, then it must be a proto from an interface
  // or static parameter. In this case it certainly is abstract.
  FbldType* type_decl = (FbldType*)type->r->decl;
  if (type_decl->kind == FBLD_ABSTRACT_KIND) {
    return true;
  }

  // If the kind isn't abstract, then the type can only be abstract if the
  // declaration is marked abstract.
  if (type->r->decl->access != FBLD_ABSTRACT_ACCESS) {
    return false;
  }

  // If the type is defined in the global namespace, then everyone has access
  // to it.
  if (type->r->mref == NULL) {
    return false;
  }

  // Otherwise, if we find the parent module of the type is an ancestor of the
  // current context, then we have access to the type.
  while (mref != NULL) {
    if (FbldQRefsEqual(mref, type->r->mref)) {
      return false;
    }
    mref = mref->r->mref;
  }

  return true;
}

// DeclQRef --
//   Create a non-parameter qref that refers to the given declaration.
//
// Inputs:
//   arena - Arena to use for allocations.
//   mref - Reference for the module the declaration is defined in.
//   interf - The interface this decl is declared in, if at all.
//   decl - The decl to create a qref to.
//
// Result:
//   A qref that refers to the given declaration.
//
// Side Effects:
//   Allocates a new qref.
static FbldQRef* DeclQRef(FblcArena* arena, FbldQRef* mref, FbldInterf* interf, FbldDecl* decl)
{
  FbldR* r = FBLC_ALLOC(arena, FbldR);
  r->decl = decl;
  r->mref = mref;
  r->param = interf != NULL;
  r->interf = interf;

  FbldQRef* qref = FBLC_ALLOC(arena, FbldQRef);
  qref->name = decl->name;

  qref->paramv = FBLC_ALLOC(arena, FbldQRefV);
  FblcVectorInit(arena, *qref->paramv);
  for (size_t i = 0; i < decl->paramv->size; ++i) {
    FblcVectorAppend(arena, *qref->paramv, DeclQRef(arena, mref, NULL, decl->paramv->xs[i]));
  }

  qref->mref = mref;
  qref->r = r;
  return qref;
}

// ResolveQRef --
//   Resolve an unresolved qualified reference.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The current environment.
//   qref - The unresolved qref to resolve.
//
// Returns:
//   The resolution info for the qref, or NULL if it failed to resolve.
//
// Side effects:
//   Prints a message to stderr if the qref fails to resolve.
static FbldR* ResolveQRef(FblcArena* arena, Env* env, FbldQRef* qref)
{
  // Entity explicitly qualified?
  if (qref->mref != NULL) {
    // The entity is of the form foo@bar, and comes from a module bar in the
    // local scope. Resolve the module bar and find its interface.
    if (!CheckQRef(arena, env, qref->mref)) {
      return NULL;
    }

    assert(qref->mref->r->decl != NULL);
    if (!EnsureProto(arena, env, qref->mref->r->decl)) {
      return NULL;
    }

    if (qref->mref->r->decl->tag != FBLD_MODULE_DECL) {
      ReportError("%s does not refer to a module\n", qref->mref->name->loc, qref->mref->name->name);
      return NULL;
    }

    FbldModule* module = (FbldModule*)qref->mref->r->decl;
    if (module->iref == NULL) {
      // The module does not have an explicit interface.
      // Look for the entity declaration in the body of the module.
      assert(module->body != NULL);
      for (size_t i = 0; i < module->body->declv->size; ++i) {
        FbldDecl* decl = module->body->declv->xs[i];
        if (decl->access != FBLD_PRIVATE_ACCESS && FbldNamesEqual(qref->name->name, decl->name->name)) {
          FbldR* r = FBLC_ALLOC(arena, FbldR);
          r->decl = decl;
          r->mref = qref->mref;
          r->param = false;
          r->interf = NULL;
          return r;
        }
      }

      ReportError("%s not found in %s\n",
          qref->name->loc, qref->name->name, qref->mref->name->name);
      return NULL;
    }

    // TODO: FbldImportQRef the interface?
    assert(module->iref->r != NULL);

    // Bail out now if the interface failed to check. There would already
    // have been an error message reported for that failure.
    if (module->iref->r->decl == NULL) {
      return NULL;
    }

    assert(module->iref->r->decl->tag == FBLD_INTERF_DECL);
    FbldInterf* interf = (FbldInterf*)module->iref->r->decl;

    // Look for the entity declaration in the interface.
    for (size_t i = 0; i < interf->body->declv->size; ++i) {
      if (FbldNamesEqual(qref->name->name, interf->body->declv->xs[i]->name->name)) {
        FbldR* r = FBLC_ALLOC(arena, FbldR);
        r->decl = interf->body->declv->xs[i];
        r->mref = qref->mref;
        r->param = false;
        r->interf = interf;
        return r;
      }
    }

    ReportError("%s not found in interface for %s\n",
        qref->name->loc, qref->name->name, qref->mref->name->name);
    return NULL;
  }

  // Entity imported?
  for (size_t i = 0; i < env->prgm->importv->size; ++i) {
    FbldImport* import = env->prgm->importv->xs[i];
    for (size_t j = 0; j < import->itemv->size; ++j) {
      if (FbldNamesEqual(qref->name->name, import->itemv->xs[j]->dest->name)) {
        assert(import->itemv->xs[j]->source->r != NULL);
        return import->itemv->xs[j]->source->r;
      } 
    }
  }

  // Entity defined locally?
  for (size_t i = 0; i < env->prgm->declv->size; ++i) {
    FbldDecl* decl = env->prgm->declv->xs[i];
    if (FbldNamesEqual(qref->name->name, decl->name->name)) {
      if (decl->alias == NULL) {
        FbldR* r = FBLC_ALLOC(arena, FbldR);
        r->decl = decl;
        r->mref = env->mref;
        r->param = env->interf != NULL;
        r->interf = env->interf;
        return r;
      } else {
        if (!EnsureDecl(arena, env, qref->name->loc, decl)) {
          return NULL;
        }
        assert(decl->alias->r != NULL);
        assert(decl->alias->r->decl != NULL);
        return decl->alias->r;
      }
    }
  }

  // Check whether the name refers to a static parameter in scope.
  for (SVar* svar = env->svars; svar != NULL; svar = svar->next) {
    if (FbldNamesEqual(qref->name->name, svar->name->name)) {
      return svar->var;
    }
  }

  ReportError("%s not defined\n", qref->name->loc, qref->name->name);
  return NULL;
}

// CheckPartialQRef --
//   Check that the given qref is well formed.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   qref - The qref to check.
//
// Returns:
//   true if the qref is well formed, false otherwise.
//
// Side effects:
//   Updates qref.r.* fields based on the result of qref resolution.
//   Prints a message to stderr if the qref is not well formed and has not
//   already been reported as being bad.
static bool CheckPartialQRef(FblcArena* arena, Env* env, FbldQRef* qref)
{
  if (qref->r != NULL) {
    return qref->r->decl != NULL;
  }

  // By default assume the qref fails to resolve. We will overwrite this with
  // something more meaningful once we have successfully resolved the
  // reference and confirmed the qref is well formed.
  qref->r = &FailedR;

  FbldR* r = ResolveQRef(arena, env, qref);
  if (r == NULL || r->decl == NULL) {
    return false;
  }

  if (!EnsureParams(arena, env, r->decl)) {
    return false;
  }

  // Check the parameters we have so far. Not all parameters need be supplied
  // at this point.
  if (qref->paramv->size > r->decl->paramv->size) {
    ReportError("Too many static arguments to %s\n", qref->name->loc, qref->name->name);
    // TODO: Free r
    return false;
  }

  for (size_t i = 0; i < qref->paramv->size; ++i) {
    if (!CheckPartialQRef(arena, env, qref->paramv->xs[i])) {
      // TODO: Free r
      return false;
    }
  }

  qref->r = r;
  for (size_t i = 0; i < qref->paramv->size; ++i) {
    if (!CheckProto(arena, env, qref->paramv->xs[i], qref, r->decl->paramv->xs[i])) {
      // TODO: Free r
      qref->r = &FailedR;
      return false;
    }
  }
  
  return true;
}

// CheckQRef --
//   Check that the given qref is well formed and has all static parameters
//   supplied.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   qref - The qref to check.
//
// Returns:
//   true if the qref is well formed, false otherwise.
//
// Side effects:
//   Updates qref.r.* fields based on the result of qref resolution.
//   Prints a message to stderr if the qref is not well formed and has not
//   already been reported as being bad.
static bool CheckQRef(FblcArena* arena, Env* env, FbldQRef* qref)
{
  if (!CheckPartialQRef(arena, env, qref)) {
    return false;
  }

  if (qref->paramv->size != qref->r->decl->paramv->size) {
    ReportError("Expected %i arguments to %s, but %i provided\n", qref->name->loc, qref->r->decl->paramv->size, qref->name->name,
        qref->paramv->size);
    return false;
  }
  return true;
}

// CheckEnv --
//   Check that the entities from the given environment are well formed.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment to check.
//
// Result:
//   true if the env is well formed, false otherwise.
//
// Side effects:
//   Resolves qrefs.
//   Prints a message to stderr if the environment is not well formed.
static bool CheckEnv(FblcArena* arena, Env* env)
{
  bool success = true;

  DeclStatus decl_status[env->prgm->declv->size];
  for (size_t i = 0; i < env->prgm->declv->size; ++i) {
    decl_status[i].params = DP_NEW;
    decl_status[i].proto = DP_NEW;
    decl_status[i].decl = DP_NEW;
  }
  env->decl_status = decl_status;

  // Check import statements.
  for (size_t import_id = 0; import_id < env->prgm->importv->size; ++import_id) {
    FbldImport* import = env->prgm->importv->xs[import_id];
    for (size_t i = 0; i < import->itemv->size; ++i) {
      Require(NotRedefined(env, import->itemv->xs[i]->dest), &success);
      if (import->mref == NULL) {
        // Import '@' from parent

        // Ensure the imported entity has already been checked.
        FbldQRef* shead = import->itemv->xs[i]->source;
        while (shead->mref != NULL) {
          shead = shead->mref;
        }

        if (!CheckPartialQRef(arena, env->parent, shead)) {
          return false;
        }
        assert(shead->r->decl != NULL);
        if (!EnsureDecl(arena, env->parent, shead->name->loc, shead->r->decl)) {
          return false;
        }

        Require(CheckPartialQRef(arena, env->parent, import->itemv->xs[i]->source), &success);
      } else {
        // Import from local

        // Append the import mref to the back of the source for checking the
        // source.
        FbldQRef* shead = import->itemv->xs[i]->source;
        while (shead->mref != NULL) {
          shead = shead->mref;
        }
        shead->mref = import->mref;
        Require(CheckPartialQRef(arena, env, import->itemv->xs[i]->source), &success);
      }
    }
  }

  for (size_t decl_id = 0; decl_id < env->prgm->declv->size; ++decl_id) {
    FbldDecl* decl = env->prgm->declv->xs[decl_id];
    Require(EnsureDecl(arena, env, decl->name->loc, decl), &success);
  }

  return success;
}

// CheckInterf --
//   Check that the given interf declaration is well formed.
//
// Inputs:
//   arena - Arena to use for allocations.
//   interf - The interface declaration to check.
//
// Result:
//   true if the interface checks out, false otherwise.
//
// Side effects:
//   Resolves qrefs.
//   Prints a message to stderr if the interface declaration is not well
//   formed.
static bool CheckInterf(FblcArena* arena, Env* env, FbldInterf* interf)
{
  FbldR* r = FBLC_ALLOC(arena, FbldR);
  r->decl = &interf->_base;
  r->mref = env->mref;
  r->param = env->interf != NULL;
  r->interf = env->interf;

  FbldQRef* mref = FBLC_ALLOC(arena, FbldQRef);
  mref->name = interf->_base.name;
  mref->paramv = FBLC_ALLOC(arena, FbldQRefV);
  FblcVectorInit(arena, *mref->paramv);
  mref->mref = env->mref;
  mref->r = r;

  Env interf_env = {
    .parent = env,
    .mref = mref,
    .interf = interf,
    .prgm = interf->body,
    .svars = NULL,
    .decl_status = NULL
  };
  return CheckEnv(arena, &interf_env);
}

// CheckModule --
//   Check that the given module definition is well formed.
//
// Inputs:
//   arena - Arena to use for allocations.
//   module - The module definition to check.
//
// Result:
//   true if the module is well formed, false otherwise.
//
// Side effects:
//   Resolves qrefs.
//   Generates an anonymous interface for the module if appropriate.
//   Prints a message to stderr if the module definition is not well
//   formed.
static bool CheckModule(FblcArena* arena, Env* env, FbldModule* module)
{
  if (module->iref != NULL) {
    if (!CheckQRef(arena, env, module->iref)) {
      return false;
    }

    if (module->iref->r->decl->tag != FBLD_INTERF_DECL) {
      ReportError("%s does not refer to an interface\n", module->iref->name->loc, module->iref->name->name);
      return false;
    }
  }

  if (module->_base.alias != NULL) {
    assert(module->body == NULL);
    if (!CheckPartialQRef(arena, env, module->_base.alias)) {
      return false;
    }

    // TODO: is env->mref the right proto_src to use? I don't think so. I
    // think we need a DeclQRef for module.
    if (!CheckProto(arena, env, module->_base.alias, env->mref, &module->_base)) {
      return false;
    }
    return true;
  }

  if (module->body == NULL) {
    return true;
  }

  FbldR* r = FBLC_ALLOC(arena, FbldR);
  r->decl = &module->_base;
  r->mref = env->mref;
  r->param = false;
  r->interf = NULL;

  FbldQRef* mref = FBLC_ALLOC(arena, FbldQRef);
  mref->name = module->_base.name;
  mref->paramv = FBLC_ALLOC(arena, FbldQRefV);
  FblcVectorInit(arena, *mref->paramv);
  mref->mref = env->mref;
  mref->r = r;

  Env module_env = {
    .parent = env,
    .mref = mref,
    .interf = NULL,
    .prgm = module->body,
    .svars = NULL,
    .decl_status = NULL,
  };

  if (!CheckEnv(arena, &module_env)) {
    return false;
  }

  bool success = true;
  if (module->iref != NULL) {
    // Verify the module has everything it should according to its interface.
    assert(module->iref->r->decl->tag == FBLD_INTERF_DECL);
    if (!EnsureDecl(arena, env, module->iref->name->loc, module->iref->r->decl)) {
      return false;
    }
    FbldInterf* interf = (FbldInterf*)module->iref->r->decl;

    for (size_t i = 0; i < interf->body->declv->size; ++i) {
      FbldDecl* decl_i = interf->body->declv->xs[i];
      for (size_t m = 0; m < module->body->declv->size; ++m) {
        FbldDecl* decl_m = module->body->declv->xs[m];
        if (FbldNamesEqual(decl_i->name->name, decl_m->name->name)) {
          // Create a src reference that is as if we had accessed the module's
          // declaration through its interface, rather than directly, with
          // static parameters that match the parameters in the module.
          // TODO: Don't leak src and qref like this.
          FbldQRef* proto_src = DeclQRef(arena, mref, interf, decl_i);
          proto_src->r->param = false;
          FbldQRef* qref = DeclQRef(arena, mref, NULL, decl_m);
          Require(CheckProto(arena, env, qref, proto_src, decl_i), &success);

          FbldType* type_i = (FbldType*)decl_i;
          if (decl_i->tag == FBLD_TYPE_DECL && type_i->kind == FBLD_ABSTRACT_KIND) {
            if (decl_m->access != FBLD_ABSTRACT_ACCESS) {
              ReportError("%s is abstract in the interface.\n", decl_m->name->loc, decl_m->name->name);
              success = false;
            }
          } else if (decl_m->access != FBLD_PUBLIC_ACCESS) {
              ReportError("%s is public in the interface.\n", decl_m->name->loc, decl_m->name->name);
              success = false;
          }

          // Set type_i to NULL to indicate we found the matching type.
          decl_i = NULL;
          break;
        }
      }

      if (decl_i != NULL) {
        ReportError("No implementation found for %s from the interface\n", module->_base.name->loc, decl_i->name->name);
        success = false;
      }
    }

    for (size_t m = 0; m < module->body->declv->size; ++m) {
      FbldDecl* decl_m = module->body->declv->xs[m];
      if (decl_m->access != FBLD_PRIVATE_ACCESS) {
        bool found = false;
        for (size_t i = 0; !found && i < interf->body->declv->size; ++i) {
          FbldDecl* decl_i = interf->body->declv->xs[i];
          found = FbldNamesEqual(decl_i->name->name, decl_m->name->name);
        }
        if (!found) {
          ReportError("%s not declared in interface.\n", decl_m->name->loc, decl_m->name->name);
          success = false;
        }
      }
    }
  }

  return success;
}

// CheckTypesMatch --
//   Check whether the given types match.
//   TODO: Change this function to take a context as input.
//
// Inputs:
//   loc - location to use for error reporting.
//   expected - the expected type.
//   actual - the actual type.
//
// Results:
//   true if the types are valid and match. false otherwise.
//
// Side effects:
//   Prints an error message to stderr if the types don't match. If either
//   type is null, it is assumed an error has already been printed, in which
//   case no additional error message will be reported.
//
// TODO: Is it acceptable for expected and/or actual to be non-NULL failed
// resolved names? Should we require they be one or the other?
static bool CheckTypesMatch(FbldLoc* loc, FbldQRef* expected, FbldQRef* actual)
{
  if (expected == NULL || actual == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return false;
  }

  assert(expected->r != NULL);
  assert(actual->r != NULL);

  if (expected->r->decl == NULL || actual->r->decl == NULL) {
    // Assume a type error has already been reported or will be reported in
    // this case and that additional error messages would not be helpful.
    return false;
  }

  if (!FbldQRefsEqual(expected, actual)) {
    ReportError("Expected type ", loc); FbldPrintQRef(stderr, expected);
    fprintf(stderr, ", but found type "); FbldPrintQRef(stderr, actual);
    fprintf(stderr, "\n");
    return false;
  }
  return true;
}

// CheckType --
//   Check that the given qref refers to a type.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   qref - The qref to check.
//
// Result:
//   true if the qref resolves and refers to a type, false otherwise.
//
// Side effects:
//   Loads program modules as needed to check the type.
//   Resolves qref if necessary.
//   Reports errors to stderr if the entity could not be resolved or does not
//   refer to a type.
static bool CheckType(FblcArena* arena, Env* env, FbldQRef* qref)
{
  if (!CheckQRef(arena, env, qref)) {
    return false;
  }

  if (qref->r->decl->tag != FBLD_TYPE_DECL) {
    ReportError("%s does not refer to a type\n", qref->name->loc, qref->name->name);
    ReportError("(%s defined here)\n", qref->r->decl->name->loc, qref->name->name);
    return false;
  }
  return true;
}

// CheckTypeInProto --
//   Check that the given qref refers to a type and has acceptable access
//   modes given that it is referenced from the given proto.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   proto - The proto that refers to the qref.
//   qref - The qref to check.
//
// Result:
//   true if the qref resolves, refers to a type, and has proper visibility,
//   false otherwise.
//
// Side effects:
//   Resolves qref if necessary.
//   Reports errors to stderr.
static bool CheckTypeInProto(FblcArena* arena, Env* env, FbldQRef* proto, FbldQRef* qref)
{
  if (!CheckType(arena, env, qref)) {
    return false;
  }

  // Confirm that qref has at least as much visibility as proto.
  FbldQRef* qrefv = qref;
  while (qrefv != NULL && qrefv->r->decl->access != FBLD_PRIVATE_ACCESS) {
    qrefv = qrefv->r->mref;
  }

  if (qrefv == NULL) {
    return true;
  }

  FbldQRef* protov = proto;
  while (protov != NULL && protov->r->decl->access != FBLD_PRIVATE_ACCESS) {
    protov = protov->r->mref;
  }

  while (protov != NULL) {
    if (FbldQRefsEqual(protov->r->mref, qrefv->r->mref)) {
      return true;
    }
    protov = protov->r->mref;
  }

  ReportError("%s has less visibility than %s\n", qref->name->loc, qref->name->name, proto->name->name);
  return false;
}

// CheckExpr --
//   Check that the given expression is well formed.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   vars - The list of variables currently in scope.
//   expr - The expression to check.
//
// Results:
//   The type of the expression, or NULL if the expression is not well typed.
//
// Side effects:
//   Prints a message to stderr if the expression is not well typed.
static FbldQRef* CheckExpr(FblcArena* arena, Env* env, Vars* vars, FbldExpr* expr)
{
  switch (expr->tag) {
    case FBLD_VAR_EXPR: {
      FbldVarExpr* var_expr = (FbldVarExpr*)expr;
      for (size_t i = 0; vars != NULL; ++i) {
        if (FbldNamesEqual(vars->name, var_expr->var.name.name)) {
          var_expr->var.id = i;
          return vars->type;
        }
        vars = vars->next;
      }
      ReportError("variable '%s' not defined\n", var_expr->var.name.loc, var_expr->var.name.name);
      return NULL;
    }

    case FBLD_APP_EXPR: {
      bool success = true;
      FbldAppExpr* app_expr = (FbldAppExpr*)expr;

      FbldQRef* arg_types[app_expr->argv->size];
      for (size_t i = 0; i < app_expr->argv->size; ++i) {
        arg_types[i] = CheckExpr(arena, env, vars, app_expr->argv->xs[i]);
        Require(arg_types[i] != NULL, &success);
      }

      Require(CheckQRef(arena, env, app_expr->func), &success);

      FbldQRef* return_type = NULL;
      FbldArgV* argv = NULL;
      if (app_expr->func->r->decl == NULL) {
        return NULL;
      }
      if (!EnsureProto(arena, env, app_expr->func->r->decl)) {
        return NULL;
      }

      if (app_expr->func->r->decl->tag == FBLD_FUNC_DECL) {
        FbldFunc* func = (FbldFunc*)app_expr->func->r->decl;
        argv = func->argv;
        return_type = FbldImportQRef(arena, app_expr->func, func->return_type);
      } else if (app_expr->func->r->decl->tag == FBLD_TYPE_DECL) {
        FbldType* type = (FbldType*)app_expr->func->r->decl;
        if (type->kind != FBLD_STRUCT_KIND || IsAbstract(app_expr->func, env->mref)) {
          ReportError("Cannot do application on type %s.\n", app_expr->func->name->loc, app_expr->func->name->name);
          return NULL;
        }
        argv = type->fieldv;
        return_type = app_expr->func;
      } else {
        ReportError("'%s' does not refer to a type or function.\n", app_expr->func->name->loc, app_expr->func->name->name);
        return NULL;
      }

      if (argv->size == app_expr->argv->size) {
        for (size_t i = 0; i < argv->size; ++i) {
          FbldQRef* expected = FbldImportQRef(arena, app_expr->func, argv->xs[i]->type);
          Require(CheckTypesMatch(app_expr->argv->xs[i]->loc, expected, arg_types[i]), &success);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", app_expr->func->name->loc, argv->size, app_expr->func->name->name, app_expr->argv->size);
        return NULL;
      }
      return success ? return_type : NULL;
    }

    case FBLD_ACCESS_EXPR: {
      FbldAccessExpr* access_expr = (FbldAccessExpr*)expr;
      FbldQRef* qref = CheckExpr(arena, env, vars, access_expr->obj);
      if (qref == NULL || qref->r->decl == NULL) {
        return NULL;
      }
      if (!EnsureProto(arena, env, qref->r->decl)) {
        return NULL;
      }

      if (IsAbstract(qref, env->mref)) {
        ReportError("Cannot access field of abstract type %s\n", access_expr->field.name.loc, qref->name->name);
        return NULL;
      }

      assert(qref->r->decl->tag == FBLD_TYPE_DECL);
      FbldType* type = (FbldType*)qref->r->decl;
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (FbldNamesEqual(access_expr->field.name.name, type->fieldv->xs[i]->name->name)) {
          access_expr->field.id = i;
          return FbldImportQRef(arena, qref, type->fieldv->xs[i]->type);
        }
      }
      ReportError("%s is not a field of type %s\n", access_expr->field.name.loc, access_expr->field.name.name, type->_base.name->name);
      return NULL;
    }

    case FBLD_UNION_EXPR: {
      FbldUnionExpr* union_expr = (FbldUnionExpr*)expr;
      FbldQRef* arg_type = CheckExpr(arena, env, vars, union_expr->arg);
      if (arg_type == NULL) {
        return NULL;
      }
      if (!CheckType(arena, env, union_expr->type)) {
        return NULL;
      }
      if (!EnsureProto(arena, env, union_expr->type->r->decl)) {
        return NULL;
      }

      assert(union_expr->type->r->decl->tag == FBLD_TYPE_DECL);
      FbldType* type_def = (FbldType*)union_expr->type->r->decl;
      if (type_def->kind != FBLD_UNION_KIND || IsAbstract(union_expr->type, env->mref)) {
        ReportError("%s does not refer to a union type.\n", union_expr->type->name->loc, union_expr->type->name->name);
        return NULL;
      }


      for (size_t i = 0; i < type_def->fieldv->size; ++i) {
        if (FbldNamesEqual(union_expr->field.name.name, type_def->fieldv->xs[i]->name->name)) {
          union_expr->field.id = i;
          FbldQRef* expected = FbldImportQRef(arena, union_expr->type, type_def->fieldv->xs[i]->type);
          if (!CheckTypesMatch(union_expr->arg->loc, expected, arg_type)) {
            return NULL;
          }
          return union_expr->type;
        }
      }
      ReportError("%s is not a field of type %s\n", union_expr->field.name.loc, union_expr->field.name.name, union_expr->type->name->name);
      return NULL;
    }

    case FBLD_LET_EXPR: {
      FbldLetExpr* let_expr = (FbldLetExpr*)expr;
      for (Vars* curr = vars; curr != NULL; curr = curr->next) {
        if (FbldNamesEqual(curr->name, let_expr->var->name)) {
          ReportError("Redefinition of variable '%s'\n", let_expr->var->loc, let_expr->var->name);
          return NULL;
        }
      }

      bool success = true;
      Require(CheckType(arena, env, let_expr->type), &success);
      FbldQRef* def_type = CheckExpr(arena, env, vars, let_expr->def);
      Require(def_type != NULL, &success);
      Require(CheckTypesMatch(let_expr->def->loc, let_expr->type, def_type), &success);

      Vars nvars = {
        .type = let_expr->type,
        .name = let_expr->var->name,
        .next = vars
      };
      FbldQRef* body_type = CheckExpr(arena, env, &nvars, let_expr->body);
      return success ? body_type : NULL;
    }

    case FBLD_COND_EXPR: {
      bool success = true;
      FbldCondExpr* cond_expr = (FbldCondExpr*)expr;
      FbldQRef* type = CheckExpr(arena, env, vars, cond_expr->select);
      Require(type != NULL, &success);
      if (type != NULL) {
        assert(type->r->decl->tag == FBLD_TYPE_DECL);
        FbldType* type_def = (FbldType*)type->r->decl;
        if (type_def->kind == FBLD_UNION_KIND && !IsAbstract(type, env->mref)) {
          if (type_def->fieldv->size != cond_expr->argv->size) {
            ReportError("Expected %d arguments, but %d were provided.\n", cond_expr->_base.loc, type_def->fieldv->size, cond_expr->argv->size);
            success = false;
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", cond_expr->select->loc, type_def->_base.name->name);
          success = false;
        }
      }

      assert(cond_expr->argv->size > 0);
      FbldQRef* result_type = CheckExpr(arena, env, vars, cond_expr->argv->xs[0]);
      for (size_t i = 1; i < cond_expr->argv->size; ++i) {
        FbldQRef* arg_type = CheckExpr(arena, env, vars, cond_expr->argv->xs[i]);
        Require(arg_type != NULL, &success);
        Require(CheckTypesMatch(cond_expr->argv->xs[i]->loc, result_type, arg_type), &success);
      }
      return success ? result_type : NULL;
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
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   vars - The list of variables currently in scope.
//   ports - The list of ports currently in scope.
//   actn - The action to check.
//
// Results:
//   The type of the action, or NULL if the action is not well typed.
//
// Side effects:
//   Prints a message to stderr if the action is not well typed.
static FbldQRef* CheckActn(FblcArena* arena, Env* env, Vars* vars, Ports* ports, FbldActn* actn)
{
  switch (actn->tag) {
    case FBLD_EVAL_ACTN: {
      FbldEvalActn* eval_actn = (FbldEvalActn*)actn;
      return CheckExpr(arena, env, vars, eval_actn->arg);
    }

    case FBLD_GET_ACTN: {
      FbldGetActn* get_actn = (FbldGetActn*)actn;
      for (size_t i = 0; ports != NULL; ++i) {
        if (FbldNamesEqual(ports->name, get_actn->port.name.name)) {
          if (ports->polarity == FBLD_GET_POLARITY) {
            get_actn->port.id = i;
            return ports->type;
          } else {
            ReportError("Port '%s' should have get polarity, but has put polarity.\n", get_actn->port.name.loc, get_actn->port.name.name);
            return NULL;
          }
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", get_actn->port.name.loc, get_actn->port.name.name);
      return NULL;
    }

    case FBLD_PUT_ACTN: {
      FbldPutActn* put_actn = (FbldPutActn*)actn;
      FbldQRef* arg_type = CheckExpr(arena, env, vars, put_actn->arg);

      for (size_t i = 0; ports != NULL; ++i) {
        if (FbldNamesEqual(ports->name, put_actn->port.name.name)) {
          if (ports->polarity == FBLD_PUT_POLARITY) {
            put_actn->port.id = i;
            if (!CheckTypesMatch(put_actn->arg->loc, ports->type, arg_type)) {
              return NULL;
            }
            return arg_type != NULL ? ports->type : NULL;
          } else {
            ReportError("Port '%s' should have put polarity, but has get polarity.\n", put_actn->port.name.loc, put_actn->port.name.name);
            return NULL;
          }
        }
        ports = ports->next;
      }
      ReportError("port '%s' not defined.\n", put_actn->port.name.loc, put_actn->port.name.name);
      return NULL;
    }

    case FBLD_COND_ACTN: {
      bool success = true;
      FbldCondActn* cond_actn = (FbldCondActn*)actn;
      FbldQRef* type = CheckExpr(arena, env, vars, cond_actn->select);
      Require(type != NULL, &success);
      if (type != NULL) {
        assert(type->r->decl->tag == FBLD_TYPE_DECL);
        FbldType* type_def = (FbldType*)type->r->decl;
        if (type_def->kind == FBLD_UNION_KIND && !IsAbstract(type, env->mref)) {
          if (type_def->fieldv->size != cond_actn->argv->size) {
            ReportError("Expected %d arguments, but %d were provided.\n", cond_actn->_base.loc, type_def->fieldv->size, cond_actn->argv->size);
            success = false;
          }
        } else {
          ReportError("The condition has type %s, which is not a union type.\n", cond_actn->select->loc, type_def->_base.name->name);
          success = false;
        }
      }

      assert(cond_actn->argv->size > 0);
      FbldQRef* result_type = CheckActn(arena, env, vars, ports, cond_actn->argv->xs[0]);
      for (size_t i = 1; i < cond_actn->argv->size; ++i) {
        FbldQRef* arg_type = CheckActn(arena, env, vars, ports, cond_actn->argv->xs[i]);
        Require(arg_type != NULL, &success);
        Require(CheckTypesMatch(cond_actn->argv->xs[i]->loc, result_type, arg_type), &success);
      }
      return success ? result_type : NULL;
    }

    case FBLD_CALL_ACTN: {
      bool success = true;
      FbldCallActn* call_actn = (FbldCallActn*)actn;

      Ports* port_types[call_actn->portv.size];
      for (size_t i = 0; i < call_actn->portv.size; ++i) {
        port_types[i] = NULL;
        Ports* curr = ports;
        for (size_t id = 0; curr != NULL; ++id) {
          if (FbldNamesEqual(curr->name, call_actn->portv.xs[i].name.name)) {
            call_actn->portv.xs[i].id = id;
            port_types[i] = curr;
            break;
          }
          curr = curr->next;
        }
        if (port_types[i] == NULL) {
          ReportError("Port '%s' not defined.\n", call_actn->portv.xs[i].name.loc, call_actn->portv.xs[i].name.name);
          success = false;
        }
      }

      FbldQRef* arg_types[call_actn->argv->size];
      for (size_t i = 0; i < call_actn->argv->size; ++i) {
        arg_types[i] = CheckExpr(arena, env, vars, call_actn->argv->xs[i]);
        Require(arg_types[i] != NULL, &success);
      }

      if (!CheckQRef(arena, env, call_actn->proc)) {
        return NULL;
      }
      if (!EnsureProto(arena, env, call_actn->proc->r->decl)) {
        return NULL;
      }

      if (call_actn->proc->r->decl->tag != FBLD_PROC_DECL) {
        ReportError("%s does not refer to a proc.\n", call_actn->proc->name->loc, call_actn->proc->name->name);
        return NULL;
      }

      FbldProc* proc = (FbldProc*)call_actn->proc->r->decl;
      if (proc->portv->size == call_actn->portv.size) {
        for (size_t i = 0; i < proc->portv->size; ++i) {
          if (port_types[i] != NULL) {
            if (port_types[i]->polarity != proc->portv->xs[i].polarity) {
                ReportError("Port '%s' has wrong polarity. Expected '%s', but found '%s'.\n",
                    call_actn->portv.xs[i].name.loc, call_actn->portv.xs[i].name.name,
                    proc->portv->xs[i].polarity == FBLD_PUT_POLARITY ? "put" : "get",
                    port_types[i]->polarity == FBLD_PUT_POLARITY ? "put" : "get");
                success = false;
            }
            FbldQRef* expected = FbldImportQRef(arena, call_actn->proc, proc->portv->xs[i].type);
            Require(CheckTypesMatch(call_actn->portv.xs[i].name.loc, expected, port_types[i]->type), &success);
          }
        }
      } else {
        ReportError("Expected %d port arguments to %s, but %d were provided.\n", call_actn->proc->name->loc, proc->portv->size, call_actn->proc->name->name, call_actn->portv.size);
        success = false;
      }

      if (proc->argv->size == call_actn->argv->size) {
        for (size_t i = 0; i < call_actn->argv->size; ++i) {
          FbldQRef* expected = FbldImportQRef(arena, call_actn->proc, proc->argv->xs[i]->type);
          Require(CheckTypesMatch(call_actn->argv->xs[i]->loc, expected, arg_types[i]), &success);
        }
      } else {
        ReportError("Expected %d arguments to %s, but %d were provided.\n", call_actn->proc->name->loc, proc->argv->size, call_actn->proc->name->name, call_actn->argv->size);
        success = false;
      }
      return success ? FbldImportQRef(arena, call_actn->proc, proc->return_type) : NULL;
    }

    case FBLD_LINK_ACTN: {
      bool success = true;
      FbldLinkActn* link_actn = (FbldLinkActn*)actn;
      CheckType(arena, env, link_actn->type);
      for (Ports* curr = ports; curr != NULL; curr = curr->next) {
        if (FbldNamesEqual(curr->name, link_actn->get->name)) {
          ReportError("Redefinition of port '%s'\n", link_actn->get->loc, link_actn->get->name);
          success = false;
        } else if (FbldNamesEqual(curr->name, link_actn->put->name)) {
          ReportError("Redefinition of port '%s'\n", link_actn->put->loc, link_actn->put->name);
          success = false;
        }
      }

      if (FbldNamesEqual(link_actn->put->name, link_actn->get->name)) {
        ReportError("Redefinition of port '%s'\n", link_actn->get->loc, link_actn->get->name);
        success = false;
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
      FbldQRef* body_type = CheckActn(arena, env, vars, &putport, link_actn->body);
      return success ? body_type : NULL;
    }

    case FBLD_EXEC_ACTN: {
      FbldExecActn* exec_actn = (FbldExecActn*)actn;

      bool success = true;
      Vars vars_data[exec_actn->execv->size];
      Vars* nvars = vars;
      for (size_t i = 0; i < exec_actn->execv->size; ++i) {
        FbldExec* exec = exec_actn->execv->xs + i;
        Require(CheckType(arena, env, exec->type), &success);
        FbldQRef* def_type = CheckActn(arena, env, vars, ports, exec->actn);
        Require(def_type != NULL, &success);
        Require(CheckTypesMatch(exec->actn->loc, exec->type, def_type), &success);
        vars_data[i].type = exec->type;
        vars_data[i].name = exec->name->name;
        vars_data[i].next = nvars;
        nvars = vars_data + i;
      }
      FbldQRef* body_type = CheckActn(arena, env, nvars, ports, exec_actn->body);
      return success ? body_type : NULL;
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
//   arena - Arena to use for allocations.
//   env - The environment for type checking.
//   proto - The proto that refers to the args.
//   argv - The vector of args to check.
//   vars - Space for argv->size vars to fill in based on the given args.
//   success - Set to false on error.
//
// Result:
//   A pointer into vars with the variable scope implied by the arguments.
//
// Side effects:
//   Fills in elements of vars.
//   Loads program modules as needed to check the arguments. In case
//   there is a problem, reports errors to stderr and sets success to false.
static Vars* CheckArgV(FblcArena* arena, Env* env, FbldQRef* proto, FbldArgV* argv, Vars* vars, bool* success)
{
  Vars* next = NULL;
  for (size_t arg_id = 0; arg_id < argv->size; ++arg_id) {
    FbldArg* arg = argv->xs[arg_id];

    // Check whether an argument has already been declared with the same name.
    for (size_t i = 0; i < arg_id; ++i) {
      if (FbldNamesEqual(arg->name->name, argv->xs[i]->name->name)) {
        ReportError("Redefinition of %s\n", arg->name->loc, arg->name->name);
        *success = false;
        break;
      }
    }
    Require(CheckTypeInProto(arena, env, proto, arg->type), success);

    vars[arg_id].name = arg->name->name;
    vars[arg_id].type = CheckType(arena, env, arg->type) ? arg->type : NULL;
    Require(vars[arg_id].type != NULL, success);
    vars[arg_id].next = next;
    arg->type = vars[arg_id].type;
    next = vars + arg_id;
  }
  return next;
}

// NotRedefinedHelper --
//   Helper function for NotRedefined. Checks whether the two names are for
//   the same entity definition.
//
// Inputs:
//   redef - The name of a potentially redefined entity.
//   prev - The name of a potentially previous entity that has the same name.
//
// Results:
//   true if redef and prev are names of the same entity, false otherwise.
//
// Side effects:
//   Reports an error in case redef and prev are names from different entity
//   definitions.
static bool NotRedefinedHelper(FbldName* redef, FbldName* prev)
{
  // This function is only intended to be called after we have checked the two
  // names are equal (as opposed to from the same entity definition).
  assert(FbldNamesEqual(redef->name, prev->name));

  if (redef == prev) {
    return true;
  }

  assert(strcmp(redef->loc->source, prev->loc->source) != 0
      || redef->loc->line != prev->loc->line
      || redef->loc->col != prev->loc->col);
  ReportError("redefinition of %s\n", redef->loc, redef->name);
  ReportError("previous definition was here\n", prev->loc);
  return false;
}

// NotRedefined --
//   Ensure that an entity has not already been defined with the given name in
//   the environment.
//
// Inputs:
//   env - The environment to check if the name is redefined in.
//   name - The name to check.
//
// Results:
//   true if the name has not already been defined in the environment, false
//   otherwise.
//
// Side effects:
//   Reports an error if the name is already defined.
static bool NotRedefined(Env* env, FbldName* name)
{
  // We use pointer equality on the FbldName to check whether the first
  // definition of the entity in the environment has the given name.
  for (size_t import_id = 0; import_id < env->prgm->importv->size; ++import_id) {
    FbldImport* import = env->prgm->importv->xs[import_id];
    for (size_t i = 0; i < import->itemv->size; ++i) {
      FbldName* dest = import->itemv->xs[i]->dest;
      if (FbldNamesEqual(name->name, dest->name)) {
        return NotRedefinedHelper(name, dest);
      }
    }
  }

  for (size_t decl_id = 0; decl_id < env->prgm->declv->size; ++decl_id) {
    FbldName* decl = env->prgm->declv->xs[decl_id]->name;
    if (FbldNamesEqual(name->name, decl->name)) {
      return NotRedefinedHelper(name, decl);
    }
  }

  for (SVar* svar = env->svars; svar != NULL; svar = svar->next) {
    if (FbldNamesEqual(name->name, svar->name->name)) {
      return NotRedefinedHelper(name, svar->name);
    }
  }

  return true;
}

// CheckDecl --
//   Check that the given declaration is well formed and well typed.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The current environment.
//   decl - The declaration to check.
//   status - Updated with progress and status of proto and decl checking.
//
// Results:
//   None.
//
// Side effects:
//   Prints error messages to stderr if there are any problems.
//   Updates status as progress is made with status updates.
static void CheckDecl(FblcArena* arena, Env* env, FbldDecl* decl, DeclStatus* status)
{
  bool success = true;
  assert(status->params == DP_NEW);
  assert(status->proto == DP_NEW);
  assert(status->decl == DP_NEW);
  status->params = DP_PENDING;
  status->proto = DP_PENDING;
  status->decl = DP_PENDING;

  Require(NotRedefined(env, decl->name), &success);

  // Check the static parameters and add them to the environment.
  SVar* svars_in = env->svars;
  SVar svars_data[decl->paramv->size];
  for (size_t i = 0; i < decl->paramv->size; ++i) {
    DeclStatus status = {
      .proto = DP_NEW,
      .decl = DP_NEW,
    };
    CheckDecl(arena, env, decl->paramv->xs[i], &status);
    assert(status.proto == status.decl);
    Require(status.proto == DP_SUCCESS, &success);

    FbldR* r = FBLC_ALLOC(arena, FbldR);
    r->decl = decl->paramv->xs[i];
    r->mref = env->mref;
    r->param = true;
    r->interf = NULL;
    svars_data[i].name = decl->paramv->xs[i]->name;
    svars_data[i].var = r;
    svars_data[i].next = env->svars;
    env->svars = svars_data + i;
  }

  status->params = success ? DP_SUCCESS : DP_FAILED;

  // TODO: Don't leak proto.
  FbldQRef* proto = DeclQRef(arena, env->mref, env->interf, decl);
  switch (decl->tag) {
    case FBLD_TYPE_DECL: {
      FbldType* type = (FbldType*)decl;
      assert(type->kind != FBLD_UNION_KIND || type->fieldv->size > 0);
      assert(type->kind != FBLD_ABSTRACT_KIND || type->fieldv == NULL);

      if (type->fieldv != NULL) {
        Vars unused[type->fieldv->size];
        CheckArgV(arena, env, proto, type->fieldv, unused, &success);
      }

      status->proto = success ? DP_SUCCESS : DP_FAILED;
      status->decl = success ? DP_SUCCESS : DP_FAILED;
      break;
    }

    case FBLD_FUNC_DECL: {
      FbldFunc* func = (FbldFunc*)decl;
      Vars vars_data[func->argv->size];
      Vars* vars = CheckArgV(arena, env, proto, func->argv, vars_data, &success);
      Require(CheckTypeInProto(arena, env, proto, func->return_type), &success);

      status->proto = success ? DP_SUCCESS : DP_FAILED;

      if (func->body != NULL) {
        FbldQRef* body_type = CheckExpr(arena, env, vars, func->body);
        Require(body_type != NULL, &success);
        Require(CheckTypesMatch(func->body->loc, func->return_type, body_type), &success);
      }

      status->decl = success ? DP_SUCCESS : DP_FAILED;
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
            ReportError("Redefinition of port '%s'\n", port->name->loc, port->name->name);
            success = false;
          }
        }

        ports_data[port_id].name = port->name->name;
        ports_data[port_id].type = CheckTypeInProto(arena, env, proto, port->type) ? port->type : NULL;
        Require(ports_data[port_id].type != NULL, &success);
        ports_data[port_id].polarity = port->polarity;
        ports_data[port_id].next = ports;
        ports = ports_data + port_id;
      }

      Vars vars_data[proc->argv->size];
      Vars* vars = CheckArgV(arena, env, proto, proc->argv, vars_data, &success);
      Require(CheckTypeInProto(arena, env, proto, proc->return_type), &success);

      status->proto = success ? DP_SUCCESS : DP_FAILED;

      if (proc->body != NULL) {
        FbldQRef* body_type = CheckActn(arena, env, vars, ports, proc->body);
        Require(body_type != NULL, &success);
        Require(CheckTypesMatch(proc->body->loc, proc->return_type, body_type), &success);
      }

      status->decl = success ? DP_SUCCESS : DP_FAILED;
      break;
    }

    case FBLD_INTERF_DECL: {
      // TODO: Verify the visiblity of the entities referred to in the interf
      FbldInterf* interf = (FbldInterf*)decl;
      Require(CheckInterf(arena, env, interf), &success);

      status->proto = success ? DP_SUCCESS : DP_FAILED;
      status->decl = success ? DP_SUCCESS : DP_FAILED;
      break;
    }

    case FBLD_MODULE_DECL: {
      // TODO: Verify the visiblity of the entities referred to in the interf
      FbldModule* module = (FbldModule*)decl;
      Require(CheckModule(arena, env, module), &success);

      status->proto = success ? DP_SUCCESS : DP_FAILED;
      status->decl = success ? DP_SUCCESS : DP_FAILED;
      break;
    }

    default:
      UNREACHABLE("Invalid decl tag");
  }

  env->svars = svars_in;

  assert(status->proto != DP_NEW && status->proto != DP_PENDING);
  assert(status->decl != DP_NEW && status->decl != DP_PENDING);
}

// FbldCheckQRef -- see fblcs.h for documentation.
bool FbldCheckQRef(FblcArena* arena, FbldProgram* prgm, FbldQRef* qref)
{
  Env env = {
    .parent = NULL,
    .mref = NULL,
    .interf = NULL,
    .prgm = prgm,
    .svars = NULL,
    .decl_status = NULL,
  };

  return CheckQRef(arena, &env, qref);
}

// CheckProtoArgs --
//   Check whether the args from a declaration match the prototype's args.
//
// Inputs:
//   arena - Arena to use for allocations.
//   qref - The qref where the args came from.
//   args - The args to check.
//   proto_src - The src where the proto_args are from.
//   proto_args - The args from the prototype.
//   match_names - Whether the argument names must match as well as the types.
//
// Result:
//   True on success, false on error.
//
// Side effects:
//   Prints an error message if the arguments don't match the prototype
//   arguments.
static bool CheckProtoArgs(FblcArena* arena, FbldQRef* qref, FbldArgV* args, FbldQRef* proto_src, FbldArgV* proto_args, bool match_names)
{
  if (proto_args->size != args->size) {
    ReportError("Wrong number of args, expected %i but found %i\n",
        qref->name->loc, proto_args->size, args->size);
    return false;
  }

  bool success = true;
  for (size_t i = 0; i < proto_args->size && i < args->size; ++i) {
    Require(CheckTypesMatch(qref->name->loc, FbldImportQRef(arena, proto_src, proto_args->xs[i]->type), FbldImportQRef(arena, qref, args->xs[i]->type)), &success);
    if (match_names && !FbldNamesEqual(proto_args->xs[i]->name->name, args->xs[i]->name->name)) {
      ReportError("Argument name %s does not match prototype name %s\n",
          args->xs[i]->name->loc, args->xs[i]->name->name,
          proto_args->xs[i]->name->name);
      success = false;
    }
  }
  return success;
}

// CheckProto --
//   Check that a qref matches the given prototype.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The current environment.
//   qref - The qref whose proto to check.
//   proto_src - The src referring to the proto to check against.
//   proto - The expected prototype of qref.
//
// Results:
//   true if qref matches the given prototype, false otherwise.
//
// Side effects:
//   Prints a message to stderr if the prototype doesn't match.
//
static bool CheckProto(FblcArena* arena, Env* env, FbldQRef* qref, FbldQRef* proto_src, FbldDecl* proto)
{
  assert(qref->r != NULL);
  assert(qref->r->decl != NULL);
  FbldDecl* decl = qref->r->decl;

  if (proto->tag != decl->tag) {
    ReportError("%s does not match required prototype\n", qref->name->loc, qref->name->name);
    ReportError("Required prototype here.\n", proto->name->loc, proto->name->name);
    return false;
  }

  if (!EnsureProto(arena, env, proto)) {
    return false;
  }

  bool success = true;
  switch (proto->tag) {
    case FBLD_TYPE_DECL: {
      FbldType* type_proto = (FbldType*)proto;
      FbldType* type = (FbldType*)decl;
      switch (type_proto->kind) {
        case FBLD_STRUCT_KIND: {
          if (type->kind != FBLD_STRUCT_KIND) {
            ReportError("%s does not refer to a struct type as required\n", qref->name->loc, qref->name->name);
            return false;
          }
        } break;

        case FBLD_UNION_KIND: {
          if (type->kind != FBLD_UNION_KIND) {
            ReportError("%s does not refer to a union type as required\n", qref->name->loc, qref->name->name);
            return false;
          }
        } break;

        case FBLD_ABSTRACT_KIND: {
          // Nothing to check.
        } break;
      }

      if (type_proto->kind != FBLD_ABSTRACT_KIND) {
        Require(CheckProtoArgs(arena, qref, type->fieldv, proto_src, type_proto->fieldv, true), &success);
      }

      break;
    }

    case FBLD_FUNC_DECL: {
      FbldFunc* func_proto = (FbldFunc*)proto;
      FbldFunc* func = (FbldFunc*)decl;
      Require(CheckProtoArgs(arena, qref, func->argv, proto_src, func_proto->argv, false), &success);
      Require(CheckTypesMatch(qref->name->loc, FbldImportQRef(arena, proto_src, func_proto->return_type), FbldImportQRef(arena, qref, func->return_type)), &success);
      break;
    }

    case FBLD_PROC_DECL: {
      FbldProc* proc_proto = (FbldProc*)proto;
      FbldProc* proc = (FbldProc*)decl;
      if (proc_proto->portv->size != proc->portv->size) {
        ReportError("Process %s does not satisfy the required prototype: expectd %i ports but found %i\n", qref->name->loc, qref->name->name,
            proc_proto->portv->size, proc->portv->size);
        return false;
      }
    
      for (size_t i = 0; i < proc_proto->portv->size && i < proc->portv->size; ++i) {
        FbldPort* proto_port = proc_proto->portv->xs + i;
        FbldPort* port = proc->portv->xs + i;
        Require(CheckTypesMatch(qref->name->loc, FbldImportQRef(arena, proto_src, proto_port->type), FbldImportQRef(arena, qref, port->type)), &success);
    
        if (!FbldNamesEqual(proto_port->name->name, port->name->name)) {
          ReportError("Expected name %s, but found name %s\n", port->name->loc,
              proto_port->name->name, port->name->name);
          return false;
        }
    
        if (proto_port->polarity != port->polarity) {
          ReportError("Expected opposite polarity", port->name->loc);
          return false;
        }
      }
    
      Require(CheckProtoArgs(arena, qref, proc->argv, proto_src, proc_proto->argv, false), &success);
      Require(CheckTypesMatch(proc->return_type->name->loc, FbldImportQRef(arena, proto_src, proc_proto->return_type), FbldImportQRef(arena, qref, proc->return_type)), &success);
      break;
    }

    case FBLD_INTERF_DECL: {
      // TODO: Check interface declarations match.
      return true;
    }

    case FBLD_MODULE_DECL: {
      // TODO: Check module declarations match.
      return true;
    }

    default: {
      UNREACHABLE("Invalid decl kind");
      return false;
    }
  }

  if (!success) {
    // TODO: Print expected and actual prototypes.
    ReportError("%s does not have required prototype\n", qref->name->loc, qref->name->name);
    return false;
  }

  return true;
}

// EnsureParams --
//  Ensure that the static parameters of the given declaration have been checked, or
//  check it if necessary and it is a local declaration.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment to resolve from. Static parameters in the
//         environment will be ignored when doing resolution.
//   decl - The declaration to ensure has been checked.
//
// Results:
//   true if the declaration static params checked out fine, false otherwise.
//
// Side effects:
//   Checks the declaration if necessary and possible.
//   Prints an error message to stderr in case of error.
static bool EnsureParams(FblcArena* arena, Env* env, FbldDecl* decl)
{
  if (env->decl_status != NULL) {
    for (size_t i = 0; i < env->prgm->declv->size; ++i) {
      if (env->prgm->declv->xs[i] == decl) {
        DeclStatus* status = env->decl_status + i;
        if (status->params == DP_NEW) {
          SVar* svars = env->svars;
          env->svars = NULL;
          CheckDecl(arena, env, decl, status);
          env->svars = svars;
        } else if (status->params == DP_PENDING) {
          // TODO: Use the location of the caller, not the declaration itself.
          ReportError("Recursive params dependency detected involving %s\n",
              decl->name->loc, decl->name->name);
          return false;
        }
        return status->params == DP_SUCCESS;
      }
    }
  }

  // Non-local declarations must already have been checked and succeeded.
  return true;
}

// EnsureProto --
//  Ensure that the prototype of the given declaration has been checked, or
//  check it if necessary and it is a local declaration.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment to resolve from. Static parameters in the
//         environment will be ignored when doing resolution.
//   decl - The declaration to ensure has been checked.
//
// Results:
//   true if the declaration prototype checked out fine, false otherwise.
//
// Side effects:
//   Checks the declaration if necessary and possible.
//   Prints an error message to stderr in case of error.
static bool EnsureProto(FblcArena* arena, Env* env, FbldDecl* decl)
{
  if (env->decl_status != NULL) {
    for (size_t i = 0; i < env->prgm->declv->size; ++i) {
      if (env->prgm->declv->xs[i] == decl) {
        DeclStatus* status = env->decl_status + i;
        if (status->proto == DP_NEW) {
          SVar* svars = env->svars;
          env->svars = NULL;
          CheckDecl(arena, env, decl, status);
          env->svars = svars;
        } else if (status->proto == DP_PENDING) {
          // TODO: Is this possible? How?
          // TODO: Use the location of the caller, not the declaration itself.
          ReportError("Recursive proto dependency detected involving %s\n",
              decl->name->loc, decl->name->name);
          return false;
        }
        return status->proto == DP_SUCCESS;
      }
    }
  }

  // Non-local declarations must already have been checked and succeeded.
  return true;
}

// EnsureDecl --
//  Ensure that the full given declaration has been checked, or
//  check it if necessary and it is a local declaration.
//
// Inputs:
//   arena - Arena to use for allocations.
//   env - The environment to resolve from. Static parameters in the
//         environment will be ignored when doing resolution.
//   loc - The location where the decl is required, for error reporting
//         purposes.
//   decl - The declaration to ensure has been checked.
//
// Results:
//   true if the full declaration checked out fine, false otherwise.
//
// Side effects:
//   Checks the declaration if necessary and possible.
//   Prints an error message to stderr in case of error.
static bool EnsureDecl(FblcArena* arena, Env* env, FbldLoc* loc, FbldDecl* decl)
{
  if (env->decl_status != NULL) {
    for (size_t i = 0; i < env->prgm->declv->size; ++i) {
      if (env->prgm->declv->xs[i] == decl) {
        DeclStatus* status = env->decl_status + i;
        if (status->decl == DP_NEW) {
          SVar* svars = env->svars;
          env->svars = NULL;
          CheckDecl(arena, env, decl, status);
          env->svars = svars;
        } else if (status->decl == DP_PENDING) {
          ReportError("Recursive dependency through %s not allowed\n", loc, decl->name->name);
          return false;
        }
        return status->decl == DP_SUCCESS;
      }
    }
  }

  // Non-local declarations must already have been checked.
  return true;
}

// FbldCheckProgram -- see documentation in fbld.h
bool FbldCheckProgram(FblcArena* arena, FbldProgram* prgm)
{
  Env env = {
    .parent = NULL,
    .mref = NULL,
    .interf = NULL,
    .prgm = prgm,
    .svars = NULL,
    .decl_status = NULL,
  };
  return CheckEnv(arena, &env);
}

// CheckValue --
//   Check that the value is well formed in the given context.
//
// Inputs:
//   arena - Arena to use for allocations.
//   value - The value to check.
//
// Results:
//   true if the value is well formed, false otherwise.
//
// Side effects:
//   Resolves references in the value.
//   Prints a message to stderr if the value is not well formed.
static bool CheckValue(FblcArena* arena, Env* env, FbldValue* value)
{
  if (!CheckType(arena, env, value->type)) {
    return false;
  }

  assert(value->type->r->decl->tag == FBLD_TYPE_DECL);
  FbldType* type = (FbldType*)value->type->r->decl;

  switch (value->kind) {
    case FBLD_STRUCT_KIND: {
      for (size_t i = 0; i < type->fieldv->size; ++i) {
        if (!CheckValue(arena, env, value->fieldv->xs[i])) {
          return false;
        }
      }

      // TODO: check that the types of the fields match what is expected.
      return true;
    }

    case FBLD_UNION_KIND: {
      if (!CheckValue(arena, env, value->fieldv->xs[0])) {
        return false;
      }

      // TODO: check that the type of the argument matches what is expected.
      return true;
    }

    case FBLD_ABSTRACT_KIND: {
      ReportError("type %s is abstract\n", value->type->name->loc, value->type->name->name);
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
  Env env = {
    .parent = NULL,
    .mref = NULL,
    .interf = NULL,
    .prgm = prgm,
    .svars = NULL,
    .decl_status = NULL,
  };

  return CheckValue(arena, &env, value);
}
