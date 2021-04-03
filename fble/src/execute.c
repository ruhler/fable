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

static FbleExecStatus RunThread(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);
static void AbortThreads(FbleValueHeap* heap, FbleThreadV* threads);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleFuncValue* func, FbleValue** args, FbleProfile* profile);

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
      FbleReleaseValue(heap, &thread->stack->_base);
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
    FbleReleaseValue(heap, &threads->xs[i]->stack->_base);
    threads->xs[i]->stack = NULL;
    FbleFreeProfileThread(heap->arena, threads->xs[i]->profile);
    FbleFree(heap->arena, threads->xs[i]);
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

  FbleRefValue* final_result = FbleThreadCall(heap, func, args, main_thread);

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
          FbleReleaseValue(heap, &final_result->_base);
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
      FbleReleaseValue(heap, &final_result->_base);
      FbleFree(arena, threads.xs);
      return NULL;
    }
  }

  // Give a chance to process any remaining io before exiting.
  io->io(io, heap, false);

  assert(final_result->value != NULL);
  FbleValue* result = final_result->value;
  if (result != NULL) {
    FbleRetainValue(heap, result);
  }
  FbleReleaseValue(heap, &final_result->_base);
  FbleFree(arena, threads.xs);
  return result;
}

// FbleThreadCall -- see documentation in execute.h
FbleRefValue* FbleThreadCall(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;

  size_t locals = func->localc;

  FbleRefValue* result = FbleNewValue(heap, FbleRefValue);
  result->_base.tag = FBLE_REF_VALUE;
  result->value = NULL;

  FbleStackValue* stack = FbleNewValue(heap, FbleStackValue);
  stack->_base.tag = FBLE_STACK_VALUE;

  stack->joins = 0;

  stack->func = func;
  FbleValueAddRef(heap, &stack->_base, &func->_base);

  stack->pc = 0;

  stack->locals.size = func->localc;
  stack->locals.xs = FbleArrayAlloc(arena, FbleValue*, locals);
  memset(stack->locals.xs, 0, locals * sizeof(FbleValue*));

  stack->result = result;
  FbleValueAddRef(heap, &stack->_base, &result->_base);

  stack->tail = thread->stack;
  if (stack->tail != NULL) {
    FbleValueAddRef(heap, &stack->_base, &stack->tail->_base);
    FbleReleaseValue(heap, &stack->tail->_base);
  }

  for (size_t i = 0; i < func->argc; ++i) {
    stack->locals.xs[i] = args[i];
    FbleValueAddRef(heap, &stack->_base, args[i]);
  }
  thread->stack = stack;
  return result;
}

// FbleThreadTailCall -- see documentation in execute.h
void FbleThreadTailCall(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread)
{
  FbleArena* arena = heap->arena;
  FbleStackValue* stack = thread->stack;
  size_t old_localc = stack->func->localc;
  size_t localc = func->localc;

  // Take references to the function and arguments early to ensure we don't
  // drop the last reference to them before we get the chance.
  FbleValueAddRef(heap, &stack->_base, &func->_base);
  for (size_t i = 0; i < func->argc; ++i) {
    FbleValueAddRef(heap, &stack->_base, args[i]);
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
