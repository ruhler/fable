/**
 * @file io.fble.h
 *  Routines for interacting with @l{/Core/Io%}.
 */

#ifndef FBLE_CORE_IO_FBLE_H_
#define FBLE_CORE_IO_FBLE_H_

#include <fble/fble-main.h>      // for FbleMainStatus
#include <fble/fble-program.h>   // for FblePreloadedModule
#include <fble/fble-profile.h>   // for FbleProfile
#include <fble/fble-value.h>     // for FbleValue

/**
 * @func[FbleIoM] Helper for creating a /Core/Io%.IoM@ instance.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleProfile*][profile] Profile to add blocks to.
 *  @returns[FbleValue*] An instance of IoM@.
 *  @sideeffects Adds blocks to profile.
 */
FbleValue* FbleIoM(FbleValueHeap* heap, FbleProfile* profile);

#endif // FBLE_CORE_IO_FBLE_H_
