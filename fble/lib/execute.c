// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include "execute.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fble-alloc.h"     // for FbleAlloc, FbleFree, etc.
#include "fble-value.h"     // for FbleValue, etc.
#include "fble-vector.h"    // for FbleVectorInit, etc.

#include "heap.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static void PushStackFrame(FbleValue* func, FbleValue** result, size_t locals, FbleThread* thread);
static void PopStackFrame(FbleValueHeap* heap, FbleThread* thread);
static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void AbortThreads(FbleValueHeap* heap, FbleThreadV* threads);
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
  stack->pc = 0;
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

// RunThread --
//   Run the given thread to completion or until it can no longer make
//   progress.
//
// Inputs:
//   heap - the value heap.
//   threads - the thread list, for forking threads.
//   thread - the thread to run.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//
// Results:
//   The status of running the thread.
//
// Side effects:
// * The thread is executed, updating its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleExecStatus status = FBLE_EXEC_FINISHED;
  while ((status == FBLE_EXEC_FINISHED || status == FBLE_EXEC_CONTINUED) && thread->stack != NULL) {
    status = FbleFuncValueExecutable(thread->stack->func)->run(heap, threads, thread, io_activity);
  }
  return status;
}

// AbortThreads --
//   Clean up threads.
//
// Inputs:
//   heap - the value heap.
//   threads - the threads to clean up.
//
// Side effects:
//   Cleans up the thread state.
static void AbortThreads(FbleValueHeap* heap, FbleThreadV* threads)
{
  while (threads->size > 0) {
    size_t blocked = 0;
    for (size_t i = 0; i < threads->size; ++i) {
      FbleThread* thread = threads->xs[i];

      if (thread->children > 0) {
        threads->xs[blocked] = thread;
        blocked++;
        continue;
      }

      while (thread->stack != NULL) {
        FbleFuncValueExecutable(thread->stack->func)->abort(heap, thread->stack);
        PopStackFrame(heap, thread);
      }

      FbleFreeStackAllocator(thread->allocator);
      FbleFreeProfileThread(thread->profile);
      if (thread->parent != NULL) {
        thread->parent->children--;
      }
      FbleFree(thread);
    }
    threads->size = blocked;
  }
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
  FbleThreadV threads;
  FbleVectorInit(threads);

  FbleThread* main_thread = FbleAlloc(FbleThread);
  main_thread->stack = NULL;
  main_thread->allocator = FbleNewStackAllocator();
  main_thread->parent = NULL;
  main_thread->children = 0;
  main_thread->profile = profile == NULL ? NULL : FbleNewProfileThread(profile);
  FbleVectorAppend(threads, main_thread);

  FbleValue* result = NULL;
  FbleThreadCall(heap, &result, func, args, main_thread);

  while (threads.size > 0) {
    bool unblocked = false;
    for (size_t i = 0; i < threads.size; ++i) {
      FbleThread* thread = threads.xs[i];
      FbleExecStatus status = RunThread(heap, &threads, thread, &unblocked);
      switch (status) {
        case FBLE_EXEC_CONTINUED: {
          UNREACHABLE("unexpected status");
          unblocked = true;
          break;
        }

        case FBLE_EXEC_FINISHED: {
          unblocked = true;
          assert(thread->stack == NULL);
          FbleFreeStackAllocator(thread->allocator);
          FbleFreeProfileThread(thread->profile);
          if (thread->parent != NULL) {
            thread->parent->children--;
          }
          FbleFree(thread);

          threads.xs[i] = threads.xs[threads.size - 1];
          threads.size--;
          i--;
          break;
        }

        case FBLE_EXEC_BLOCKED: {
          break;
        }

        case FBLE_EXEC_ABORTED: {
          AbortThreads(heap, &threads);
          FbleReleaseValue(heap, result);
          FbleFree(threads.xs);
          return NULL;
        }
      }
    }
  }

  FbleFree(threads.xs);
  return result;
}

// FbleThreadCall -- see documentation in execute.h
void FbleThreadCall(FbleValueHeap* heap, FbleValue** result, FbleValue* func, FbleValue** args, FbleThread* thread)
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

// FbleThreadTailCall -- see documentation in execute.h
void FbleThreadTailCall(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleThread* thread)
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
    FbleFree(executable->profile_blocks.xs);

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
    FbleFree(module->deps.xs);
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
    FbleFree(program->modules.xs);
    FbleFree(program);
  }
}
