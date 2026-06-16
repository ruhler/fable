/**
 * @file io.fble.h
 *  Routines for interacting with @l{/Std/Io%}.
 */

#ifndef FBLE_STD_IO_FBLE_H_
#define FBLE_STD_IO_FBLE_H_

#include <fble/fble-main.h>      // for FbleMainStatus
#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>   // for FbleProfile
#include <fble/fble-runtime.h>   // for FbleValue

/**
 * @func[FbleIo] Helper for creating a /Std/Io%.Io@<M@> instance.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @returns[FbleValue*] An instance of Io@.
 *  @sideeffects Adds blocks to profile.
 */
FbleValue* FbleIo(FbleRuntime* runtime);

/**
 * @func[FbleIoMonad] Helper for creating Io@'s Monad@ instance.
 *  @arg[FbleRuntime*][runtime] The runtime context.
 *  @returns[FbleValue*] The instance of Monad@ for use with Io@.
 *  @sideeffects Adds blocks to profile.
 */
FbleValue* FbleIoMonad(FbleRuntime* runtime);

#endif // FBLE_STD_IO_FBLE_H_
