// execute.h --
//   Header file describing internal interface for working with execution of
//   fble programs.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_EXECUTE_H_
#define FBLE_INTERNAL_EXECUTE_H_

#include <stdbool.h>      // for bool

#include "fble-value.h"   // for FbleValueHeap

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

// Forward declaration of FbleThread type.
typedef struct FbleThread FbleThread;

// FbleThreadV --
//   A vector of threads.
typedef struct {
  size_t size;
  FbleThread** xs;
} FbleThreadV;

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

// FbleStandardRunFunction --
//   An standard run function that runs an fble function by interpreting the
//   instructions in its instruction block.
//
// See documentation of FbleRunFunction above.
FbleExecStatus FbleStandardRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);

#endif // FBLE_INTERNAL_EXECUTE_H_
