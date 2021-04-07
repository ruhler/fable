// execute.c --
//   Code for executing fble instructions, functions, and processes.

#include "execute.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL
#include <string.h>   // for memset

#include "fble.h"     // for FbleIO.
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

  FbleReleaseValue(heap, &stack->func->_base);
  for (size_t i = 0; i < stack->locals.size; ++i) {
    FbleReleaseValue(heap, stack->locals.xs[i]);
  }
  FbleFree(heap->arena, stack->locals.xs);
  FbleFree(heap->arena, stack);
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
      PopStackFrame(heap, thread);
    }

    FbleFreeProfileThread(heap->arena, thread->profile);
    FbleFree(heap->arena, thread);
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
  FbleArena* arena = heap->arena;
  FbleThreadV threads;
  FbleVectorInit(arena, threads);

  FbleThread* main_thread = FbleAlloc(arena, FbleThread);
  main_thread->stack = NULL;
  main_thread->profile = profile == NULL ? NULL : FbleNewProfileThread(arena, profile);
  FbleVectorAppend(arena, threads, main_thread);

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
          FbleFreeProfileThread(arena, thread->profile);
          FbleFree(arena, thread);

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
          FbleFree(arena, threads.xs);
          return NULL;
        }
      }
    }

    bool blocked = !unblocked;
    if (!io->io(io, heap, blocked) && blocked) {
      FbleString* source = FbleNewString(arena, __FILE__);
      FbleLoc loc = { .source = source, .line = 0, .col = 0 };
      FbleReportError("deadlock\n", loc);
      FbleFreeString(arena, source);

      AbortThreads(heap, &threads);
      FbleReleaseValue(heap, result);
      FbleFree(arena, threads.xs);
      return NULL;
    }
  }

  // Give a chance to process any remaining io before exiting.
  io->io(io, heap, false);
  FbleFree(arena, threads.xs);
  return result;
}

// FbleThreadCall -- see documentation in execute.h
void FbleThreadCall(FbleValueHeap* heap, FbleValue** result, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;

  size_t locals = func->localc;

  FbleStack* stack = FbleAlloc(arena, FbleStack);
  stack->joins = 0;
  stack->func = func;
  FbleRetainValue(heap, &func->_base);
  stack->pc = 0;
  stack->locals.size = func->localc;
  stack->locals.xs = FbleArrayAlloc(arena, FbleValue*, locals);
  memset(stack->locals.xs, 0, locals * sizeof(FbleValue*));
  stack->result = result;
  stack->tail = thread->stack;

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals.xs[i] = args[i];
    FbleRetainValue(heap, args[i]);
  }
  thread->stack = stack;
}

// FbleThreadTailCall -- see documentation in execute.h
void FbleThreadTailCall(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;
  FbleStack* stack = thread->stack;
  size_t old_localc = stack->func->localc;
  size_t localc = func->localc;

  // Take references to the function and arguments early to ensure we don't
  // drop the last reference to them before we get the chance.
  FbleRetainValue(heap, &func->_base);
  for (size_t i = 0; i < func->argc; ++i) {
    FbleRetainValue(heap, args[i]);
  }

  FbleReleaseValue(heap, &stack->func->_base);
  for (size_t i = 0; i < stack->locals.size; ++i) {
    FbleReleaseValue(heap, stack->locals.xs[i]);
  }

  stack->func = func;
  stack->locals.size = func->localc;
  stack->pc = 0;

  if (localc > old_localc) {
    FbleFree(arena, stack->locals.xs);
    stack->locals.xs = FbleArrayAlloc(arena, FbleValue*, localc);
  }
  memset(stack->locals.xs, 0, stack->locals.size * sizeof(FbleValue*));

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals.xs[i] = args[i];
  }
}

// FbleThreadReturn -- see documentation in execute.h
void FbleThreadReturn(FbleValueHeap* heap, FbleThread* thread, FbleValue* result)
{
  FbleRetainValue(heap, result);
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
