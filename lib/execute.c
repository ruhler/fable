/**
 * @file execute.c
 *  Execution of fble functions.
 */

#include <fble/fble-execute.h>

#include <alloca.h>         // for alloca
#include <assert.h>         // for assert
#include <stdlib.h>         // for NULL
#include <string.h>         // for memcpy
#include <sys/time.h>       // for getrlimit, setrlimit
#include <sys/resource.h>   // for getrlimit, setrlimit

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleFreeVector.

#include "heap.h"
#include "tc.h"
#include "unreachable.h"

static FbleValue* PartialApply(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args);
static FbleValue* Call(FbleValueHeap* heap, FbleProfileThread* profile, size_t argc, FbleValue** func_and_args);
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args, FbleProfile* profile);


/**
 * @func[PartialApply] Partially applies a function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][func] The function to evaluate.
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
static FbleValue* PartialApply(FbleValueHeap* heap, FbleValue* func, size_t argc, FbleValue** args)
{
  assert(false && "TODO: Implement me");
  return NULL;
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
      FbleReportError("called undefined function\n", FbleNewLoc(__FILE__, __LINE__, 7));
      return NULL;
    }

    FbleExecutable* executable = func->executable;
    if (argc < executable->num_args) {
      FbleValue* partial = PartialApply(heap, func_and_args[0], argc, func_and_args + 1);
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
    memmove(buffer, func_and_args, argc + 1 * sizeof(FbleValue*));
    func_and_args = buffer;

    FbleValue** tail_call_buffer = func_and_args + 1 + argc;
    FbleValue* result = executable->run(heap, profile,
        tail_call_buffer, func_and_args[0], func_and_args + 1);
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
      // Make the result be the new func for the unused args.
      func_and_args = unused - 1;
      func_and_args[0] = result;
      argc = num_unused;
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
  FbleFunction* function = FbleFuncValueFunction(func);
  FbleValue* result = FbleCall(heap, profile_thread, function, argc, args);
  FbleFreeProfileThread(profile_thread);

  // Restore the stack limit to what it was before.
  if (setrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }
  return result;
}

// See documentation in fble-execute.h.
FbleValue* FbleCall(FbleValueHeap* heap, FbleProfileThread* profile, FbleValue* function, size_t argc, FbleValue** args)
{
  FbleFunction* func = FbleFuncValueFunction(function);
  if (func == NULL) {
    FbleReportError("called undefined function\n", FbleNewLoc(__FILE__, __LINE__, 5));
    return NULL;
  }

  FbleExecutable* executable = func->executable;
  if (argc < executable->num_args) {
    return PartialApply(heap, function, argc, args);
  }

  if (profile) {
    FbleProfileEnterBlock(profile, func->profile_block_offset + executable->profile_block_id);
  }

  size_t num_unused = argc - executable->num_args;
  FbleValue** unused = args + executable->num_args;

  FbleValue* tail_call_buffer[executable->tail_call_buffer_size + 1 + num_unused];
  FbleValue* result = executable->run(heap, profile, tail_call_buffer, func, args);

  if (num_unused > 0 && result != FbleTailCallSentinelValue) {
    // If not all args were used, treat that like we need to do a tail call.
    tail_call_buffer[0] = result;
    tail_call_buffer[1] = NULL;
    result = FbleTailCallSentinelValue;
  }

  if (result == FbleTailCallSentinelValue) {
    // Find how many tail call args there are.
    argc = 0;
    while (tail_call_buffer[1 + argc] != NULL) {
      argc++;
    }

    // Add the unused args to the end of the tail call args and make that
    // our new func and args. We'll have to retain those args.
    for (size_t i = 0; i < num_unused) {
      FbleRetainValue(unused[i]);
      tail_call_buffer[1 + argc] = unused[i];
      argc++;
    }

    // Use Call to finish up now that we have ownership of the func and
    // remaining args.
    result = Call(heap, profile, argc, tail_call_buffer);
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

// See documentation in fble-execute.h.
void FbleFreeExecutable(FbleExecutable* executable)
{
  if (executable == NULL) {
    return;
  }

  // We've had trouble with double free in the past. Check to make sure the
  // magic in the block hasn't been corrupted. Otherwise we've probably
  // already freed this executable and decrementing the refcount could end up
  // corrupting whatever is now making use of the memory that was previously
  // used for the instruction block.
  assert(executable->magic == FBLE_EXECUTABLE_MAGIC && "corrupt FbleExecutable");

  assert(executable->refcount > 0);
  executable->refcount--;
  if (executable->refcount == 0) {
    executable->on_free(executable);
    FbleFree(executable);
  }
}

// See documentation in fble-execute.h.
void FbleExecutableNothingOnFree(FbleExecutable* this)
{
  (void)this;
}

// See documentation in fble-execute.h.
void FbleFreeExecutableModule(FbleExecutableModule* module)
{
  assert(module->magic == FBLE_EXECUTABLE_MODULE_MAGIC && "corrupt FbleExecutableModule");
  if (--module->refcount == 0) {
    FbleFreeModulePath(module->path);
    for (size_t j = 0; j < module->deps.size; ++j) {
      FbleFreeModulePath(module->deps.xs[j]);
    }
    FbleFreeVector(module->deps);
    FbleFreeExecutable(module->executable);
    for (size_t i = 0; i < module->profile_blocks.size; ++i) {
      FbleFreeName(module->profile_blocks.xs[i]);
    }
    FbleFreeVector(module->profile_blocks);
    FbleFree(module);
  }
}

// See documentation in fble-execute.h.
void FbleFreeExecutableProgram(FbleExecutableProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeExecutableModule(program->modules.xs[i]);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
}
