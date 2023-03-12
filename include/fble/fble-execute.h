/**
 * @file fble-execute.h
 * API for describing fble functions.
 */

#ifndef FBLE_EXECUTE_H_
#define FBLE_EXECUTE_H_

#include "fble-alloc.h"       // for FbleStackAllocator, etc.
#include "fble-module-path.h" // for FbleModulePath
#include "fble-profile.h"     // for FbleProfileThread
#include "fble-value.h"       // for FbleValue, FbleValueHeap

/**
 * A thread of execution.
 *
 * This is an abstract handle for use with FbleThread* APIs used to implement
 * fble functions.
 */
typedef struct FbleThread FbleThread;

/**
 * @func[FbleRunFunction] Implementation of fble function logic.
 *
 *  Type of a C function that implements an fble function.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleThread*] thread
 *   The thread to run.
 *  @arg[FbleExecutable*] executable
 *   The FbleExecutable associated with the function.
 *  @arg[FbleValue**] args
 *   Arguments to the function. Borrowed.
 *  @arg[FbleValue**] statics
 *   The function's static variables. Borrowed.
 *  @arg[FbleBlockId] profile_block_offset
 *   The function profile block offset.
 *
 *  @returns FbleValue*
 *   @i The result of executing the function.
 *   @i NULL if the function aborts.
 *   @i A special sentinal value to indicate a tail call is required.
 *
 *  @sideeffects
 *   @i May call FbleThreadTailCall to indicate tail call required.
 *   @i Executes the fble function, with whatever side effects that may have.
 */
typedef FbleValue* FbleRunFunction(
    FbleValueHeap* heap,
    FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** args,
    FbleValue** statics,
    FbleBlockId profile_block_offset);

/**
 * Magic number used by FbleExecutable.
 */
#define FBLE_EXECUTABLE_MAGIC 0xB10CE

/**
 * Describes how to execute a function.
 *
 * FbleExecutable is a reference counted, partially abstract data type
 * describing how to execute a function. Users can subclass this type to
 * provide their own implementations for fble functions.
 */
struct FbleExecutable {
  /** reference count. */
  size_t refcount;

  /** FBLE_EXECUTABLE_MAGIC */
  size_t magic;

  /** Number of args to the function. */
  size_t num_args;

  /** Number of static values used by the function. */
  size_t num_statics;

  /**
   * Profiling block associated with this executable. Relative to the
   * function's profile_block_offset.
   */
  FbleBlockId profile_block_id;

  /** How to run the function. See FbleRunFunction for more info. */
  FbleRunFunction* run;

  /**
   * Called to free this FbleExecutable.
   *
   * Subclasses of FbleExecutable should use this to free any custom state.
   */
  void (*on_free)(struct FbleExecutable* this);
};

/**
 * @func[FbleFreeExecutable] Frees an FbleExecutable.
 *
 *  Decrements the refcount and, if necessary, frees resources associated with
 *  the given executable.
 *
 *  @arg[FbleExecutable*][executable] The executable to free. May be NULL.
 *
 *  @sideeffects
 *   Decrements the refcount and, if necessary, calls executable->on_free and
 *   free resources associated with the given executable.
 */
void FbleFreeExecutable(FbleExecutable* executable);

/**
 * @func[FbleExecutableNothingOnFree] No-op FbleExecutable on_free function.
 * 
 *  Implementation of a no-op FbleExecutable.on_free function.
 *
 *  @arg[FbleExecutable*][this] The FbleExecutable to free.
 *
 *  @sideeffects
 *   None.
 */
void FbleExecutableNothingOnFree(FbleExecutable* this);

/**
 * Magic number used by FbleExecutableModule.
 */
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
   *
   * executable->args must be the same as deps.size.
   * executable->statics must be 0.
   */
  FbleExecutable* executable;

  /**
   * Profile blocks used by functions in the module.
   *
   * This FbleExecutableModule owns the names and the vector.
   */
  FbleNameV profile_blocks;
} FbleExecutableModule;

/** A vector of FbleExecutableModule. */
typedef struct {
  size_t size;                /**< Number of elements. */
  FbleExecutableModule** xs;  /**< Elements. */
} FbleExecutableModuleV;

/**
 * @func[FbleFreeExecutableModule] Frees an FbleExecutableModule.
 *
 *  Decrements the reference count and if appropriate frees resources
 *  associated with the given module.
 *
 *  @arg[FbleExecutableModule*][module] The module to free
 *
 *  @sideeffects
 *   Frees resources associated with the module as appropriate.
 */
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

/**
 * @func[FbleFreeExecutableProgram] Frees an FbleExecutableProgram.
 *
 *  @arg[FbleExecutableProgram*][program] The program to free, may be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeExecutableProgram(FbleExecutableProgram* program);

/**
 * @func[FbleThreadCall] Calls an fble function.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleThread*] thread
 *   The thread whose stack to push the frame on to.
 *  @arg[FbleValue*] func
 *   The function to execute. Borrowed.
 *  @arg[FbleValue**] args
 *   Arguments to pass to the function. length == func->argc. Borrowed.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Updates the threads stack.
 *   @i Enters a profiling block for the function being called.
 *   @i Executes the called function to completion, returning the result.
 */
FbleValue* FbleThreadCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args);

/**
 * @func[FbleThreadCall_] Calls an fble function using varags.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleThread*] thread
 *   The thread whose stack to push the frame on to.
 *  @arg[FbleValue*] func
 *   The function to execute. Borrowed.
 *  @arg[...][]
 *   func->argc number of arguments to pass to the function. Borrowed.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Updates the threads stack.
 *   @i Enters a profiling block for the function being called.
 *   @i Executes the called function to completion, returning the result.
 */
FbleValue* FbleThreadCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...);

/**
 * @func[FbleThreadTailCall] Tail calls an fble function.
 *
 *  Replaces the current frame with a new one.
 *
 *  This function should only be called from an FbleRunFunction invoked by
 *  FbleThreadCall.
 *
 *  The run function should directly return the result of FbleThreadTailCall.
 *  It should not execute any other thread API functions after calling
 *  FbleThreadTailCall.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleThread*] thread
 *   The thread with the stack to change.
 *  @arg[FbleValue*] func 
 *   The function to execute. Consumed.  
 *  @arg[FbleValue**] args
 *   Args to the function. length == func->argc. Array borrowed, elements
 *   consumed.
 *
 *  @returns FbleValue*
 *   An special sentinal value to indicate tail call is required.
 *
 *  @sideeffects
 *   @i Exits the current frame, which potentially frees any instructions
 *    belonging to that frame.
 *   @i The func and all args have their ownership transferred to
 *    FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
 *    doing an FbleReleaseValue call for func and args.
 *   @i Replaces the profiling block for the function being called.
 */
FbleValue* FbleThreadTailCall(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, FbleValue** args);

/**
 * @func[FbleThreadTailCall_] Tail calls an fble function using varargs.
 *
 *  Replaces the current frame with a new one.
 *
 *  This function should only be called from an FbleRunFunction invoked by
 *  FbleThreadCall.
 *
 *  The run function should directly return the result of FbleThreadTailCall_.
 *  It should not execute any other thread API functions after calling
 *  FbleThreadTailCall_.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleThread*] thread
 *   The thread with the stack to change.
 *  @arg[FbleValue*] func
 *   The function to execute. Consumed.
 *  @arg[...] 
 *   func->args number of args to the function. Args consumed.
 *
 *  @returns FbleValue*
 *   A special sentinal value to indicate tail call is required.
 *
 *  @sideeffects
 *   @i Exits the current frame, which potentially frees any instructions
 *    belonging to that frame.
 *   @i The func and all args have their ownership transferred to
 *    FbleThreadTailCall, so that calling FbleThreadTailCall has the effect of
 *    doing an FbleReleaseValue call for func and args.
 *   @i Replaces the profiling block for the function being called.
 */
FbleValue* FbleThreadTailCall_(FbleValueHeap* heap, FbleThread* thread, FbleValue* func, ...);

/**
 * @func[FbleThreadSample] Takes a profiling sample on the thread.
 *
 *  @arg[FbleThread*][thread] The profile thread to sample.
 *
 *  @sideeffects
 *   @i Charges calls on the current thread with the given time.
 *   @i Has no effect if profiling is currently disabled.
 */
void FbleThreadSample(FbleThread* thread);

/**
 * @func[FbleThreadEnterBlock] Enters a profiling block.
 *
 *  When calling into a function, use this function to tell the profiling
 *  logic what block is being called into.
 *
 *  @arg[FbleThread*][thread] The thread to do the call on.
 *  @arg[FbleBlockId][block ] The block to call into.
 *
 *  @sideeffects
 *   @i A corresponding call to FbleThreadExitBlock or FbleThreadReplaceBlock
 *    should be made when the call leaves, for proper accounting and resource
 *    management.
 *   @i Has no effect if profiling is currently disabled.
 */
void FbleThreadEnterBlock(FbleThread* thread, FbleBlockId block);

/**
 * @func[FbleThreadReplaceBlock] Replaces a profiling block.
 *
 *  When tail-calling into a function, use this function to tell the profiling
 *  logic what block is being called into.
 *
 *  @arg[FbleThread*][thread] The thread to do the call on.
 *  @arg[FbleBlockId][block ] The block to tail call into.
 *
 *  @sideeffects
 *   @i Replaces the current profiling block with the new block.
 *   @i Frees resources associated with the block being replaced, but a
 *    corresponding call to FbleThreadExitBlock or FbleThreadReplaceBlock
 *    will still needed to free resources associated with the replacement
 *    block.
 *   @i Has no effect if profiling is currently disabled.
 */
void FbleThreadReplaceBlock(FbleThread* thread, FbleBlockId block);

/**
 * @func[FbleThreadExitBlock] Exits a profiling block.
 *
 *  When returning from a function, use this function to tell the profiling
 *  logic what block is being exited.
 *
 *  @arg[FbleThread*][thread] The thread to exit the call on.
 *
 *  @sideeffects
 *   @i Updates the profile data associated with the given thread.
 *   @i Has no effect if profiling is currently disabled.
 */
void FbleThreadExitBlock(FbleThread* thread);

#endif // FBLE_EXECUTE_H_
