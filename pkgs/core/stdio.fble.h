/**
 * @file stdio.fble.h
 *  Routines for interacting with @l{/Core/Stdio/IO%}.
 */

#ifndef FBLE_CORE_STDIO_FBLE_H_
#define FBLE_CORE_STDIO_FBLE_H_

#include <fble/fble-main.h>      // for FbleMainStatus
#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>   // for FbleProfile
#include <fble/fble-value.h>     // for FbleValue

/**
 * @func[FbleStdio]
 * @ Executes a @l{/Core/Stdio/IO%.Run(/Core/Stdio%.Main@)} function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleProfile*][profile] Profile to store execution results to.
 *  @arg[FbleValue*][stdio] The stdio process to execute. Borrowed.
 *  @arg[size_t][argc]
 *   The number of string arguments to the stdio function. Borrowed.
 *  @arg[FbleValue**][argv]
 *   The string arguments to the stdio function. Borrowed.
 *
 *  @returns[FbleValue*]
 *   The @l{Bool@} result of executing the stdio process, or NULL in case of
 *   error.
 *
 *  @sideeffects
 *   @i Updates the profile.
 *   @i Allocates a value on the heap.
 *   @item
 *    Performs input and output on stdin, stdout, and stderr according to the
 *    execution of the @l{Stdio@} process.
 */
FbleValue* FbleStdio(FbleValueHeap* heap, FbleProfile* profile, FbleValue* stdio, size_t argc, FbleValue** argv);

/**
 * @type[FbleStdioMainStatus]
 *  Status code from running a /Core/Stdio%.Main@ application.
 *
 *  @i 0 for successful execution, app returned exit status 0.
 *  @i 1 through 111 for app returned exit status 1 through 111.
 *  @i 112 for app returned exit status outside of the range [0, 111].
 *  @i 112 +n for FbleMainStatus failure status n.
 */
typedef int FbleStdioMainStatus;

/**
 * @func[FbleStdioMainOtherStatus] Map FbleMainStatus exit code to FbleStdioMain exit code.
 *  @arg[FbleMainStatus][status] The status code to convert.
 *  @returns[int] The converted status code.
 */
FbleStdioMainStatus FbleStdioMainOtherStatus(FbleMainStatus status);

/**
 * @func[FbleStdioMainAppStatus] Map application exit code to FbleStdioMain exit code.
 *  @arg[FbleValue*][status] The Int@ status result of the FbleStdioMain function.
 *  @returns[FbleStdioMainStatus] The converted status code.
 */
FbleStdioMainStatus FbleStdioMainAppStatus(FbleValue* result);

/**
 * @func[FbleStdioMain] A main function for a @l{Stdio@} program.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The preloaded module to use as the @l{Stdio@} program to run, or NULL to
 *   determine the module based on command line options.
 *  @returns[FbleStdioMainStatus] The exit status.
 *
 *  @sideeffects
 *   @item
 *    Runs the @l{Stdio@} program, which may interact with stdin, stdout, and
 *    stderr.
 *   @i Writes to a profile if specified by the command line options.
 */
FbleStdioMainStatus FbleStdioMain(int argc, const char** argv, FblePreloadedModule* preloaded);

/**
 * @value[_Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_] @l{/Core/Stdio/IO/Builtin%} implementation.
 *  @type[FblePreloadedModule]
 */
extern FblePreloadedModule _Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_;

#endif // FBLE_CORE_STDIO_FBLE_H_
