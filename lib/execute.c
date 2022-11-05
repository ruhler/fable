// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include "execute.h"

#include <assert.h>   // for assert
#include <stdarg.h>   // for va_list, va_start, va_end
#include <stdlib.h>   // for NULL

#include <fble/fble-alloc.h>     // for FbleAlloc, FbleFree, etc.
#include <fble/fble-value.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "heap.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

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
  FbleStack* stack = FbleStackAllocExtra(thread->allocator, FbleStack, locals * sizeof(FbleValue*));
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
  FbleStack* stack = thread->stack;
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
  FbleThreadCall(heap, &thread, &result, func, args);

  FbleExecStatus status = FBLE_EXEC_FINISHED;
  while ((status == FBLE_EXEC_FINISHED || status == FBLE_EXEC_CONTINUED) && thread.stack != NULL) {
    status = FbleFuncValueExecutable(thread.stack->func)->run(heap, &thread);
  }

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
void FbleThreadCall(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, FbleValue** args)
{
  FbleExecutable* executable = FbleFuncValueExecutable(func);
  if (thread->profile != NULL) {
    FbleProfileEnterBlock(thread->profile, FbleFuncValueProfileBaseId(func) + executable->profile);
  }

  FbleRetainValue(heap, func);
  size_t locals = executable->locals;

  PushStackFrame(func, result, locals, thread);

  for (size_t i = 0; i < executable->args; ++i) {
    thread->stack->locals[i] = args[i];
    FbleRetainValue(heap, args[i]);
  }
}

// FbleThreadCall_ -- see documentation in execute.h
void FbleThreadCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, ...)
{
  FbleExecutable* executable = FbleFuncValueExecutable(func);
  size_t argc = executable->args;
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, func);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  FbleThreadCall(heap, thread, result, func, args);
}

// FbleThreadTailCall -- see documentation in execute.h
void FbleThreadTailCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args)
{
  FbleExecutable* executable = FbleFuncValueExecutable(func);
  if (thread->profile != NULL) {
    FbleProfileReplaceBlock(thread->profile, FbleFuncValueProfileBaseId(func) + executable->profile);
  }

  size_t locals = executable->locals;
  FbleValue** result = thread->stack->result;

  PopStackFrame(heap, thread);
  PushStackFrame(func, result, locals, thread);

  for (size_t i = 0; i < executable->args; ++i) {
    thread->stack->locals[i] = args[i];
  }
}

// FbleThreadTailCall_ -- see documentation in execute.h
void FbleThreadTailCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...)
{
  FbleExecutable* executable = FbleFuncValueExecutable(func);
  size_t argc = executable->args;
  FbleValue* args[argc];
  va_list ap;
  va_start(ap, func);
  for (size_t i = 0; i < argc; ++i) {
    args[i] = va_arg(ap, FbleValue*);
  }
  va_end(ap);
  FbleThreadTailCall(heap, thread, func, args);
}

// FbleThreadReturn -- see documentation in execute.h
void FbleThreadReturn(FbleValueHeap* heap, FbleThread* thread, FbleValue* result)
{
  if (thread->profile != NULL) {
    FbleProfileExitBlock(thread->profile);
  }
  *(thread->stack->result) = result;
  PopStackFrame(heap, thread);
}

// FbleEval -- see documentation in fble.h
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
    for (size_t i = 0; i < executable->profile_blocks.size; ++i) {
      FbleFreeName(executable->profile_blocks.xs[i]);
    }
    FbleVectorFree(executable->profile_blocks);

    executable->on_free(executable);
    FbleFree(executable);
  }
}

// FbleExecutableNothingOnFree -- see documentation in execute.h
void FbleExecutableNothingOnFree(FbleExecutable* this)
{}

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
