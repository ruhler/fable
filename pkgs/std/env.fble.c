/**
 * @file env.fble.c
 *  Implementation of /Std/Io/Env% foreign functions.
 */

#include "env.fble.h"

#include <stdlib.h>									 // for getenv
#include <string.h>                  // for strchr

#include <fble/fble-alloc.h>         // for FbleFree
#include <fble/fble-function.h>
#include <fble/fble-runtime.h>       // for FbleValue, etc.

#include "data.fble.h"               // For FbleNewStringValue.


/**
 * @func[GetVarImpl] FbleRunFunction to read environment variables.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Unit@) { Maybe@<String@>; }
 */
static FbleValue* GetVarImpl(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  char* var = FbleStringValueAccess(args[0]);
  char* value = getenv(var);
  FbleFree(var);

  if (value == NULL) {
    return FbleNewMaybeValue(heap, NULL);
  }

  FbleValue* str = FbleNewStringValue(heap, value);
  return FbleNewMaybeValue(heap, str);
}

// See documentation in env.fble.h
FbleForeign _Fble_2f_Std_2f_Io_2f_Env_25__2e_GetVar = {
  .path = "/Std/Io/Env%",
  .name = "GetVar",
  .num_args = 2,
  .max_call_args = 0,
  .run = &GetVarImpl,
};
