/**
 * @file app.h
 *  Header for FbleAppMain.
 */
#ifndef FBLE_APP_APP_FBLE_H_
#define FBLE_APP_APP_FBLE_H_

#include <fble/fble-program.h>   // for FblePreloadedModule

#include "cli.fble.h"            // for FbleCliMainStatus

/**
 * @func[FbleAppMain] Main function for running an @l{App@} program.
 *  @arg[int][argc] The number of args.
 *  @arg[const char**][argv] The args.
 *  @arg[FblePreloadedModule*][preloaded]
 *   The preloaded module to use as the @l{App@} program to run, or NULL to
 *   determine the module based on command line options.
 *
 *  @returns[FbleStdioMainStatus] The app exit status.
 *
 *  @sideeffects
 *   @i Makes the program portable to all locales via setlocale.
 *   @item
 *    Runs the @l{App@} program, which may interact with keyboard and time
 *    events and draw to a display.
 *   @i Writes to a profile if specified by the command line options.
 */
FbleCliMainStatus FbleAppMain(int argc, const char** argv, FblePreloadedModule* preloaded);

#endif // FBLE_APP_APP_FBLE_H_

