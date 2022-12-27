/**
 * @file execute.c
 * Execution of fble functions.
 */

#include <fble/fble-execute.h>

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL, rand

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "heap.h"
#include "tc.h"
#include "unreachable.h"
#include "value.h"

/**
 * Managed execution stack.
 *
 * This is used to store function and arguments for tail calls.
 */
typedef struct Stack {
  /** Number of non-tail call stack frames above this frame on the stack. */
  size_t normal_call_frames;

  /** The function being executed at this frame of the stack. */
  FbleValue* func;

  /** the next frame down in the stack. */
  struct Stack* tail;

  /**
   * args to the function.
   *
   * Size is func->executable->num_args.
   */
  FbleValue* args[];
} Stack;

/**
 * A thread of execution.
 */
struct FbleThread {
  /** The execution stack. */
  Stack* stack;

  /** Memory allocator for the stack. */
  FbleStackAllocator* allocator;

  /**
   * The profile associated with this thread.
   * May be NULL to disable profiling.
   */
  FbleProfileThread* profile;
};

/**
 * Returned from FbleThreadTailCall to indicate tail call required.
 */
static FbleValue* sTailCallSentinelValue = (FbleValue*)"TAIL CALL SENTINEL VALUE";

static void PushNormalCallStackFrame(FbleThread* thread);
static void PushTailCallStackFrame(FbleValue* func, FbleValue** args, FbleThread* thread);
static void PopStackFrame(FbleValueHeap* heap, FbleThread* thread);
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);


// PushNormalCallStackFrame --
//   Push a frame on top of the thread's stack for a normal function call.
//
// Inputs:
//   thread - the thread whose stack to push to.
//
// Side effects:
// * The caller should call PopStackFrame when this frame is done executing.
static void PushNormalCallStackFrame(FbleThread* thread)
{
  thread->stack->normal_call_frames++;
}

// PushTailCallStackFrame --
//   Push a frame on top of the thread's stack.
//
// Inputs:
//   heap - the value heap
//   func - the function to push on the stack. Consumed.
//   args - arguments to the function. Consumed.
//   thread - the thread whose stack to push to.
//
// Side effects:
// * Pushes a frame on top of the stack.
// * Allocates memory that should be freed with a corresponding call to
//   PopStackFrame.
static void PushTailCallStackFrame(FbleValue* func, FbleValue** args, FbleThread* thread)
{
  FbleFuncInfo info = FbleFuncValueInfo(func);
  FbleExecutable* executable = info.executable;

  Stack* stack = FbleStackAllocExtra(thread->allocator, Stack, executable->num_args * sizeof(FbleValue*));
  stack->normal_call_frames = 0;
  stack->func = func;
  stack->tail = thread->stack;

  for (size_t i = 0; i < executable->num_args; ++i) {
    stack->args[i] = args[i];
  }

  thread->stack = stack;
}

// PopStackFrame --
//   Pops the top frame off the thread's stack.
//
// Inputs:
//   heap - the value heap
//   thread - the thread whose stack to pop the top frame off of.
//
// Side effects:
// * Pops the top frame off the stack.
static void PopStackFrame(FbleValueHeap* heap, FbleThread* thread)
{
  if (thread->stack->normal_call_frames > 0) {
    --thread->stack->normal_call_frames;
    return;
  }

  Stack* stack = thread->stack;
  thread->stack = thread->stack->tail;

  FbleFuncInfo info = FbleFuncValueInfo(stack->func);
  FbleExecutable* executable = info.executable;
  FbleReleaseValue(heap, stack->func);
  for (size_t i = 0; i < executable->num_args; ++i) {
    FbleReleaseValue(heap, stack->args[i]);
  }

  FbleStackFree(thread->allocator, stack);
}

// Eval --
//   Evaluate the given function.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   func - the function to evaluate.
//   args - args to pass to the function. length == func->argc.
//   profile - profile to update with execution stats. May be NULL to disable
//             profiling.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleReleaseValue when no longer in
//   use. Prints a message to stderr in case of error.
//   Updates profile based on the execution.
//   Does not take ownership of the function or the args.
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile)
{
  Stack stack = {
    .normal_call_frames = 0,
    .func = NULL,
    .tail = NULL
  };

  FbleThread thread;
  thread.stack = &stack;
  thread.allocator = FbleNewStackAllocator();
  thread.profile = profile == NULL ? NULL : FbleNewProfileThread(profile);

  FbleValue* result = FbleThreadCall(heap, &thread, func, args);
  assert(thread.stack == &stack);
  FbleFreeStackAllocator(thread.allocator);
  FbleFreeProfileThread(thread.profile);
  return result;
}

// FbleThreadCall -- see documentation in execute.h
FbleValue* FbleThreadCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args)
{
  FbleFuncInfo info = FbleFuncValueInfo(func);
  FbleExecutable* executable = info.executable;

  PushNormalCallStackFrame(thread);

  if (thread->profile != NULL) {
    FbleProfileEnterBlock(thread->profile, info.profile_block_offset + executable->profile_block_id);
  }

  FbleValue* result = executable->run(
        heap, thread, executable, args,
        info.statics, info.profile_block_offset);
  while (result == sTailCallSentinelValue) {
    assert(thread->stack->normal_call_frames == 0);
    func = thread->stack->func;
    info = FbleFuncValueInfo(func);
    executable = info.executable;

    if (thread->profile != NULL) {
      FbleProfileReplaceBlock(thread->profile, info.profile_block_offset + executable->profile_block_id);
    }

    result = executable->run(
        heap, thread, executable, thread->stack->args,
        info.statics, info.profile_block_offset);
  }

  if (thread->profile != NULL) {
    FbleProfileExitBlock(thread->profile);
  }

  PopStackFrame(heap, thread);
  return result;
}

// FbleThreadCall_ -- see documentation in execute.h
FbleValue* FbleThreadCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...)
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
  return FbleThreadCall(heap, thread, func, args);
}

// FbleThreadTailCall -- see documentation in execute.h
FbleValue* FbleThreadTailCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args)
{
  PopStackFrame(heap, thread);
  PushTailCallStackFrame(func, args, thread);
  return sTailCallSentinelValue;
}

// FbleThreadTailCall_ -- see documentation in execute.h
FbleValue* FbleThreadTailCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...)
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
  return FbleThreadTailCall(heap, thread, func, args);
}

// FbleEval -- see documentation in fble-value.h
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, NULL, profile);
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile)
{
  return Eval(heap, func, args, profile);
}

// FbleFreeExecutable -- see documentation in execute.h
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

// FbleExecutableNothingOnFree -- see documentation in execute.h
void FbleExecutableNothingOnFree(FbleExecutable* this)
{
  (void)this;
}

// FbleFreeExecutableModule -- see documentation in fble-execute.h
void FbleFreeExecutableModule(FbleExecutableModule* module)
{
  assert(module->magic == FBLE_EXECUTABLE_MODULE_MAGIC && "corrupt FbleExecutableModule");
  if (--module->refcount == 0) {
    FbleFreeModulePath(module->path);
    for (size_t j = 0; j < module->deps.size; ++j) {
      FbleFreeModulePath(module->deps.xs[j]);
    }
    FbleVectorFree(module->deps);
    FbleFreeExecutable(module->executable);
    for (size_t i = 0; i < module->profile_blocks.size; ++i) {
      FbleFreeName(module->profile_blocks.xs[i]);
    }
    FbleVectorFree(module->profile_blocks);
    FbleFree(module);
  }
}

// FbleFreeExecutableProgram -- see documentation in fble-execute.h
void FbleFreeExecutableProgram(FbleExecutableProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeExecutableModule(program->modules.xs[i]);
    }
    FbleVectorFree(program->modules);
    FbleFree(program);
  }
}

// See documentation in fble-execute.h
void FbleThreadSample(FbleThread* thread)
{
  if (thread->profile && (rand() % 1024 == 0)) {
    FbleProfileSample(thread->profile, 1);
  }
}

// See documentation in fble-execute.h
void FbleThreadEnterBlock(FbleThread* thread, FbleBlockId block)
{
  if (thread->profile) {
    FbleProfileEnterBlock(thread->profile, block);
  }
}

// See documentation in fble-execute.h
void FbleThreadReplaceBlock(FbleThread* thread, FbleBlockId block)
{
  if (thread->profile) {
    FbleProfileReplaceBlock(thread->profile, block);
  }
}

// See documentation in fble-execute.h
void FbleThreadExitBlock(FbleThread* thread)
{
  if (thread->profile) {
    FbleProfileExitBlock(thread->profile);
  }
}
