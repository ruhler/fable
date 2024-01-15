/**
 * @file function.c
 *  Execution of fble functions.
 */

#include <fble/fble-function.h>

#include <alloca.h>         // for alloca
#include <assert.h>         // for assert
#include <stdlib.h>         // for NULL
#include <string.h>         // for memcpy, memmove
#include <sys/time.h>       // for getrlimit, setrlimit
#include <sys/resource.h>   // for getrlimit, setrlimit

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleFreeVector.

#include "heap.h"
#include "tc.h"
#include "unreachable.h"

static bool IsTailCall(FbleValue* result);
static FbleValue* GetThunk(FbleValue* result);
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);

static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);


/**
 * @func[IsTailCall] Tests if a tail call is requried.
 *  @arg[FbleValue*][result] The result of a call to check.
 *  @returns[bool] True if the tail call bit is set.
 *  @sideeffects None
 */
static bool IsTailCall(FbleValue* result)
{
  return (((intptr_t)result) & FbleTailCallMask) == FbleTailCallBit;
}

/**
 * @func[GetThunk] Gets the thunk for a tail call.
 *  @arg[FbleValue*][result] The result of the call to get the thunk from.
 *  @returns[FbleValue*] The thunk for the tail call.
 *  @sideffects
 *   Behavior is undefined if result is not a tail call.
 */
static FbleValue* GetThunk(FbleValue* result)
{
  return (FbleValue*)((intptr_t)result & ~FbleTailCallBit);
}

// FbleRunFunction for PartialApply executable.
// See documentation of FbleRunFunction in fble-function.h
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  size_t a = function->executable.num_args;
  size_t s = function->executable.num_statics - 1;
  size_t argc = s + a;
  FbleValue* nargs[argc];
  memcpy(nargs, function->statics + 1, s * sizeof(FbleValue*));
  memcpy(nargs + s, args, a * sizeof(FbleValue*));

  FbleFunction* func = FbleFuncValueFunction(function->statics[0]);
  if (func == NULL) {
    FbleLoc loc = FbleNewLoc(__FILE__, __LINE__ + 1, 5);
    FbleReportError("called undefined function\n", loc);
    FbleFreeLoc(loc);
    return NULL;
  }

  FbleExecutable* executable = &func->executable;
  if (argc == executable->num_args) {
    return func->executable.run(heap, profile, func, nargs);
  }
  return FbleCall(heap, profile, function->statics[0], argc, nargs);
}

// See documentation in fble-function.h
FbleValue* FblePartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args)
{
  size_t num_args = 0;
  if (function->executable.num_args > argc) {
    num_args = function->executable.num_args - argc;
  }

  FbleExecutable exe = {
    .num_args = num_args,
    .num_statics = 1 + argc,
    .profile_block_id = function->executable.profile_block_id,
    .run = &PartialApplyImpl
  };

  FbleValue* statics[1 + argc];
  statics[0] = func;
  memcpy(statics + 1, args, argc * sizeof(FbleValue*));
  return FbleNewFuncValue(heap, &exe, function->profile_block_offset, statics);
}

/**
 * @func[Eval] Evaluates the given function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][func] The function to evaluate.
 *  @arg[size_t][argc] Number of args to pass.
 *  @arg[FbleValue**][args] Args to pass to the function.
 *  @arg[FbleProfile*][profile]
 *   Profile to update with execution stats. Must not be NULL.
 *  
 *  @returns[FbleValue*] The computed value, or NULL on error.
 *  
 *  @sideeffects
 *   @item
 *    The returned value must be freed with FbleReleaseValue when no longer in
 *    use.
 *   @i Prints a message to stderr in case of error.
 *   @i Updates profile based on the execution.
 *   @i Does not take ownership of the function or the args.
 */
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile)
{
  // The fble spec requires we don't put an arbitrarily low limit on the stack
  // size. Fix that here.
  struct rlimit original_stack_limit;
  if (getrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "getrlimit failed");
  }

  struct rlimit new_stack_limit = original_stack_limit;
  new_stack_limit.rlim_cur = new_stack_limit.rlim_max;
  if (setrlimit(RLIMIT_STACK, &new_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }

  FbleProfileThread* profile_thread = FbleNewProfileThread(profile);
  FbleValue* result = FbleCall(heap, profile_thread, func, argc, args);
  FbleFreeProfileThread(profile_thread);

  // Restore the stack limit to what it was before.
  if (setrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }
  return result;
}

// See documentation in fble-function.h
FbleValue* FbleCall(FbleValueHeap* heap, FbleProfileThread* profile, FbleValue* function, size_t argc, FbleValue** args)
{
  FbleFunction* func = FbleFuncValueFunction(function);
  if (func == NULL) {
    FbleLoc loc = FbleNewLoc(__FILE__, __LINE__ + 1, 5);
    FbleReportError("called undefined function\n", loc);
    FbleFreeLoc(loc);
    return NULL;
  }

  FbleExecutable* executable = &func->executable;
  if (argc < executable->num_args) {
    return FblePartialApply(heap, func, function, argc, args);
  }

  if (profile) {
    FbleProfileEnterBlock(profile, func->profile_block_offset + executable->profile_block_id);
  }

  size_t num_unused = argc - executable->num_args;
  FbleValue** unused = args + executable->num_args;

  FbleValue* result = executable->run(heap, profile, func, args);

  while (IsTailCall(result)) {
    FbleValue* thunk = GetThunk(result);
    FbleFunction* thunk_func = FbleFuncValueFunction(thunk);
    if (thunk_func == NULL) {
      FbleLoc loc = FbleNewLoc(__FILE__, __LINE__ + 1, 7);
      FbleReportError("called undefined function\n", loc);
      FbleFreeLoc(loc);
      FbleReleaseValue(heap, thunk);
      return NULL;
    }

    FbleExecutable* thunk_exe = &thunk_func->executable;
    if (thunk_exe->num_args > 0) {
      result = thunk;
      break;
    }

    if (profile) {
      FbleProfileReplaceBlock(profile, thunk_func->profile_block_offset + thunk_exe->profile_block_id);
    }

    result = thunk_exe->run(heap, profile, thunk_func, NULL);
    FbleReleaseValue(heap, thunk);
  }
  
  if (num_unused > 0) {
    FbleValue* new_func = result;
    result = FbleCall(heap, profile, new_func, num_unused, unused);
    FbleReleaseValue(heap, new_func);
  }

  if (profile != NULL) {
    FbleProfileExitBlock(profile);
  }

  return result;
}

// See documentation in fble-value.h.
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, 0, NULL, profile);
}

// See documentation in fble-value.h.
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile)
{
  return Eval(heap, func, argc, args, profile);
}
