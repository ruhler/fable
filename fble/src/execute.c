// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include "execute.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL
#include <string.h>   // for memset

#include "fble-alloc.h"     // for FbleAlloc, FbleFree, etc.
#include "fble-value.h"     // for FbleValue, etc.
#include "fble-vector.h"    // for FbleVectorInit, etc.

#include "heap.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

static void PopStackFrame(FbleValueHeap* heap, FbleThread* thread);
static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void AbortThreads(FbleValueHeap* heap, FbleThreadV* threads);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleFuncValue* func, FbleValue** args, FbleProfile* profile);

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

  for (size_t i = 0; i < stack->func->executable->locals; ++i) {
    assert(stack->locals[i] == NULL);
  }
  FbleReleaseValue(heap, &stack->func->_base);
  FbleFree(stack);
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
//   FBLE_EXEC_FINISHED - if the thread has finished running.
//   FBLE_EXEC_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - not used.
//   FBLE_EXEC_ABORTED - if execution should be aborted.
//
// Side effects:
// * The thread is executed, updating its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity)
{
  FbleExecStatus status = FBLE_EXEC_FINISHED;
  while (status == FBLE_EXEC_FINISHED && thread->stack != NULL) {
    if (thread->stack->joins > 0) {
      thread->stack->joins--;
      thread->stack = NULL;
      return FBLE_EXEC_FINISHED;
    }

    status = thread->stack->func->executable->run(heap, threads, thread, io_activity);
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
  for (size_t i = 0; i < threads->size; ++i) {
    FbleThread* thread = threads->xs[i];
    while (thread->stack != NULL) {
      if (thread->stack->joins > 0) {
        thread->stack->joins--;
        thread->stack = NULL;
        break;
      }


      thread->stack->func->executable->abort(heap, thread->stack);
      PopStackFrame(heap, thread);
    }

    FbleFreeProfileThread(thread->profile);
    FbleFree(thread);
  }
  threads->size = 0;
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
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleFuncValue* func, FbleValue** args, FbleProfile* profile)
{
  FbleThreadV threads;
  FbleVectorInit(threads);

  FbleThread* main_thread = FbleAlloc(FbleThread);
  main_thread->stack = NULL;
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
        case FBLE_EXEC_FINISHED: {
          unblocked = true;
          assert(thread->stack == NULL);
          FbleFreeProfileThread(thread->profile);
          FbleFree(thread);

          threads.xs[i] = threads.xs[threads.size - 1];
          threads.size--;
          i--;
          break;
        }

        case FBLE_EXEC_BLOCKED: {
          break;
        }

        case FBLE_EXEC_YIELDED: {
          unblocked = true;
          break;
        }

        case FBLE_EXEC_RUNNING: UNREACHABLE("unexpected status");
        case FBLE_EXEC_ABORTED: {
          AbortThreads(heap, &threads);
          FbleReleaseValue(heap, result);
          FbleFree(threads.xs);
          return NULL;
        }
      }
    }

    bool blocked = !unblocked;
    if (!io->io(io, heap, blocked) && blocked) {
      FbleString* source = FbleNewString(__FILE__);
      FbleLoc loc = { .source = source, .line = 0, .col = 0 };
      FbleReportError("deadlock\n", loc);
      FbleFreeString(source);

      AbortThreads(heap, &threads);
      FbleReleaseValue(heap, result);
      FbleFree(threads.xs);
      return NULL;
    }
  }

  // Give a chance to process any remaining io before exiting.
  io->io(io, heap, false);
  FbleFree(threads.xs);
  return result;
}

// FbleThreadCall -- see documentation in execute.h
void FbleThreadCall(FbleValueHeap* heap, FbleValue** result, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  size_t locals = func->executable->locals;

  FbleStack* stack = FbleAllocExtra(FbleStack, locals * sizeof(FbleValue*));
  stack->joins = 0;
  stack->func = func;
  FbleRetainValue(heap, &func->_base);
  stack->pc = 0;
  stack->result = result;
  stack->tail = thread->stack;
  memset(stack->locals, 0, locals * sizeof(FbleValue*));

  for (size_t i = 0; i < func->executable->args; ++i) {
    stack->locals[i] = args[i];
    FbleRetainValue(heap, args[i]);
  }
  thread->stack = stack;
}

// FbleThreadTailCall -- see documentation in execute.h
void FbleThreadTailCall(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  size_t locals = func->executable->locals;

  FbleStack* stack = FbleAllocExtra(FbleStack, locals * sizeof(FbleValue*));
  stack->joins = 0;
  stack->func = func;
  stack->pc = 0;
  stack->result = thread->stack->result;
  stack->tail = thread->stack->tail;
  memset(stack->locals, 0, locals * sizeof(FbleValue*));

  for (size_t i = 0; i < func->executable->args; ++i) {
    stack->locals[i] = args[i];
  }

  PopStackFrame(heap, thread);
  thread->stack = stack;
}

// FbleThreadReturn -- see documentation in execute.h
void FbleThreadReturn(FbleValueHeap* heap, FbleThread* thread, FbleValue* result)
{
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
  assert(func->tag == FBLE_FUNC_VALUE);
  FbleIO io = { .io = &FbleNoIO, };
  FbleValue* result = Eval(heap, &io, (FbleFuncValue*)func, args, profile);
  return result;
}

// FbleNoIO -- See documentation in fble.h
bool FbleNoIO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  return false;
}

// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueHeap* heap, FbleIO* io, FbleValue* proc, FbleProfile* profile)
{
  assert(proc->tag == FBLE_PROC_VALUE);
  return Eval(heap, io, (FbleFuncValue*)proc, NULL, profile);
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
{}

// FbleFreeExecutableProgram -- see documentation in fble-execute.h
void FbleFreeExecutableProgram(FbleExecutableProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleExecutableModule* module = program->modules.xs + i;
      FbleFreeModulePath(module->path);
      for (size_t j = 0; j < module->deps.size; ++j) {
        FbleFreeModulePath(module->deps.xs[j]);
      }
      FbleFree(module->deps.xs);
      FbleFreeExecutable(module->executable);
    }
    FbleFree(program->modules.xs);
    FbleFree(program);
  }
}
