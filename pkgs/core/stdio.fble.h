/**
 * @file stdio.fble.h
 *  Implementation of @l{/Core/Stdio/FFI%} module.
 */

#ifndef FBLE_CORE_STDIO_NATIVE_FBLE_H_
#define FBLE_CORE_STDIO_NATIVE_FBLE_H_

#include <fble/fble-value.h>   // for FbleValueHeap

/**
 * @func[FbleRegisterStdioForeignValues]
 * @ Registers foreign functions for /Core/Stdio/FFI%
 *  @arg[FbleValueHeap*][heap] The heap to register values to.
 *  @sideeffects Registers all the /Core/Stdio/FFI% foreign values.
 */
void FbleRegisterStdioForeignValues(FbleValueHeap* heap);

#endif // FBLE_CORE_STDIO_NATIVE_FBLE_H_
