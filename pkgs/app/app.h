/**
 * @file app.h
 *  Header for FbleAppMain.
 */
#ifndef FBLE_APP_APP_FBLE_H_
#define FBLE_APP_APP_FBLE_H_

#include <fble/fble-program.h>   // for FbleNativeModule

/**
 * @func[FbleAppMain] Main function for running an @l{App@} program.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FbleNativeModule*][module]
 *   The native module to use as the @l{App@} program to run, or NULL to
 *   determine the module based on command line options.
 *
 *  @returns[int]
 *   0 on success, 1 on failure, 2 for usage error.
 *
 *  @sideeffects
 *   @item
 *    Runs the @l{App@} program, which may interact with keyboard and time
 *    events and draw to a display.
 *   @i Writes to a profile if specified by the command line options.
 */
int FbleAppMain(int argc, const char** argv, FbleNativeModule* module);

#endif // FBLE_APP_APP_FBLE_H_

