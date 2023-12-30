/**
 * @file fble-execute.h
 *  API for describing fble functions.
 */

#ifndef FBLE_EXECUTE_H_
#define FBLE_EXECUTE_H_

#include "fble-alloc.h"       // for FbleStackAllocator, etc.
#include "fble-module-path.h" // for FbleModulePath
#include "fble-profile.h"     // for FbleProfileThread
#include "fble-value.h"       // for FbleValue, FbleValueHeap

/**
 * Sentinel value to return from FbleRunFunction to indicate tail call.
 *
 * We use the value 0x2 to be distinct from NULL and packed values.
 */
#define FbleTailCallSentinelValue ((FbleValue*) 0x2)

/**
 * @func[FbleRunFunction] Implementation of fble function logic.
 *  Type of a C function that implements an fble function.
 *
 *  To perform a tail call, the implementation of the run function should
 *  place the function to call followed by args in order into the
 *  tail_call_buffer, then return FbleTailCallSentinelValue. The function to
 *  tail call must not be undefined.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   Profile thread for recording profiling information. NULL if profiling is
 *   disabled.
 *  @arg[FbleValue**] tail_call_buffer
 *   Pre-allocated space to store tail call func and args.
 *  @arg[FbleFunction*] function
 *   The function to execute.
 *  @arg[FbleValue**] args
 *   Arguments to the function. Borrowed.
 *
 *  @returns FbleValue*
 *   @i The result of executing the function.
 *   @i NULL if the function aborts.
 *   @i FbleTailCallSentinelValue to indicate tail call.
 *
 *  @sideeffects
 *   Executes the fble function, with whatever side effects that may have.
 */
typedef FbleValue* FbleRunFunction(
    FbleValueHeap* heap,
    FbleProfileThread* profile,
    FbleValue** tail_call_buffer,
    FbleFunction* function,
    FbleValue** args);

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
   * Number of value slots needed for the tail call buffer.
   *
   * The tail call buffer is used to pass function and arguments when making a
   * tail call.
   *
   * This will be 0 if there are no tail calls. It will be (1 + argc) for the
   * tail call with the most number of arguments, allowing sufficient space
   * to pass the function and all arguments for that tail call.
   */
  size_t tail_call_buffer_size;

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
 *  @arg[FbleExecutableProgram*][program] The program to free, may be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given program.
 */
void FbleFreeExecutableProgram(FbleExecutableProgram* program);

/**
 * @func[FbleCall] Calls an fble function.
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *  @arg[FbleFunction*] func
 *   The function to execute.
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
FbleValue* FbleCall(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* func, FbleValue** args);

/**
 * @func[FbleCall_] Calls an fble function using varags.
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *  @arg[FbleFunction*] func
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
FbleValue* FbleCall_(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* func, ...);

#endif // FBLE_EXECUTE_H_
