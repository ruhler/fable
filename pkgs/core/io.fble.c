/**
 * @file io.fble.c
 *  Implementation of FbleIoM and helper functions.
 */

#include "io.fble.h"

#include <assert.h>           // for assert

#include <fble/fble-value.h>       // for FbleValue, etc.

static FbleValue* ReturnImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* DoImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* RunImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);


/**
 * @func[ReturnImpl] FbleRunFunction for monadic 'return' on a pure monad.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(A@, Unit@) { A@; }}.
 *
 *  @sideeffects None.
 */
static FbleValue* ReturnImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  return args[0];
}

/**
 * @func[DoImpl] FbleRunFunction for monadic 'do' on a pure monad.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{((Unit@) { A@; }, (A@, Unit@) { B@; }, Unit@) { B@; }}.
 *
 *  @sideeffects None.
 */
static FbleValue* DoImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  FbleValue* ma = args[0];
  FbleValue* f = args[1];
  FbleValue* u = args[2];
  FbleValue* a = FbleCall(heap, profile, ma, 1, &u);
  FbleValue* fargs[2] = { a, u };
  return FbleCall(heap, profile, f, 2, fargs);
}

/**
 * @func[RunImpl] FbleRunFunction for Io@.run function
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(External@, Unit@) { A@; }}.
 *
 *  @sideeffects None.
 */
static FbleValue* RunImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  // External@ is exactly (Unit@) { A@; }, so we can just pass back the first
  // argument.
  return args[0];
}

// See documentation in io.fble.h
FbleValue* FbleIoM(FbleValueHeap* heap, FbleProfile* profile)
{
  FbleName block_names[3];
  block_names[0].name = FbleNewString("IoReturn!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("IoDo!");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("IoRun!");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleNameV names = { .size = 3, .xs = block_names };
  FbleBlockId block_id = FbleAddBlocksToProfile(profile, names);
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);
  FbleFreeName(block_names[2]);

  static FbleExecutable return_exe = {
    .num_args = 2,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &ReturnImpl,
  };
  FbleValue* monad_return = FbleNewFuncValue(heap, &return_exe, block_id, NULL);

  static FbleExecutable do_exe = {
    .num_args = 3,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &DoImpl,
  };
  FbleValue* monad_do = FbleNewFuncValue(heap, &do_exe, block_id + 1, NULL);

  static FbleExecutable run_exe = {
    .num_args = 1,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &RunImpl,
  };
  FbleValue* run = FbleNewFuncValue(heap, &run_exe, block_id + 2, NULL);

  FbleValue* type = FbleGenericTypeValue;
  FbleValue* monad = FbleNewStructValue_(heap, 3, type, monad_return, monad_do);
  FbleValue* io = FbleNewStructValue_(heap, 1, run);
  return FbleNewStructValue_(heap, 3, type, monad, io);
}
