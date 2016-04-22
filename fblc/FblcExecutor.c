// FblcExecutor.c --
//
//   This file implements routines for executing Fblc processes.

#include "FblcInternal.h"


// FblcExecute --
//
//   Execute a process action in the given program environment. The program
//   and process must be well formed.
//
// Inputs:
//   env - The program environment.
//   actn - The process action to execute.
//
// Returns:
//   The result of executing the given process action in the program
//   environment in a scope with no local variables and no ports. Returns NULL
//   if the action does not return a value.
//
// Side effects:
//   None.

FblcValue* FblcExecute(const FblcEnv* env, const FblcActn* actn)
{
  assert(actn->tag == FBLC_EVAL_ACTN);
  return FblcEvaluate(env, actn->ac.eval.expr);
}
