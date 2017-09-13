// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf

#include "fblc.h"
#include "fbld.h"

// CompiledType
//   The compiled type for a named entity.
typedef struct {
  FbldQName* entity;
  FblcType* compiled;
} CompiledType;

// CompiledTypeV
//   A vector of compiled types.
typedef struct {
  size_t size;
  CompiledType* xs;
} CompiledTypeV;

// CompiledFunc
//   The compiled function for a named entity.
typedef struct {
  FbldQName* entity;
  FblcFunc* compiled;
} CompiledFunc;

// CompiledFuncV
//   A vector of compiled functions.
typedef struct {
  size_t size;
  CompiledFunc* xs;
} CompiledFuncV;

// Compiled
//   A collection of already compiled entities.
typedef struct {
  CompiledTypeV typev;
  CompiledFuncV funcv;
} Compiled;

static FbldFunc* LookupFunc(FbldProgram* prgm, FbldQName* entity);
static FbldQName* ResolveEntity(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity);
static FblcType* CompileType(FblcArena* arena, FbldProgram* prgm, FbldQName* entity, Compiled* compiled);
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity, Compiled* compiled);
static FblcType* CompileForeignType(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity, Compiled* compiled);

// LookupFunc --
//   Look up a function entity in the program.
//
// Inputs:
//   prgm - The program to look in.
//   entity - The entity to look up.
//
// Results:
//   The function entity or NULL if no such function was found.
//
// Side effects:
//   None
static FbldFunc* LookupFunc(FbldProgram* prgm, FbldQName* entity)
{
  for (size_t i = 0; i < prgm->mdefnv.size; ++i) {
    FbldMDefn* mdefn = prgm->mdefnv.xs[i];
    if (FbldNamesEqual(entity->mref->name->name, mdefn->name->name)) {
      for (size_t j = 0; j < mdefn->funcv->size; ++j) {
        FbldFunc* func = mdefn->funcv->xs[j];
        if (FbldNamesEqual(entity->name->name, func->name->name)) {
          return func;
        }
      }
    }
  }
  return NULL;
}

// ResolveEntity --
//   Resolve all type and module arguments in the given entity specification.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program context.
//   mref - The context the entity is being referred to from.
//   entity - The entity to resolve.
//
// Results:
//   A resolved form of the entity, with type and module variables replaced
//   based on the provided mref context.
//
// Side effects:
//   Allocates a new entity that somebody should probably clean up somehow.
static FbldQName* ResolveEntity(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity)
{
  FbldQName* resolved = FBLC_ALLOC(arena, FbldQName);
  resolved->name = entity->name;

  if (entity->mref == NULL) {
    resolved->mref = mref;
    return resolved;
  }

  // TODO: Substitute type and module parameters in entity->mref based on the
  // substitution map derived from mref and the module for entity->mref->name
  // (or something like that).
  assert(false && "TODO: ResolveEntity");
  return NULL;
}

// CompileType --
//   Return a compiled fblc type for the named type.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program environment.
//   entity - The type to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc type, or NULL in case of error.
//
// Side effects:
//   Adds the compiled type to 'compiled' if it is newly compiled.
static FblcType* CompileType(FblcArena* arena, FbldProgram* prgm, FbldQName* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->typev.size; ++i) {
    if (FbldQNamesEqual(compiled->typev.xs[i].entity, entity)) {
      return compiled->typev.xs[i].compiled;
    }
  }

  assert(false && "TODO: CompileType");
  return NULL;
}

// CompileFunc --
//   Return a compiled fblc func for the named function.
//
// Inputs:
//   arena - Arena to use for allocations.
//   accessv - Collection of access expression debug info to return.
//   prgm - The program environment.
//   entity - The function to compile.
//   compiled - The collection of already compiled entities.
//
// Returns:
//   A compiled fblc func, or NULL in case of error.
//
// Side effects:
//   Adds information about access expressions to accessv.
//   Adds the compiled function to 'compiled' if it is newly compiled.
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity, Compiled* compiled)
{
  // Check to see if we have already compiled the entity.
  for (size_t i = 0; i < compiled->funcv.size; ++i) {
    if (FbldQNamesEqual(compiled->funcv.xs[i].entity, entity)) {
      return compiled->funcv.xs[i].compiled;
    }
  }

  FbldFunc* func_d = LookupFunc(prgm, entity);
  assert(func_d != NULL);

  FblcFunc* func_c = FBLC_ALLOC(arena, FblcFunc);
  FblcVectorInit(arena, func_c->argv);
  for (size_t arg_id = 0; arg_id < func_d->argv->size; ++arg_id) {
    FblcType* arg_type = CompileForeignType(arena, prgm, entity->mref, func_d->argv->xs[arg_id]->type, compiled);
    FblcVectorAppend(arena, func_c->argv, arg_type);
  }

  assert(false && "TODO");
  return NULL;
}

// CompileForeignType --
//   Compile a type referred to from another context.
//
// Inputs:
//   arena - Arena to use for allocations.
//   prgm - The program environment.
//   mref - The context from which the type is referred to.
//   entity - The type to compile.
//   compiled - The collection of compiled entities.
static FblcType* CompileForeignType(FblcArena* arena, FbldProgram* prgm, FbldMRef* mref, FbldQName* entity, Compiled* compiled)
{
  FbldQName* resolved = ResolveEntity(arena, prgm, mref, entity);
  return CompileType(arena, prgm, resolved, compiled);
}

// FbldCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity)
{
  FbldFunc* func_d = LookupFunc(prgm, entity);
  if (func_d == NULL) {
    fprintf(stderr, "main entry not found\n");
    return NULL;
  }

  Compiled compiled;
  FblcVectorInit(arena, compiled.funcv);
  FblcFunc* func_c = CompileFunc(arena, accessv, prgm, entity, &compiled);

  // Create an FblcProc wrapper for the compiled function.
  // TODO: Once we support procs in fbld, move the wrapper to the fbld side of
  // compilation.
  FblcEvalActn* body = FBLC_ALLOC(arena, FblcEvalActn);
  body->_base.tag = FBLC_EVAL_ACTN;
  body->arg = func_c->body;

  FblcProc* proc = FBLC_ALLOC(arena, FblcProc);
  FblcVectorInit(arena, proc->portv);
  proc->argv.size = func_c->argv.size;
  proc->argv.xs = func_c->argv.xs;
  proc->return_type = func_c->return_type;
  proc->body = &body->_base;

  FbldLoaded* loaded = FBLC_ALLOC(arena, FbldLoaded);
  loaded->prog = prgm;
  loaded->proc_d = func_d;
  loaded->proc_c = proc;
  return loaded;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  assert(false && "TODO: FbldCompileValue");
  return NULL;
}
