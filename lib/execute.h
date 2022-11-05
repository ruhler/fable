// execute.h --
//   Header file describing internal interface for working with execution of
//   fble programs.
//
// This is an internal library interface.

#ifndef FBLE_INTERNAL_EXECUTE_H_
#define FBLE_INTERNAL_EXECUTE_H_

#include <fble/fble-execute.h>   // for FbleExecutable typedef.

#include <stdbool.h>      // for bool

#include <fble/fble-alloc.h>     // for FbleStackAllocator, etc.
#include <fble/fble-profile.h>   // for FbleProfileThread
#include <fble/fble-value.h>     // for FbleValueHeap

// FbleStack --
//
// Fields:
//   func - the function being executed at this frame of the stack.
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
  FbleValue** result;
  struct FbleStack* tail;
  FbleValue* locals[];
} FbleStack;

// FbleExecStatus -- 
//   Shared status code used for returning status from running a function or a
//   thread.
//
// FBLE_EXEC_CONTINUED is used in the case when a function needs to perform a
// tail call. In this case, the function pushes the tail call on the managed
// stack and returns FBLE_EXEC_CONTINUED. It is the callers responsibility to
// execute the function on top of the managed stack to completion before
// continuing itself. This status is only relevant for functions, not for
// threads.
//
// FBLE_EXEC_FINISHED may only be returned if the managed stack is identical
// to what it was before the function was called.
typedef enum {
  FBLE_EXEC_CONTINUED,      // The function requires a continuation to be run.
  FBLE_EXEC_FINISHED,       // The function/thread has finished running.
  FBLE_EXEC_ABORTED,        // Execution needs to be aborted.
} FbleExecStatus;

// FbleThread --
//   Represents a thread of execution.
//
// Fields:
//   stack - the execution stack.
//   profile - the profile thread associated with this thread. May be NULL to
//             disable profiling.
typedef struct FbleThread {
  FbleStack* stack;
  FbleStackAllocator* allocator;
  FbleProfileThread* profile;
} FbleThread;

// FbleThreadCall --
//   Push a frame onto the execution stack.
//
// Inputs:
//   result - where to store a strong reference to the result of executing the
//            function.
//   thread - the thread whose stack to push the frame on to.
//   func - the function to execute. Borrowed.
//   args - arguments to pass to the function. length == func->argc. Borrowed.
//
// Side effects:
// * Updates the threads stack.
// * Enters a profiling block for the function being called.
void FbleThreadCall(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, FbleValue** args);

// FbleThreadCall_ --
//   Push a frame onto the execution stack.
//
// Inputs:
//   result - where to store a strong reference to the result of executing the
//            function.
//   thread - the thread whose stack to push the frame on to.
//   func - the function to execute. Borrowed.
//   ... - func->argc number of arguments to pass to the function. Borrowed.
//
// Side effects:
// * Updates the threads stack.
// * Enters a profiling block for the function being called.
void FbleThreadCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, ...);

// FbleThreadTailCall --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute. Consumed.
//   thread - the thread with the stack to change.
//   args - args to the function. length == func->argc.
//          Array borrowed, elements consumed.
//
// Side effects:
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The func and all args have their ownership transferred to
//   FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
//   doing an FbleReleaseValue call for func and args.
// * Replaces the profiling block for the function being called.
void FbleThreadTailCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args);

// FbleThreadTailCall_ --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute. Consumed.
//   thread - the thread with the stack to change.
//   ... - func->args number of args to the function. Args consumed.
//
// Side effects:
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The func and all args have their ownership transferred to
//   FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
//   doing an FbleReleaseValue call for func and args.
// * Replaces the profiling block for the function being called.
void FbleThreadTailCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...);

// FbleThreadReturn --
//   Return from the current frame on the thread's stack.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to do the return on.
//   result - the result to return. Consumed. May be NULL if aborting from the
//            function.
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
//   A C function to run the fble function on the top of the thread stack.
//
// The FbleRunFunction should clean up and pop the stack frame before
// returning, regardless of whether the function completes, aborts, or needs a
// continuation. In case of continuation, a continuation stack frame is pushed
// after popping the current stack frame.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to run.
//
// Results:
//   The status of running the function.
//
// Side effects:
// * The fble function on the top of the thread stack is executed.
// * The stack frame is cleaned up and popped from the stack.
typedef FbleExecStatus FbleRunFunction(FbleValueHeap* heap, FbleThread* thread);

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
