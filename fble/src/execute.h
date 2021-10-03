// execute.h --
//   Header file describing internal interface for working with execution of
//   fble programs.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_EXECUTE_H_
#define FBLE_INTERNAL_EXECUTE_H_

#include <stdbool.h>      // for bool

#include "fble-alloc.h"     // for FbleStackAllocator, etc.
#include "fble-execute.h"   // for FbleExecutable typedef.
#include "fble-profile.h"   // for FbleProfileThread
#include "fble-value.h"     // for FbleValueHeap

// FbleStack --
//
// Fields:
//   func - the function being executed at this frame of the stack.
//   pc - the next instruction in func->code to execute.
//   result - where to store the result of executing the current frame.
//   tail - the next frame down in the stack.
//   locals - array of local variables. Size is func->executable->locals.
//
// Memory Management:
//   Each thread owns its stack. The stack owns its tail.
//
//   The stack holds a strong reference to func and any non-NULL locals.
//   'result' is a pointer to something that is initially NULL and expects to
//   receive a strong reference to the return value.
typedef struct FbleStack {
  FbleValue* func;
  size_t pc;
  FbleValue** result;
  struct FbleStack* tail;
  FbleValue* locals[];
} FbleStack;

// FbleExecStatus -- 
//   Shared status code used for returning status from running an instruction,
//   running a function or running a thread.
typedef enum {
  FBLE_EXEC_FINISHED,       // The function has finished running.
  FBLE_EXEC_BLOCKED,        // The thread is blocked on I/O.
  FBLE_EXEC_YIELDED,        // The thread yielded, but is not blocked on I/O.
  FBLE_EXEC_ABORTED,        // Execution needs to be aborted.
} FbleExecStatus;

// FbleThread --
//   Represents a thread of execution.
//
// Fields:
//   stack - the execution stack.
//   parent - the parent of this thread, NULL for the initial thread.
//   children - the number of child threads.
//   profile - the profile thread associated with this thread. May be NULL to
//             disable profiling.
typedef struct FbleThread {
  FbleStack* stack;
  FbleStackAllocator* allocator;
  struct FbleThread* parent;
  size_t children;
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
//   result - where to store a strong reference to the result of executing the
//            function.
//   func - the function to execute. Borrowed.
//   args - arguments to pass to the function. length == func->argc. Borrowed.
//   thread - the thread whose stack to push the frame on to.
//
// Side effects:
// * Updates the threads stack.
// * Enters a profiling block for the function being called.
void FbleThreadCall(FbleValueHeap* heap, FbleValue** result, FbleValue* func, FbleValue** args, FbleThread* thread);

// FbleThreadTailCall --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute. Consumed.
//   args - args to the function. length == func->argc.
//          Array borrowed, elements consumed.
//   thread - the thread with the stack to change.
//
// Side effects:
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The func and all args have their ownership transferred to
//   FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
//   doing an FbleReleaseValue call for func and args.
// * Replaces the profiling block for the function being called.
void FbleThreadTailCall(FbleValueHeap* heap, FbleValue* func, FbleValue** args, FbleThread* thread);

// FbleThreadFork --
//   Fork a new thread from the given parent thread.
//
// Inputs:
//   heap - the value heap.
//   threads - pool of threads to add the forked thread to.
//   parent - the thread to fork from.
//   result - where to store a strong reference to the result of executing the
//            function.
//   func - the function to execute. Borrowed.
//   args - arguments to pass to the function. length == func->argc. Borrowed.
//
// Side effects:
// * Allocates a new thread and adds it to the threads pool.
// * Increments the joins count on the parent thread.
void FbleThreadFork(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* parent, FbleValue** result, FbleValue* func, FbleValue** args);

// FbleThreadReturn --
//   Return from the current frame on the thread's stack.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to do the return on.
//   result - the result to return. Consumed.
//
// Side effects:
// * Sets the return result for the current stack frame and pops the frame off
//   the stack. 
// * Exits the current profiling block.
// * Takes over ownership of result. FbleThreadReturn will call
//   FbleReleaseValue on the result on behalf of the caller when the result is
//   no longer needed.
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
//   The status of running the function.
//
// Side effects:
// * The fble function on the top of the thread stack is executed, updating
//   its stack.
// * io_activity is set to true if the thread does any i/o activity that could
//   unblock another thread.
typedef FbleExecStatus FbleRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);

// FbleAbortFunction --
//   A C function to abort and clean up the fble function on the top of the
//   thread stack.
//
// Inputs:
//   heap - the value heap.
//   stack - the stack frame to clean up.
//
// Side effects:
// * Local variables and intermediate execution state should be freed.
typedef void FbleAbortFunction(FbleValueHeap* heap, FbleStack* stack);

// FbleExecutable --
//   A reference counted, partially abstract data type describing how to
//   execute a function.
//
// 'profile' is the profiling block id associated with execution of this
// executable, relative to the function profile_base_id.
//
// 'profile_blocks' is an optional list of names of profile blocks used in the
// FbleExecutable. This is intended to be used for FbleExecutable's
// representing top level modules only.
//
// The 'on_free' function is called passing this as an argument just before
// the FbleExecutable object is freed. Subclasses should use this to free any
// custom state.
#define FBLE_EXECUTABLE_MAGIC 0xB10CE
struct FbleExecutable {
  size_t refcount;            // reference count.
  size_t magic;               // FBLE_EXECUTABLE_MAGIC.
  size_t args;                // The number of arguments expected by the function.
  size_t statics;             // The number of statics used by the function.
  size_t locals;              // The number of locals used by the function.
  FbleBlockId profile;
  FbleNameV profile_blocks;
  FbleRunFunction* run;       // How to run the function.
  FbleAbortFunction* abort;   // How to abort the function.
  void (*on_free)(FbleExecutable* this);
};

// FbleFreeExecutable --
//   Decrement the refcount and, if necessary, free resources associated with
//   the given executable.
//
// Inputs:
//   executable - the executable to free. May be NULL.
//
// Side effects:
//   Decrements the refcount and, if necessary, calls executable->on_free and
//   free resources associated with the given executable.
void FbleFreeExecutable(FbleExecutable* executable);

// FbleExecutableNothingOnFree --
//   Implementation of a no-op FbleExecutable.on_free function.
//
// See documentation of FbleExecutable.on_free above.
void FbleExecutableNothingOnFree(FbleExecutable* this);

#endif // FBLE_INTERNAL_EXECUTE_H_
