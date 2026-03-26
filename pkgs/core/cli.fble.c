/**
 * @file cli.fble.c
 *  Implementation of FbleCliMain and helper functions.
 */

#include "cli.fble.h"

#include <assert.h>           // for assert
#include <string.h>           // for strcmp

#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector.

#include "fble-cli.usage.h"        // for fbldUsageHelpText

#include "char.fble.h"          // for FbleCharValueAccess
#include "debug.fble.h"         // for /Core/Debug/Builtin%
#include "env.fble.h"           // for /Core/Env/Native%
#include "int.fble.h"           // for FbleNewIntValue, FbleIntValueAccess
#include "io.fble.h"            // for FbleIoM
#include "stdio.fble.h"         // for /Core/Stdio/FFI%
#include "string.fble.h"        // for FbleNewStringValue, FbleStringValueAccess

#define LIST_TAGWIDTH 1

static FbleValue* Cli(FbleValueHeap* heap, FbleProfile* profile, FbleValue* main, size_t argc, const char** argv);


/**
 * @func[Cli] Executes a @l{/Core/Cli%.Main@} function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleProfile*][profile] Profile to store execution results to.
 *  @arg[FbleValue*][main] The main program to execute. Borrowed.
 *  @arg[size_t][argc] The number of command line arguments. Borrowed.
 *  @arg[const char**][argv] The command line arguments. Borrowed.
 *
 *  @returns[FbleValue*]
 *   The @l{Exit@} result of executing the main program, or NULL in case of
 *   error.
 *
 *  @sideeffects
 *   @i Updates the profile.
 *   @i Allocates a value on the heap.
 *   @i Any side effects the main program itself has via native calls.
 */
static FbleValue* Cli(FbleValueHeap* heap, FbleProfile* profile, FbleValue* main, size_t argc, const char** argv)
{
  FbleValue* func = FbleEval(heap, main, profile);
  if (func == NULL) {
    return NULL;
  }

  FblePushFrame(heap);

  // We apply the main function with:
  //  M@ = <@ A@>(Unit@) { A@; }
  FbleValue* iom = FbleIoM(heap, profile);              // IoM@<M@>
  FbleValue* args = FbleCliArgs(heap, argc, argv);      // List@<String@>
  FbleValue* unit = FbleNewStructValue_(heap, 0);       // Unit@

  FbleValue* func_args[3] = { iom, args, unit };
  FbleValue* result = FbleApply(heap, func, 3, func_args, profile);
  return FblePopFrame(heap, result);
}

// See documentation in cli.fble.h
FbleValue* FbleCliArgs(FbleValueHeap* heap, int argc, const char** argv)
{
  FbleValue* argS = FbleNewEnumValue(heap, LIST_TAGWIDTH, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argP = FbleNewStructValue_(heap, 2, FbleNewStringValue(heap, argv[argc - i -1]), argS);
    argS = FbleNewUnionValue(heap, LIST_TAGWIDTH, 0, argP);
  }
  return argS;
}

// FbleCliMainStatus -- See documentation in cli.fble.h
FbleCliMainStatus FbleCliMainOtherStatus(FbleMainStatus status)
{
  assert(FBLE_MAIN_SUCCESS == 0);
  if (status == FBLE_MAIN_SUCCESS) {
    return 0;
  }
  return 112 + status;
}

// FbleCliMainAppStatus -- See documentation in cli.fble.h
FbleCliMainStatus FbleCliMainAppStatus(FbleValue* status)
{
  if (status == NULL) {
    return FbleCliMainOtherStatus(FBLE_MAIN_RUNTIME_ERROR);
  }

  int app_status = FbleIntValueAccess(status);
  if (app_status < 0 || app_status >= 112) {
    app_status = 112;
  }
  return app_status;
}

// FbleCliMain -- See documentation in cli.fble.h
FbleCliMainStatus FbleCliMain(int argc, const char** argv, FblePreloadedModule* preloaded)
{
  // To ease debugging of FbleCliMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  const char* profile_output_file = NULL;
  uint64_t profile_sample_period = 0;
  FbleValue* main = NULL;

  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Debug_2f_Builtin_25__2e_Trace);
  FbleRegisterForeignFunction(heap, &_Fble_2f_Core_2f_Env_2f_Native_25__2e_GetVar);
  FbleRegisterStdioForeignFunctions(heap);

  FblePreloadedModuleV builtins = { .size = 0 };
  FbleMainStatus status = FbleMain(NULL, NULL, "fble-cli", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &profile_sample_period, &main);

  FbleFreeVector(builtins);

  if (main == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return FbleCliMainOtherStatus(status);
  }

  FbleValue* value = Cli(heap, profile, main, argc, argv);

  int result = FbleCliMainAppStatus(value);

  FbleFreeValueHeap(heap);

  FbleOutputProfile(profile_output_file, profile, profile_sample_period);
  FbleFreeProfile(profile);
  return result;
}
