// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf

#include "fblc.h"
#include "fbld.h"

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
  CompiledFuncV funcv;
} Compiled;

static FbldFunc* LookupFunc(FbldProgram* prgm, FbldQName* entity);
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity, Compiled* compiled);

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
    assert(false && "TODO");
  }
  assert(false && "TODO");

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
