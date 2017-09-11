// compile.c --
//   This file implements routines for compiling an fbld program to an fblc
//   program.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf

#include "fblc.h"
#include "fbld.h"


static FbldFunc* LookupFunc(FbldProgram* prgm, FbldQName* entity);
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity);

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
//
// Returns:
//   A compiled fblc func, or NULL in case of error.
//
// Side effects:
//   Outputs an error message to stderr in case of error.
static FblcFunc* CompileFunc(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity)
{
  assert(false && "TODO: CompileFunc");
}

// FbldCompileProgram -- see documentation in fbld.h
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQName* entity)
{
  FblcFunc* func = CompileFunc(arena, accessv, prgm, entity);
  if (func == NULL) {
    return NULL;
  }

  // Create an FblcProc wrapper for the compiled function.
  // TODO: Once we support procs in fbld, move the wrapper to the fbld side of
  // compilation.
  FblcEvalActn* body = FBLC_ALLOC(arena, FblcEvalActn);
  body->_base.tag = FBLC_EVAL_ACTN;
  body->arg = func->body;

  FblcProc* proc = FBLC_ALLOC(arena, FblcProc);
  FblcVectorInit(arena, proc->portv);
  proc->argv.size = func->argv.size;
  proc->argv.xs = func->argv.xs;
  proc->return_type = func->return_type;
  proc->body = &body->_base;

  FbldLoaded* loaded = FBLC_ALLOC(arena, FbldLoaded);
  loaded->prog = prgm;
  loaded->proc_d = LookupFunc(prgm, entity);
  loaded->proc_c = proc;
  return loaded;
}

// FbldCompileValue -- see documentation in fbld.h
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value)
{
  assert(false && "TODO: FbldCompileValue");
  return NULL;
}
