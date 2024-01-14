/**
 * @file fble-function.h
 *  API for describing fble functions.
 */

#ifndef FBLE_EXECUTE_H_
#define FBLE_EXECUTE_H_

#include "fble-module-path.h" // for FbleModulePath
#include "fble-profile.h"     // for FbleProfileThread
#include "fble-value.h"       // for FbleValue

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
 *  tail_call_buffer, followed by NULL, then return FbleTailCallSentinelValue.
 *  The function to tail call may be undefined.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   Profile thread for recording profiling information. NULL if profiling is
 *   disabled.
 *  @arg[FbleValue**] tail_call_buffer
 *   Pre-allocated space to store tail call func, args, and terminated NULL.
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
 * Information needed to execute a function.
 */
struct FbleExecutable {
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
   * This will be 0 if there are no tail calls. It will be (2 + argc) for the
   * tail call with the most number of arguments, allowing sufficient space
   * to pass the function, all arguments, and NULL terminator for that tail
   * call.
   */
  size_t tail_call_buffer_size;

  /**
   * Profiling block associated with this executable. Relative to the
   * function's profile_block_offset.
   */
  FbleBlockId profile_block_id;

  /** How to run the function. See FbleRunFunction for more info. */
  FbleRunFunction* run;
};

/**
 * Information about an fble function.
 *
 * The statics are owned by whatever FbleValue object represents the function.
 * Don't try accessing them unless you know that FbleValue object is retained.
 */
struct FbleFunction {
  FbleExecutable executable;
  FbleBlockId profile_block_offset;
  FbleValue** statics;
};

/**
 * @func[FbleCall] Calls an fble function.
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   The current profile thread, or NULL if profiling is disabled.
 *  @arg[FbleValue*] func
 *   The function to execute. May be undefined.
 *  @arg[size_t] argc
 *   Number of arguments passed to the function.
 *  @arg[FbleValue**] args
 *   Arguments to pass to the function. Borrowed.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Updates the threads stack.
 *   @i Enters a profiling block for the function being called.
 *   @i Executes the called function to completion, returning the result.
 */
FbleValue* FbleCall(FbleValueHeap* heap, FbleProfileThread* profile, FbleValue* func, size_t argc, FbleValue** args);

#endif // FBLE_EXECUTE_H_
