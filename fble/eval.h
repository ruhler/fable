// eval.h --
//   Header file describing internal interface for working with evaluation of
//   fble programs.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_EVAL_H_
#define FBLE_INTERNAL_EVAL_H_

#include <stdbool.h>      // for bool

#include "fble-value.h"   // for FbleValueHeap

// FbleEvalStatus -- 
//   Shared status code used for returning status from running an instruction,
//   running a frame, running a thread, or running multiple threads.
//
// Not all status options are relevant in all cases. See documentation for the
// particular function for details on how the status options are used.
typedef enum {
  FBLE_EVAL_FINISHED,       // The thread has finished running.
  FBLE_EVAL_BLOCKED,        // The thread is blocked on I/O.
  FBLE_EVAL_YIELDED,        // The thread yielded, but is not blocked on I/O.
  FBLE_EVAL_RUNNING,        // The thread is actively running.
  FBLE_EVAL_CONTINUE,       // The frame has returned for its caller to invoke a tail
                            // call on behalf of the frame.
  FBLE_EVAL_ABORTED,        // The thread needs to be aborted.
} FbleEvalStatus;

// Forward declaration of FbleThread type.
typedef struct FbleThread FbleThread;

// FbleRunFunction --
//   A C function to run the fble function on the top of the thread stack to
//   completion or until it can no longer make progress.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to run.
//   io_activity - set to true if the thread does any i/o activity that could
//                 unblock another thread.
//
// Results:
//   FBLE_EVAL_FINISHED - if we have just returned from the current stack frame.
//   FBLE_EVAL_BLOCKED - if the thread is blocked on I/O.
//   FBLE_EVAL_YIELDED - if our time slice for executing instructions is over.
//   FBLE_EVAL_RUNNING - not used.
//   FBLE_EVAL_CONTINUE - to indicate the function has just been replaced by
//                        its tail call.
//   FBLE_EVAL_ABORTED - if the thread should be aborted.
//
// Side effects:
// * The fble function on the top of the thread stack is executed, updating
//   its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
typedef FbleEvalStatus FbleRunFunction(FbleValueHeap* heap, FbleThread* thread, bool* io_activity);

// FbleStandardRunFunction --
//   An standard run function that runs an fble function by interpreting the
//   instructions in its instruction block.
//
// See documentation of FbleRunFunction above.
FbleEvalStatus FbleStandardRunFunction(FbleValueHeap* heap, FbleThread* thread, bool* io_activity);

#endif // FBLE_INTERNAL_EVAL_H_
