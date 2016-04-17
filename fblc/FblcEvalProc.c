// FblcEvalProc.c --
//
//   This file implements routines for evaluating Fblc processes.

#include "FblcInternal.h"


// FblcEvalProc --
//
//   Evaluate a process under the given program environment. The program
//   and process must be well formed.
//
// Inputs:
//   env - The program environment.
//   expr - The process to evaluate.
//
// Returns:
//   The result of evaluating the given process in the program environment
//   in a scope with no local variables and no ports. Returns NULL if the
//   process does not return a value.
//
// Side effects:
//   None.

FblcValue* FblcEvalProc(const FblcEnv* env, const FblcProcExpr* proc)
{
  assert(proc->tag == FBLC_EVAL_PROC);
  return FblcEvaluate(env, proc->eval.expr);
}
