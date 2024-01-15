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

static FbleValue* PartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args);
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleValue** tail_call_buffer, FbleFunction* function, FbleValue** args);

static FbleValue* Call(FbleValueHeap* heap, FbleProfileThread* profile, size_t argc, FbleValue** func_and_args);
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);


// FbleRunFunction for PartialApply executable.
// See documentation of FbleRunFunction in fble-function.h
static FbleValue* PartialApplyImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleValue** tail_call_buffer, FbleFunction* function, FbleValue** args)
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
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleFunction*][function] The function to apply.
 *  @arg[FbleValue*][func] The function value to apply.
 *  @arg[size_t][argc] Number of args to pass.
 *  @arg[FbleValue**][args] Args to pass to the function.
 *
 *  @returns[FbleValue*] The allocated result.
 *
 *  @sideeffects
 *   @item
 *    Allocates an FbleValue that should be freed with FbleReleaseValue when
 *    no longer needed.
 *   @item
 *    Behavior is undefined if the number of arguments provided is not less
 *    than the number of arguments expected by the function.
 */
static FbleValue* PartialApply(FbleValueHeap* heap, FbleFunction* function, FbleValue* func, size_t argc, FbleValue** args)
{
  FbleExecutable exe = {
    .num_args = function->executable.num_args - argc,
    .num_statics = 1 + argc,
    .tail_call_buffer_size = 2 + function->executable.num_args,
    .profile_block_id = function->executable.profile_block_id,
    .run = &PartialApplyImpl
  };

  FbleValue* statics[1 + argc];
  statics[0] = func;
  memcpy(statics + 1, args, argc * sizeof(FbleValue*));
  return FbleNewFuncValue(heap, &exe, function->profile_block_offset, statics);
}

/**
 * @func[Call] Calls an fble function.
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *  @arg[size_t] argc
 *   Number of arguments passed to the function.
 *  @arg[FbleValue**] func_and_args
 *   Function followed by arguments to pass to the function. Consumed.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Enters a profiling block for the function being called.
 *   @i Executes the called function to completion, returning the result.
 *   @i Calls FbleReleaseValue on the provided func and args.
 */
static FbleValue* Call(FbleValueHeap* heap, FbleProfileThread* profile, size_t argc, FbleValue** func_and_args)
{
  size_t buffer_size = 1; // In units of FbleValue*.
  FbleValue** buffer = alloca(sizeof(FbleValue*));

  while (true) {
    FbleFunction* func = FbleFuncValueFunction(func_and_args[0]);
    if (func == NULL) {
      FbleLoc loc = FbleNewLoc(__FILE__, __LINE__ + 1, 7);
      FbleReportError("called undefined function\n", loc);
      FbleFreeLoc(loc);
      FbleReleaseValues(heap, 1 + argc, func_and_args);
      return NULL;
    }

    FbleExecutable* executable = &func->executable;
    if (argc < executable->num_args) {
      FbleValue* partial = PartialApply(heap, func, func_and_args[0], argc, func_and_args + 1);
      FbleReleaseValues(heap, argc+1, func_and_args);
      return partial;
    }

    if (profile) {
      FbleProfileReplaceBlock(profile, func->profile_block_offset + executable->profile_block_id);
    }

    // Move func_and_args to start of buffer, allocating more space at the
    // back of the buffer for the tail_call_buffer if needed.
    size_t num_unused = argc - executable->num_args;
    size_t needed_buffer_size = 1 + argc
      + executable->tail_call_buffer_size
      + num_unused;   // Space for moving unused args after tail_call_buffer.
    if (needed_buffer_size > buffer_size) {
      size_t incr = needed_buffer_size - buffer_size;
      FbleValue** nbuffer = alloca(incr * sizeof(FbleValue*));

      // We assume repeated calls to alloca result in adjacent memory
      // allocations on the stack. We don't know if stack goes up or down. To
      // find the start of the full allocated region, pick the smallest of the
      // pointers we have so far.
      if (nbuffer < buffer) {
        buffer = nbuffer;
      }
      buffer_size += incr;
    }
    memmove(buffer, func_and_args, (argc + 1) * sizeof(FbleValue*));
    func_and_args = buffer;

    FbleValue** tail_call_buffer = func_and_args + 1 + argc;
    FbleValue* result = executable->run(heap, profile,
        tail_call_buffer, func, func_and_args + 1);
    FbleReleaseValues(heap, 1 + executable->num_args, func_and_args);
    FbleValue** unused = func_and_args + 1 + executable->num_args;

    if (result == FbleTailCallSentinelValue) {
      // Add the unused args to the end of the tail call args and make that
      // our new func and args.
      func_and_args = tail_call_buffer;
      argc = 1;
      while (func_and_args[1 + argc] != NULL) {
        argc++;
      }
      memcpy(func_and_args + 1 + argc, unused, num_unused * sizeof(FbleValue*));
      argc += num_unused;
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
    return PartialApply(heap, func, function, argc, args);
  }

  if (profile) {
    FbleProfileEnterBlock(profile, func->profile_block_offset + executable->profile_block_id);
  }

  size_t num_unused = argc - executable->num_args;
  FbleValue** unused = args + executable->num_args;

  FbleValue* tail_call_buffer[executable->tail_call_buffer_size + 1 + num_unused];
  FbleValue* result = executable->run(heap, profile, tail_call_buffer, func, args);

  if (result == FbleTailCallSentinelValue) {
    // Find how many tail call args there are.
    argc = 1;
    while (tail_call_buffer[1 + argc] != NULL) {
      argc++;
    }

    // Add the unused args to the end of the tail call args and make that
    // our new func and args. We'll have to retain those args.
    for (size_t i = 0; i < num_unused; ++i) {
      FbleRetainValue(heap, unused[i]);
      tail_call_buffer[1 + argc] = unused[i];
      argc++;
    }

    // Use Call to finish up now that we have ownership of the func and
    // remaining args.
    result = Call(heap, profile, argc, tail_call_buffer);
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