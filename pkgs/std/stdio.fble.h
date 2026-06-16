/**
 * @file stdio.fble.h
 *  Implementation of @l{/Std/Io/File/Internal%} FFI.
 */

#ifndef FBLE_STD_STDIO_NATIVE_FBLE_H_
#define FBLE_STD_STDIO_NATIVE_FBLE_H_

#include <fble/fble-runtime.h>   // for FbleValueHeap

/**
 * @func[FbleRegisterStdioForeignValues]
 * @ Registers foreign functions for /Std/Io/File/Internal%
 *  @arg[FbleValueHeap*][heap] The heap to register values to.
 *  @sideeffects Registers all the /Std/Io/File/Internal% foreign values.
 */
void FbleRegisterStdioForeignValues(FbleValueHeap* heap);

#endif // FBLE_STD_STDIO_NATIVE_FBLE_H_
