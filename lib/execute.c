/**
 * @file execute.c
 * Execution of fble functions.
 */

#include <fble/fble-execute.h>

#include <alloca.h>   // for alloca
#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL
#include <sys/time.h>       // for getrlimit, setrlimit
#include <sys/resource.h>   // for getrlimit, setrlimit

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleFreeVector.

#include "heap.h"
#include "tc.h"
#include "unreachable.h"

static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);


/**
 * @func[Eval] Evaluates the given function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][func] The function to evaluate.
 *  @arg[FbleValue**][args] Args to pass to the function. length == func->argc.
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
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile)
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
  FbleValue* result = FbleCall(heap, profile_thread, func, args);
  FbleFreeProfileThread(profile_thread);

  // Restore the stack limit to what it was before.
  if (setrlimit(RLIMIT_STACK, &original_stack_limit) != 0) {
    assert(false && "setrlimit failed");
  }
  return result;
}

// See documentation in fble-execute.h.
FbleValue* FbleCall(FbleValueHeap* heap, FbleProfileThread* profile, FbleValue* func, FbleValue** args)
{
  FbleFuncInfo info = FbleFuncValueInfo(func);
  FbleExecutable* executable = info.executable;

  if (profile) {
    FbleProfileEnterBlock(profile, info.profile_block_offset + executable->profile_block_id);
  }

  size_t buffer_size = executable->tail_call_buffer_size;
  FbleValue** buffer = alloca(buffer_size * sizeof(FbleValue*));
  FbleValue** tail_call_buffer = buffer;
  FbleValue* result = executable->run(
        heap, tail_call_buffer, executable, args,
        info.statics, info.profile_block_offset, profile);
  while (result == FbleTailCallSentinelValue) {
    // Invariants at this point in the code:
    // * The new func and args to call are sitting in tail_call_buffer.
    // * The start of our stack allocated buffer is 'buffer'.
    func = tail_call_buffer[0];
    info = FbleFuncValueInfo(func);
    executable = info.executable;
    size_t num_args_plus_1 = 1 + executable->num_args;
    if (executable->tail_call_buffer_size + num_args_plus_1 > buffer_size) {
      size_t incr = executable->tail_call_buffer_size + num_args_plus_1 - buffer_size;
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

    for (size_t i = 0; i < num_args_plus_1; ++i) {
      buffer[i] = tail_call_buffer[i];
    }
    args = buffer + 1;
    tail_call_buffer = buffer + num_args_plus_1;

    if (profile) {
      FbleProfileReplaceBlock(profile, info.profile_block_offset + executable->profile_block_id);
    }

    result = executable->run(
        heap, tail_call_buffer, executable, args,
        info.statics, info.profile_block_offset, profile);
    for (size_t i = 0; i < num_args_plus_1; ++i) {
      FbleReleaseValue(heap, buffer[i]);
    }
  }

  if (profile != NULL) {
    FbleProfileExitBlock(profile);
  }

  return result;
}

// See documentation in fble-execute.h.
FbleValue* FbleCall_(FbleValueHeap* heap, FbleProfileThread* profile, FbleValue* func, ...)
{
  FbleExecutable* executable = FbleFuncValueInfo(func).executable;
  size_t argc = executable->num_args;
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, func);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  return FbleCall(heap, profile, func, args);
}

// See documentation in fble-value.h.
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, NULL, profile);
}

// See documentation in fble-value.h.
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile)
{
  return Eval(heap, func, args, profile);
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
