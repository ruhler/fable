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
#include "stdio.fble.h"         // for /Core/Stdio/Native%
#include "string.fble.h"        // for FbleNewStringValue, FbleStringValueAccess

#define LIST_TAGWIDTH 1

static FbleValue* ReturnImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* DoImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* Cli(FbleValueHeap* heap, FbleProfile* profile, FbleValue* main, size_t argc, const char** argv);


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
  FbleValue* native = FbleNewStructValue_(heap, 0);     // Native@<M@>
  FbleValue* monad = FbleCliNativeMonad(heap, profile); // Monad@<M@>
  FbleValue* args = FbleCliArgs(heap, argc, argv);      // List@<String@>
  FbleValue* unit = FbleNewStructValue_(heap, 0);       // Unit@

  FbleValue* func_args[4] = { native, monad, args, unit };
  FbleValue* result = FbleApply(heap, func, 4, func_args, profile);
  return FblePopFrame(heap, result);
}

// See documentation in cli.fble.h
FbleValue* FbleCliNativeMonad(FbleValueHeap* heap, FbleProfile* profile)
{
  FbleName block_names[2];
  block_names[0].name = FbleNewString("CliReturn!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("CliDo!");
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

  return FbleNewStructValue_(heap, 2, monad_return, monad_do);
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

  FblePreloadedModuleV builtins;
  FbleInitVector(builtins);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Debug_2f_Builtin_25_);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Stdio_2f_Native_25_);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Env_2f_Native_25_);

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
