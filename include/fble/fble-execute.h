// fble-execute.h --
//   Public header file for fble execution related types and functions.

#ifndef FBLE_EXECUTE_H_
#define FBLE_EXECUTE_H_

#include "fble-alloc.h"       // for FbleStackAllocator, etc.
#include "fble-module-path.h" // for FbleModulePath
#include "fble-profile.h"     // for FbleProfileThread
#include "fble-value.h"       // for FbleValueHeap

// FbleExecutable --
//   Abstract type representing executable code.
//
// TODO: But we define the type below, so what's up? We should either make it
// abstract here and not define it below, or not make it abstract at all.
typedef struct FbleExecutable FbleExecutable;

#define FBLE_EXECUTABLE_MODULE_MAGIC 0x38333

/**
 * An executable module.
 *
 * Reference counted. Pass by pointer. Explicit copy and free required.
 *
 * Note: The magic field is set to FBLE_EXECUTABLE_MODULE_MAGIC and is used to
 * help detect double frees.
 */ 
typedef struct {
  size_t refcount;  /**< Current reference count. */
  size_t magic;     /**< FBLE_EXECUTABLE_MODULE_MAGIC. */

  /** the path to the module */
  FbleModulePath* path;

  /** list of distinct modules this module depends on. */
  FbleModulePathV deps;

  /**
   * Code to compute the value of the module.
   *
   * Suiteable for use in the body of a function that takes the computed
   * module values for each module listed in 'deps' as arguments to the
   * function.
   */
  FbleExecutable* executable;
} FbleExecutableModule;

/** A vector of FbleExecutableModule. */
typedef struct {
  size_t size;                /**< Number of elements. */
  FbleExecutableModule** xs;  /**< Elements. */
} FbleExecutableModuleV;

// FbleFreeExecutableModule --
//   Decrement the reference count and if appropriate free resources
//   associated with the given module.
//
// Inputs:
//   module - the module to free
//
// Side effects:
//   Frees resources associated with the module as appropriate.
void FbleFreeExecutableModule(FbleExecutableModule* module);

/**
 * An executable program.
 *
 * The program is represented as a list of executable modules in topological
 * dependency order. Later modules in the list may depend on earlier modules
 * in the list, but not the other way around.
 *
 * The last module in the list is the main program. The module path for the
 * main module is /%.
 */
typedef struct {
  FbleExecutableModuleV modules;  /**< Program modules. */
} FbleExecutableProgram;

// FbleFreeExecutableProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeExecutableProgram(FbleExecutableProgram* program);

// FbleExecStatus -- 
//   Status code used for returning status from running a function.
//
// FBLE_EXEC_CONTINUED is used in the case when a function needs to perform a
// tail call. In this case, the function pushes the tail call on the managed
// stack and returns FBLE_EXEC_CONTINUED. It is the callers responsibility to
// execute the function on top of the managed stack to completion before
// continuing itself.
typedef enum {
  FBLE_EXEC_CONTINUED,      // The function requires a continuation to be run.
  FBLE_EXEC_FINISHED,       // The function has finished running.
  FBLE_EXEC_ABORTED,        // The function has aborted.
} FbleExecStatus;

/**
 * Managed execution stack.
 *
 * Memory Management:
 *   Each thread owns its stack. The stack owns its tail.
 *
 *   The stack holds a strong reference to func and any non-NULL locals.
 *   'result' is a pointer to something that is initially NULL and expects to
 *   receive a strong reference to the return value.
 */
// Fields:
//
typedef struct FbleStack {
  /** the function being executed at this frame of the stack. */
  FbleValue* func;

  /** where to store the result of executing the current frame. */
  FbleValue** result;

  /** the next frame down in the stack. */
  struct FbleStack* tail;

  /** array of local variables. Size is func->executable->locals. */
  FbleValue* locals[];
} FbleStack;

/**
 * A thread of execution.
 */
typedef struct FbleThread {
  /** the execution stack. */
  FbleStack* stack;
  FbleStackAllocator* allocator;

  /**
   * The profile associated with this thread.
   * May be NULL to disable profiling.
   */
  FbleProfileThread* profile;
} FbleThread;

// FbleThreadCall --
//   Call an fble function.
//
// Inputs:
//   result - where to store a strong reference to the result of executing the
//            function.
//   thread - the thread whose stack to push the frame on to.
//   func - the function to execute. Borrowed.
//   args - arguments to pass to the function. length == func->argc. Borrowed.
//
// Results:
//   The status of the execution. FBLE_EXEC_CONTINUED will never be returned.
//
// Side effects:
// * Updates the threads stack.
// * Enters a profiling block for the function being called.
// * Executes the called function to completion, returning the result.
FbleExecStatus FbleThreadCall(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, FbleValue** args);

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
// Results:
//   The status of the execution. FBLE_EXEC_CONTINUED will never be returned.
//
// Side effects:
// * Updates the threads stack.
// * Enters a profiling block for the function being called.
// * Executes the called function to completion, returning the result.
FbleExecStatus FbleThreadCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue** result, FbleValue* func, ...);

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
// Results:
//   FBLE_EXEC_CONTINUED.
//
// Side effects:
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The func and all args have their ownership transferred to
//   FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
//   doing an FbleReleaseValue call for func and args.
// * Replaces the profiling block for the function being called.
FbleExecStatus FbleThreadTailCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args);

// FbleThreadTailCall_ --
//   Replace the current frame with a new one.
//
// Inputs:
//   heap - the value heap.
//   func - the function to execute. Consumed.
//   thread - the thread with the stack to change.
//   ... - func->args number of args to the function. Args consumed.
//
// Results:
//   FBLE_EXEC_CONTINUED.
//
// Side effects:
// * Exits the current frame, which potentially frees any instructions
//   belonging to that frame.
// * The func and all args have their ownership transferred to
//   FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
//   doing an FbleReleaseValue call for func and args.
// * Replaces the profiling block for the function being called.
FbleExecStatus FbleThreadTailCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...);

// FbleThreadReturn --
//   Return from the current frame on the thread's stack.
//
// Inputs:
//   heap - the value heap.
//   thread - the thread to do the return on.
//   result - the result to return. Consumed. May be NULL if aborting from the
//            function.
//
// Results:
//   FBLE_EXEC_FINISHED or FBLE_EXEC_ABORTED depending on whether the argument
//   is NULL.
//
// Side effects:
// * Sets the return result for the current stack frame and pops the frame off
//   the stack. 
// * Exits the current profiling block.
// * Takes over ownership of result. FbleThreadReturn will call
//   FbleReleaseValue on the result on behalf of the caller when the result is
//   no longer needed.
FbleExecStatus FbleThreadReturn(FbleValueHeap* heap, FbleThread* thread, FbleValue* result);

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

#define FBLE_EXECUTABLE_MAGIC 0xB10CE

/**
 * Describes how to execute a function.
 *
 * FbleExecutable is a reference counted, partially abstract data type
 * describing how to execute a function.
 */
struct FbleExecutable {
  /** reference count. */
  size_t refcount;

  /** FBLE_EXECUTABLE_MAGIC */
  size_t magic;

  /** Number of args to the function. */
  size_t args;

  /** Number of static values used by the function. */
  size_t statics;

  /** Number of local values used by the function. */
  size_t locals;

  /**
   * Profiling block associated with this executable. Relative to the
   * function's profile_base_id.
   */
  FbleBlockId profile;

  /**
   * Profile blocks used in this FbleExecutable.
   *
   * Optional. This is intended to be used for FbleExecutable's representing
   * top level modules only.
   */
  FbleNameV profile_blocks;

  /** How to run the function. */
  FbleRunFunction* run;

  /**
   * Called to free this FbleExecutable.
   *
   * Subclasses of FbleExecutable should use this to free any custom state.
   */
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

#endif // FBLE_EXECUTE_H_
