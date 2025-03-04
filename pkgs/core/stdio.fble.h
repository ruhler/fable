/**
 * @file stdio.fble.h
 *  Routines for interacting with @l{/Core/Stdio/IO%.StdioIO@}.
 */

#ifndef FBLE_CORE_STDIO_FBLE_H_
#define FBLE_CORE_STDIO_FBLE_H_

#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>
#include <fble/fble-value.h>

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
 * @func[FbleStdioMain] A main function for a @l{Stdio@} program.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The preloaded module to use as the @l{Stdio@} program to run, or NULL to
 *   determine the module based on command line options.
 *
 *  @returns[int]
 *   0 for True, 1 for False, 2 for usage error, 3 for other failure.
 *
 *  @sideeffects
 *   @item
 *    Runs the @l{Stdio@} program, which may interact with stdin, stdout, and
 *    stderr.
 *   @i Writes to a profile if specified by the command line options.
 */
int FbleStdioMain(int argc, const char** argv, FblePreloadedModule* preloaded);

/**
 * @value[_Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_] @l{/Core/Stdio/IO/Builtin%} implementation.
 *  @type[FblePreloadedModule]
 */
extern FblePreloadedModule _Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_;

#endif // FBLE_CORE_STDIO_FBLE_H_
