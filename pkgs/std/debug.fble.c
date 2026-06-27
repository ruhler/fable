/**
 * @file debug.fble.c
 *  Implementation of /Std/Stream/Debug% module FFI.
 */

#include <stdio.h>                   // for fwrite, fflush

#include <fble/fble-function.h>
#include <fble/fble-runtime.h>       // for FbleValue, etc.

#include "data.fble.h"               // for FbleCharValueAccess

// See documentation in data.fble.c.
size_t FbleUtf8Encode(uint32_t c, char bytes[6]);

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
static FbleValue* PutChar(FbleRuntime* runtime, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  uint32_t c = FbleCharValueAccess(args[0]);
  char bytes[8];
  size_t n = FbleUtf8Encode(c, bytes);
  fwrite(bytes, sizeof(char), n, stderr);
  fflush(stderr);
  return FbleNewStructValue_(runtime, 0);
}

FbleForeign _Fble_2f_Std_2f_Stream_2f_Debug_25__2e_PutChar = {
  .path = "/Std/Stream/Debug%",
  .name = "PutChar",
  .num_args = 1,
  .max_call_args = 0,
  .run = &PutChar,
};
