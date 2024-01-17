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


static FbleValue* TailCallSentinel = (FbleValue*)0x2;

typedef struct {
  FbleFunction* function;
  FbleValue* func;
  size_t argc;
  FbleValue** args;
  size_t args_capacity;
} TailCallData;

static TailCallData gTailCallData = {
  .function = NULL,
  .func = NULL,
  .argc = 0,
  .args = NULL,
  .args_capacity = 0
};

static void EnsureTailCallArgsSpace(size_t argc);

static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* PartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args);

static FbleValue* TailCall(FbleValueHeap* heap, FbleProfileThread* profile);
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);

/**
 * @func[EnsureTailCallArgsSpace] Makes sure enough space is allocated.
 *  Resizes gTailCallData.args as needed to have space for @a[argc] values.
 *
 *  @arg[size_t][argc] The number of arguments needed.
 *  @sideeffects
 *   Resizes gTailCallData.args as needed. The args array may be reallocated;
 *   the previous value should not be referenced after this call.
 */
static void EnsureTailCallArgsSpace(size_t argc)
{
  if (argc > gTailCallData.args_capacity) {
    gTailCallData.args_capacity = argc;
    FbleValue** args = FbleAllocArray(FbleValue*, argc);
    memcpy(args, gTailCallData.args, gTailCallData.argc * sizeof(FbleValue*));
    FbleFree(gTailCallData.args);
    gTailCallData.args = args;
  }
}

// FbleRunFunction for PartialApply executable.
// See documentation of FbleRunFunction in fble-function.h
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  size_t s = function->executable.num_statics - 1;
  size_t a = function->executable.num_args;
  size_t argc = s + a;
  FbleValue* nargs[argc];
  memcpy(nargs, function->statics + 1, s * sizeof(FbleValue*));
  memcpy(nargs + s, args, a * sizeof(FbleValue*));
  return FbleCall(heap, profile, function->statics[0], argc, nargs);
}

/**
 * @func[PartialApply] Partially applies a function.
 *  Creates a thunk with the function and arguments without applying the
 *  function yet.
 *
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleFunction*][function] The function to apply.
 *  @arg[FbleValue*][func] The function value to apply.
 *  @arg[size_t][argc] Number of args to pass.
 *  @arg[FbleValue**][args] Args to pass to the function.
 *
 *  @returns[FbleValue*] The allocated result.
 *
 *  @sideeffects
 *   Allocates an FbleValue that should be freed with FbleReleaseValue when no
 *   longer needed.
 */
static FbleValue* PartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args)
{
  FbleExecutable exe = {
    .num_args = function->executable.num_args - argc,
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
 * @func[TailCall] Tail calls an fble function.
 *  Calls the function and args stored in gTailCallData.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Enters a profiling block for the function being called.
 *   @i Executes the called function to completion, returning the result.
 *   @i Calls FbleReleaseValue on the provided func and args.
 */
static FbleValue* TailCall(FbleValueHeap* heap, FbleProfileThread* profile)
{
  while (true) {
    FbleFunction* function = gTailCallData.function;
    FbleExecutable* exe = &function->executable;
    size_t argc = gTailCallData.argc;
    FbleValue* func = gTailCallData.func;

    if (argc < exe->num_args) {
      FbleValue* partial = PartialApply(heap, function, func, argc, gTailCallData.args);
      FbleReleaseValue(heap, func);
      FbleReleaseValues(heap, argc, gTailCallData.args);
      return partial;
    }

    if (profile) {
      FbleProfileReplaceBlock(profile, function->profile_block_offset + exe->profile_block_id);
    }

    FbleValue* args[argc];
    memcpy(args, gTailCallData.args, argc * sizeof(FbleValue*));

    FbleValue* result = exe->run(heap, profile, function, args);
    FbleReleaseValue(heap, func);
    FbleReleaseValues(heap, exe->num_args, args);
    size_t num_unused = argc - exe->num_args;
    FbleValue** unused = args + exe->num_args;

    if (result == TailCallSentinel) {
      // Add the unused args to the end of the tail call args and make that
      // our new func and args.
      EnsureTailCallArgsSpace(gTailCallData.argc + num_unused);
      memcpy(gTailCallData.args + gTailCallData.argc, unused, num_unused * sizeof(FbleValue*));
      gTailCallData.argc += num_unused;
    } else if (num_unused > 0) {
      FbleValue* new_func = result;
      result = FbleCall(heap, profile, new_func, num_unused, unused);
      FbleReleaseValue(heap, new_func);
      FbleReleaseValues(heap, num_unused, unused);
      return result;
    } else {
      return result;
    }
  }

  FbleUnreachable("should never get here");
  return NULL;
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

  // Set up gTailCallData.args.
  gTailCallData.args_capacity = 0;
  gTailCallData.args = NULL;
  gTailCallData.argc = 0;

  FbleProfileThread* profile_thread = FbleNewProfileThread(profile);
  FbleValue* result = FbleCall(heap, profile_thread, func, argc, args);
  FbleFreeProfileThread(profile_thread);

  FbleFree(gTailCallData.args);
  gTailCallData.args = NULL;
  gTailCallData.args_capacity = 0;
  gTailCallData.argc = 0;

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
    return PartialApply(heap, func, function, argc, args);
  }

  if (profile) {
    FbleProfileEnterBlock(profile, func->profile_block_offset + executable->profile_block_id);
  }

  size_t num_unused = argc - executable->num_args;
  FbleValue** unused = args + executable->num_args;

  FbleValue* result = executable->run(heap, profile, func, args);

  if (result == TailCallSentinel) {
    EnsureTailCallArgsSpace(gTailCallData.argc + num_unused);
    for (size_t i = 0; i < num_unused; ++i) {
      gTailCallData.args[gTailCallData.argc++] = unused[i];
      FbleRetainValue(heap, unused[i]);
    }
    result = TailCall(heap, profile);
  } else if (num_unused > 0) {
    FbleValue* new_func = result;
    result = FbleCall(heap, profile, new_func, num_unused, unused);
    FbleReleaseValue(heap, new_func);
  }

  if (profile != NULL) {
    FbleProfileExitBlock(profile);
  }

  return result;
}

// See documentation in fble-function.h
FbleValue* FbleTailCall(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args, size_t releasec, FbleValue** releases)
{
  EnsureTailCallArgsSpace(argc);

  gTailCallData.function = function;
  gTailCallData.func = func;
  gTailCallData.argc = argc;
  FbleRetainValue(heap, func);
  for (size_t i = 0; i < argc; ++i) {
    FbleRetainValue(heap, args[i]);
    gTailCallData.args[i] = args[i];
  }
  FbleReleaseValues(heap, releasec, releases);
  return TailCallSentinel;
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
