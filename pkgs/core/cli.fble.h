/**
 * @file cli.fble.h
 *  Routines for interacting with @l{/Core/Cli%}.
 */

#ifndef FBLE_CORE_CLI_FBLE_H_
#define FBLE_CORE_CLI_FBLE_H_

#include <fble/fble-main.h>      // for FbleMainStatus
#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>   // for FbleProfile
#include <fble/fble-value.h>     // for FbleValue

/**
 * @func[FbleCli]
 * @ Executes a @l{/Core/Cli%.Main@} function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleProfile*][profile] Profile to store execution results to.
 *  @arg[FbleValue*][main] The main program to execute. Borrowed.
 *  @arg[size_t][argc] The number of command line arguments. Borrowed.
 *  @arg[FbleValue**][argv] The command line arguments. Borrowed.
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
FbleValue* FbleCli(FbleValueHeap* heap, FbleProfile* profile, FbleValue* main, size_t argc, FbleValue** argv);

/**
 * @type[FbleCliMainStatus]
 *  Status code from running a /Core/Cli%.Main@ application.
 *
 *  @i 0 for successful execution, app returned exit status 0.
 *  @i 1 through 111 for app returned exit status 1 through 111.
 *  @i 112 for app returned exit status outside of the range [0, 111].
 *  @i 112 +n for FbleMainStatus failure status n.
 */
typedef int FbleCliMainStatus;

/**
 * @func[FbleCliMainOtherStatus] Map FbleMainStatus exit code to FbleCliMain exit code.
 *  @arg[FbleMainStatus][status] The status code to convert.
 *  @returns[int] The converted status code.
 */
FbleCliMainStatus FbleCliMainOtherStatus(FbleMainStatus status);

/**
 * @func[FbleCliMainAppStatus] Map application exit code to FbleCliMain exit code.
 *  @arg[FbleValue*][status] The Int@ status result of the FbleCliMain function.
 *  @returns[FbleCliMainStatus] The converted status code.
 */
FbleCliMainStatus FbleCliMainAppStatus(FbleValue* result);

/**
 * @func[FbleCliMain] A main function for a @l{/Core/Cli%.Main@} program.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The preloaded module to use as the @l{/Core/Cli%.Main@} program to run,
 *   or NULL to determine the module based on command line options.
 *  @returns[FbleCliMainStatus] The exit status.
 *
 *  @sideeffects
 *   @i Application side effects from running the @l{/Core/Cli%.Main@} program.
 *   @i Writes to a profile if specified by the command line options.
 */
FbleCliMainStatus FbleCliMain(int argc, const char** argv, FblePreloadedModule* preloaded);

#endif // FBLE_CORE_CLI_FBLE_H_
