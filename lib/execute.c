// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include <fble/fble-execute.h>

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL, rand

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "heap.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

/**
 * Managed execution stack.
 *
 * Memory Management:
 *   Each thread owns its stack. The stack owns its tail.
 *
 *   The stack holds a strong reference to func and any non-NULL locals.
 *   'result' is a pointer to something that is initially NULL and expects to
 *   receive a strong reference to the return value.
 */
typedef struct Stack {
  /** the function being executed at this frame of the stack. */
  FbleValue* func;

  /** where to store the result of executing the current frame. */
  FbleValue** result;

  /** the next frame down in the stack. */
  struct Stack* tail;

  /** array of local variables. Size is func->executable->locals. */
  // TODO: Let users keep track of locals themselves and only store args here?
  FbleValue* locals[];
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


static void PushStackFrame(FbleValue* func, FbleValue** result, size_t locals, FbleThread* thread);
static void PopStackFrame(FbleValueHeap* heap, FbleThread* thread);
static FbleValue* Eval(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile);

// PushStackFrame --
//   Push a frame on top of the thread's stack.
//
// Inputs:
//   func - the function to push on the stack. Consumed.
//   result - where to store the result of executing the function.
//   locals - the number of local values to allocate stack space for.
//   thread - the thread whose stack to push to.
//
// Side effects:
// * Pushes a frame on top of the stack. Local variables are not initialized.
// * Allocates memory that should be freed with a corresponding call to
//   PopStackFrame.
// * Consumes the given function, which will be released when PopStackFrame is
//   called.
static void PushStackFrame(FbleValue* func, FbleValue** result, size_t locals, FbleThread* thread)
{
  Stack* stack = FbleStackAllocExtra(thread->allocator, Stack, locals * sizeof(FbleValue*));
  stack->func = func;
  stack->result = result;
  stack->tail = thread->stack;
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
//   Pops the top frame off the stack.
static void PopStackFrame(FbleValueHeap* heap, FbleThread* thread)
{
  Stack* stack = thread->stack;
  thread->stack = thread->stack->tail;
  FbleReleaseValue(heap, stack->func);
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
  FbleThread thread;
  thread.stack = NULL;
  thread.allocator = FbleNewStackAllocator();
  thread.profile = profile == NULL ? NULL : FbleNewProfileThread(profile);

  FbleValue* result = NULL;
  FbleExecStatus status = FbleThreadCall(heap, &thread, &result, func, args);
  switch (status) {
    case FBLE_EXEC_CONTINUED: {
      UNREACHABLE("unexpected status");
      break;
    }

    case FBLE_EXEC_FINISHED: {
      assert(thread.stack == NULL);
      break;
    }

    case FBLE_EXEC_ABORTED: {
      assert(thread.stack == NULL);
      assert(result == NULL);
      break;
    }
  }

  FbleFreeStackAllocator(thread.allocator);
  FbleFreeProfileThread(thread.profile);
  return result;
}

// FbleThreadCall -- see documentation in execute.h
FbleExecStatus FbleThreadCall(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, FbleValue** args)
{
  FbleFuncInfo info = FbleFuncValueInfo(func);

  FbleExecutable* executable = info.executable;

  FbleRetainValue(heap, func);

  PushStackFrame(func, result, executable->num_locals, thread);

  for (size_t i = 0; i < executable->num_args; ++i) {
    thread->stack->locals[i] = args[i];
    FbleRetainValue(heap, args[i]);
  }

  if (thread->profile != NULL) {
    FbleProfileEnterBlock(thread->profile, info.profile_block_offset + executable->profile_block_id);
  }
  FbleExecStatus status = executable->run(
        heap, thread, executable, thread->stack->locals,
        info.statics, info.profile_block_offset);
  while (status == FBLE_EXEC_CONTINUED) {
    func = thread->stack->func;
    info = FbleFuncValueInfo(func);
    executable = info.executable;

    if (thread->profile != NULL) {
      FbleProfileReplaceBlock(thread->profile, info.profile_block_offset + executable->profile_block_id);
    }

    status = executable->run(
        heap, thread, executable, thread->stack->locals,
        info.statics, info.profile_block_offset);
  }

  if (thread->profile != NULL) {
    FbleProfileExitBlock(thread->profile);
  }

  return status;
}

// FbleThreadCall_ -- see documentation in execute.h
FbleExecStatus FbleThreadCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, ...)
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
  return FbleThreadCall(heap, thread, result, func, args);
}

// FbleThreadTailCall -- see documentation in execute.h
FbleExecStatus FbleThreadTailCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args)
{
  FbleFuncInfo info = FbleFuncValueInfo(func);
  FbleExecutable* executable = info.executable;
  FbleValue** result = thread->stack->result;

  PopStackFrame(heap, thread);
  PushStackFrame(func, result, executable->num_locals, thread);

  for (size_t i = 0; i < executable->num_args; ++i) {
    thread->stack->locals[i] = args[i];
  }

  return FBLE_EXEC_CONTINUED;
}

// FbleThreadTailCall_ -- see documentation in execute.h
FbleExecStatus FbleThreadTailCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...)
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

// FbleThreadReturn -- see documentation in fble-execute.h
FbleExecStatus FbleThreadReturn(FbleValueHeap* heap, FbleThread* thread, FbleValue* result)
{
  *(thread->stack->result) = result;
  PopStackFrame(heap, thread);
  return result == NULL ? FBLE_EXEC_ABORTED : FBLE_EXEC_FINISHED;
}

// FbleEval -- see documentation in fble-value.h
FbleValue* FbleEval(FbleValueHeap* heap, FbleValue* program, FbleProfile* profile)
{
  return FbleApply(heap, program, NULL, profile);
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleProfile* profile)
{
  FbleValue* result = Eval(heap, func, args, profile);
  return result;
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
