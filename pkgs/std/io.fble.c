/**
 * @file io.fble.c
 *  Implementation of FbleIoM and helper functions.
 */

#include "io.fble.h"

#include <assert.h>           // for assert

#include <fble/fble-runtime.h>   // for FbleValue, etc.

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
  if (a == NULL) {
    return NULL;
  }

  heap->tail_call_argc = 2;
  heap->tail_call_buffer[0] = f;
  heap->tail_call_buffer[1] = a;
  heap->tail_call_buffer[2] = u;
  return heap->tail_call_sentinel;
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
FbleValue* FbleIo(FbleValueHeap* heap, FbleProfile* profile)
{
  FbleName block_names[1];
  block_names[0].name = FbleNewString("IoRun!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleNameV names = { .size = 1, .xs = block_names };
  FbleBlockId block_id = FbleAddBlocksToProfile(profile, names);
  FbleFreeName(block_names[0]);

  static FbleExecutable run_exe = {
    .num_args = 1,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &RunImpl,
  };
  FbleValue* run = FbleNewFuncValue(heap, &run_exe, block_id, NULL);

  return FbleNewStructValue_(heap, 1, run);
}

// See documentation in io.fble.h
FbleValue* FbleIoMonad(FbleValueHeap* heap, FbleProfile* profile)
{
  FbleName block_names[2];
  block_names[0].name = FbleNewString("IoReturn!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("IoDo!");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleNameV names = { .size = 2, .xs = block_names };
  FbleBlockId block_id = FbleAddBlocksToProfile(profile, names);
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);

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

  FbleValue* type = FbleGenericTypeValue;
  return FbleNewStructValue_(heap, 3, type, monad_return, monad_do);
}
