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
 * @func[FbleRunFunction] Implementation of fble function logic.
 *  Type of a C function that implements an fble function.
 *
 *  To perform a tail call, the implementation of the run function should
 *  call and return the result of FbleTailCall.
 *
 *  @arg[FbleValueHeap*] heap
 *   The value heap.
 *  @arg[FbleProfileThread*] profile
 *   Profile thread for recording profiling information. NULL if profiling is
 *   disabled.
 *  @arg[FbleFunction*] function
 *   The function to execute.
 *  @arg[FbleValue**] args
 *   Arguments to the function. Borrowed.
 *
 *  @returns FbleValue*
 *   @i The result of executing the function.
 *   @i NULL if the function aborts.
 *   @i The result of calling FbleTailCall.
 *
 *  @sideeffects
 *   Executes the fble function, with whatever side effects that may have.
 */
typedef FbleValue* FbleRunFunction(
    FbleValueHeap* heap,
    FbleProfileThread* profile,
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

  /** How to run the function. See FbleRunFunction for more info. */
  FbleRunFunction* run;
};

/**
 * Information about an fble function.
 *
 * The statics are owned by whatever FbleValue object represents the function.
 * Don't try accessing them unless you know that FbleValue object is retained.
 *
 * The profile_block_id is the absolute profile block ID after linking modules
 * together.
 */
struct FbleFunction {
  FbleExecutable executable;
  FbleBlockId profile_block_id;
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

/**
 * @func[FbleTailCall] Creates a tail call.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleFunction*][function] The function to execute.
 *  @arg[FbleValue*][func] The function value to execute.
 *  @arg[size_t][argc] Number of arguments passed to the function.
 *  @arg[FbleValue**][args] Arguments to pass to the function. Borrowed.
 *
 *  @returns FbleValue*
 *   The result of the function call, or NULL in case of abort.
 *
 *  @sideeffects
 *   @i Allocates an opaque object representing a tail call.
 */
FbleValue* FbleTailCall(
    FbleValueHeap* heap,
    FbleFunction* function, FbleValue* func,
    size_t argc, FbleValue** args);

#endif // FBLE_EXECUTE_H_
