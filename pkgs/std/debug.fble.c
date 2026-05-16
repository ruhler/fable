/**
 * @file debug.fble.c
 *  Implementation of /Std/Stream/Debug% module FFI.
 */

#include <stddef.h>                  // for wchar_t
#include <stdio.h>                   // for fputwc
#include <wchar.h>                   // for fputwc

#include <fble/fble-function.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

#include "data.fble.h"               // for FbleCharValueAccess

/**
 * @func[PutChar] FbleRunFunction for PutChar foreign function.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Char@) { Unit@; }.
 *
 *  @sideeffects
 *   Writes a character to the debug stream.
 */
static FbleValue* PutChar(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  wchar_t c = FbleCharValueAccess(args[0]);
  fputwc(c, stderr);
  fflush(stderr);
  return FbleNewStructValue_(heap, 0);
}

FbleForeign _Fble_2f_Std_2f_Stream_2f_Debug_25__2e_PutChar = {
  .path = "/Std/Stream/Debug%",
  .name = "PutChar",
  .num_args = 1,
  .max_call_args = 0,
  .run = &PutChar,
};
