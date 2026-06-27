/**
 * @file cli.fble.c
 *  Implementation of FbleCliMain and helper functions.
 */

#include "cli.fble.h"

#include <assert.h>           // for assert
#include <string.h>           // for strcmp

#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-program.h>     // for FblePreloadedModule
#include <fble/fble-runtime.h>     // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector.

#include "fble-cli.usage.h"        // for fbldUsageHelpText

#include "data.fble.h"          // for FbleCharValueAccess, etc.
#include "debug.fble.h"         // for /Std/Stream/Debug%
#include "env.fble.h"           // for /Std/Io/Env%
#include "io.fble.h"            // for FbleIoM
#include "stdio.fble.h"         // for /Std/Io/File/Internal%

static FbleValue* Cli(FbleRuntime* runtime, FbleValue* main, size_t argc, const char** argv);


/**
 * @func[Cli] Executes a @l{/Std/Io/Cli%.Main@} function.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @arg[FbleValue*][main] The main program to execute. Borrowed.
 *  @arg[size_t][argc] The number of command line arguments. Borrowed.
 *  @arg[const char**][argv] The command line arguments. Borrowed.
 *
 *  @returns[FbleValue*]
 *   The @l{Exit@} result of executing the main program, or NULL in case of
 *   error.
 *
 *  @sideeffects
 *   @i Updates the runtime profile.
 *   @i Allocates a value on the runtime.
 *   @i Any side effects the main program itself has via native calls.
 */
static FbleValue* Cli(FbleRuntime* runtime, FbleValue* main, size_t argc, const char** argv)
{
  FbleValue* func = FbleEval(runtime, main);
  if (func == NULL) {
    return NULL;
  }

  FblePushFrame(runtime);

  // We apply the main function with:
  //  M@ = <@ A@>(Unit@) { A@; }
  FbleValue* m = FbleIoMonad(runtime);            // Monad@<M@>
  FbleValue* io = FbleIo(runtime);                // Io@<M@>
  FbleValue* args = FbleCliArgs(runtime, argc, argv);      // List@<String@>
  FbleValue* unit = FbleNewStructValue_(runtime, 0);       // Unit@

  FbleValue* func_args[4] = { m, io, args, unit };
  FbleValue* result = FbleApply(runtime, func, 4, func_args);
  return FblePopFrame(runtime, result);
}

// See documentation in cli.fble.h
FbleValue* FbleCliArgs(FbleRuntime* runtime, int argc, const char** argv)
{
  FbleValue* items[argc];
  for (size_t i = 0; i < argc; ++i) {
    items[i] = FbleNewStringValue(runtime, argv[i]);
  }
  return FbleNewListValue(runtime, argc, items);
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

  FbleRuntime* runtime = FbleNewRuntime();
  const char* profile_output_file = NULL;
  uint64_t profile_sample_period = 0;
  FbleValue* main = NULL;

  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Stream_2f_Debug_25__2e_PutChar);
  FbleRegisterForeignValue(runtime, &_Fble_2f_Std_2f_Io_2f_Env_25__2e_GetVar);
  FbleRegisterStdioForeignValues(runtime);

  FbleMainStatus status = FbleMain(NULL, NULL, "fble-cli", fbldUsageHelpText,
      &argc, &argv, preloaded, runtime, &profile_output_file, &profile_sample_period, &main);

  if (main == NULL) {
    FbleFreeRuntime(runtime);
    return FbleCliMainOtherStatus(status);
  }

  FbleValue* value = Cli(runtime, main, argc, argv);

  int result = FbleCliMainAppStatus(value);

  FbleOutputProfile(profile_output_file, runtime->profile, profile_sample_period);
  FbleFreeRuntime(runtime);
  return result;
}
