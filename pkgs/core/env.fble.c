/**
 * @file env.fble.c
 *  Implementation of /Core/Env/Native% foreign functions.
 */

#include "env.fble.h"

#include <stdlib.h>									 // for getenv
#include <string.h>                  // for strchr

#include <fble/fble-alloc.h>         // for FbleFree
#include <fble/fble-function.h>
#include <fble/fble-value.h>         // for FbleValue, etc.

#include "string.fble.h"             // For FbleNewStringValue.

#define MAYBE_TAGWIDTH 1

/**
 * @func[GetVarImpl] FbleRunFunction to read environment variables.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Native@<M@>, Monad@<M@>, String@, Unit@) { Maybe@<String@>; }
 */
static FbleValue* GetVarImpl(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  char* var = FbleStringValueAccess(args[2]);
  char* value = getenv(var);
  FbleFree(var);

  if (value == NULL) {
    return FbleNewEnumValue(heap, MAYBE_TAGWIDTH, 1); // Nothing
  }

  FbleValue* str = FbleNewStringValue(heap, value);
  return FbleNewUnionValue(heap, MAYBE_TAGWIDTH, 0, str); // Just(str)
}

// See documentation in env.fble.h
FbleExecutable _Fble_2f_Core_2f_Env_2f_Native_25__2e_GetVar = {
  .num_args = 4,
  .num_statics = 0,
  .max_call_args = 0,
  .run = &GetVarImpl,
};
