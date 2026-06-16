/**
 * @file cli.fble.h
 *  Routines for interacting with @l{/Std/Io/Cli%}.
 */

#ifndef FBLE_STD_CLI_FBLE_H_
#define FBLE_STD_CLI_FBLE_H_

#include <fble/fble-main.h>      // for FbleMainStatus
#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>   // for FbleProfile
#include <fble/fble-runtime.h>   // for FbleValue

/**
 * @type[FbleCliMainStatus]
 *  Status code from running a /Std/Io/Cli%.Main@ application.
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
 * @func[FbleCliMain] A main function for a @l{/Std/Io/Cli%.Main@} program.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The preloaded module to use as the @l{/Std/Io/Cli%.Main@} program to run,
 *   or NULL to determine the module based on command line options.
 *  @returns[FbleCliMainStatus] The exit status.
 *
 *  @sideeffects
 *   @i Application side effects from running the @l{/Std/Io/Cli%.Main@} program.
 *   @i Writes to a profile if specified by the command line options.
 */
FbleCliMainStatus FbleCliMain(int argc, const char** argv, FblePreloadedModule* preloaded);

/**
 * @func[FbleCliArgs] Helper for creating List@<String@> args.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[int][argc] Number of command line args.
 *  @arg[const char**][argv] The command line args.
 *  @returns[FbleValue*] List@<String@> for the command line args.
 *  @sideeffects Allocates values on the fble heap.
 */
FbleValue* FbleCliArgs(FbleValueHeap* heap, int argc, const char** argv);

#endif // FBLE_STD_CLI_FBLE_H_
