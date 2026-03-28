/**
 * @file debug.fble.c
 *  Implementation of /Core/Debug/Builtin% module FFI.
 */

#include <fble/fble-function.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

#include "string.fble.h"             // For FbleDebugTrace

static FbleValue* Trace(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FbleDebugTrace(args[0]);
  return FbleNewStructValue_(heap, 0);
}

FbleForeign _Fble_2f_Core_2f_Debug_2f_Builtin_25__2e_Trace = {
  .path = "/Core/Debug/Builtin%",
  .name = "Trace",
  .num_args = 1,
  .max_call_args = 0,
  .run = &Trace,
};
