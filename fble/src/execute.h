// execute.h --
//   Header file describing internal interface for working with execution of
//   fble programs.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_EXECUTE_H_
#define FBLE_INTERNAL_EXECUTE_H_

#include <stdbool.h>      // for bool

#include "fble-profile.h"   // for FbleProfileThread
#include "fble-value.h"     // for FbleValueHeap

// FbleStack --
//
// Fields:
//   joins - the number of threads to wait for joining before resuming
//           execution of this frame of the stack.
//   func - the function being executed at this frame of the stack.
//   pc - the next instruction in func->code to execute.
//   result - where to store the result of executing the current frame.
//   tail - the next frame down in the stack.
//   localc - the number of local variables.
//   locals - array of local variables.
//
// Memory Management:
//   Each thread owns its stack. The stack owns its tail. Except that we have
//   a cactus stack structure where multiple threads can reuse parts of the
//   same stack. In that case, 'joins' will be greater than 0. The total
//   number of threads (or their stacks) that hold a reference to a stack
//   frame is 1 + joins.
//
//   The stack holds a strong reference to func and any non-NULL locals.
//   'result' is a pointer to something that is initially NULL and expects to
//   receive a strong reference to the return value.
typedef struct FbleStack {
  size_t joins;
  FbleFuncValue* func;
  size_t pc;
  FbleValue** result;
  struct FbleStack* tail;
  size_t localc;
  FbleValue* locals[];
} FbleStack;

// FbleExecStatus -- 
//   Shared status code used for returning status from running an instruction,
//   running a function or running a thread.
//
// Not all status options are relevant in all cases. See documentation for the
// particular function for details on how the status options are used.
typedef enum {
  FBLE_EXEC_FINISHED,       // The function has finished running.
  FBLE_EXEC_BLOCKED,        // The thread is blocked on I/O.
  FBLE_EXEC_YIELDED,        // The thread yielded, but is not blocked on I/O.
  FBLE_EXEC_RUNNING,        // The function is actively running.
  FBLE_EXEC_ABORTED,        // Execution needs to be aborted.
} FbleExecStatus;

// FbleThread --
//   Represents a thread of execution.
//
// Fields:
//   stack - the execution stack.
//   profile - the profile thread associated with this thread. May be NULL to
//             disable profiling.
typedef struct {
  FbleStack* stack;
  FbleProfileThread* profile;
} FbleThread;

// FbleThreadV --
//   A vector of threads.
typedef struct {
  size_t size;
  FbleThread** xs;
} FbleThreadV;

// FbleThreadCall --
//   Push a frame onto the execution stack.
//
// Inputs:
//   dest - where to store a strong reference to the result of executing the
//          function.
//   func - the function to execute. Borrowed.
//   args - arguments to pass to the function. length == func->argc. Borrowed.
//   thread - the thread whose stack to push the frame on to.
//
// Side effects:
//   Updates the threads stack.
void FbleThreadCall(FbleValueHeap* heap, FbleValue** result, FbleFuncValue* func, FbleValue** args, FbleThread* thread);

// FbleThreadTailCall --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute. Borrowed.
//   args - args to the function. length == func->argc. Borrowed.
//   thread - the thread with the stack to change.
//
// Side effects:
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The caller may assume it is safe for the function and arguments to be
//   retained solely by the frame that's being replaced when ReplaceFrame is
//   called.
void FbleThreadTailCall(FbleValueHeap* heap, FbleFuncValue* func, FbleValue** args, FbleThread* thread);

// FbleThreadReturn --
//   Return from the current frame on the thread's stack.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to do the return on.
//   result - the result to return. Borrowed.
//
void FbleThreadReturn(FbleValueHeap* heap, FbleThread* thread, FbleValue* result);

// FbleRunFunction --
//   A C function to run the fble function on the top of the thread stack to
//   completion or until it can no longer make progress.
//
// Inputs:
//   heap - the value heap.
//   threads - the thread list, for forking new threads.
//   thread - the thread to run.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//
// Results:
//   FBLE_EXEC_FINISHED - when the function is done running.
//   FBLE_EXEC_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EXEC_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EXEC_RUNNING - not used.
//   FBLE_EXEC_ABORTED - if execution should be aborted.
//
// Side effects:
// * The fble function on the top of the thread stack is executed, updating
//   its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
typedef FbleExecStatus FbleRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);

#endif // FBLE_INTERNAL_EXECUTE_H_
