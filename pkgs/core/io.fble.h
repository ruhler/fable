/**
 * @file io.fble.h
 *  Routines for interacting with @l{/Std/Io%}.
 */

#ifndef FBLE_CORE_IO_FBLE_H_
#define FBLE_CORE_IO_FBLE_H_

#include <fble/fble-main.h>      // for FbleMainStatus
#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>   // for FbleProfile
#include <fble/fble-value.h>     // for FbleValue

/**
 * @func[FbleIo] Helper for creating a /Std/Io%.Io@<M@> instance.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleProfile*][profile] Profile to add blocks to.
 *  @returns[FbleValue*] An instance of Io@.
 *  @sideeffects Adds blocks to profile.
 */
FbleValue* FbleIo(FbleValueHeap* heap, FbleProfile* profile);

/**
 * @func[FbleIoMonad] Helper for creating Io@'s Monad@ instance.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleProfile*][profile] Profile to add blocks to.
 *  @returns[FbleValue*] The instance of Monad@ for use with Io@.
 *  @sideeffects Adds blocks to profile.
 */
FbleValue* FbleIoMonad(FbleValueHeap* heap, FbleProfile* profile);

#endif // FBLE_CORE_IO_FBLE_H_
