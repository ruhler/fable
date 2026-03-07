/**
 * @file stdio.fble.h
 *  Implementation of @l{/Core/Stdio/FFI%} module.
 */

#ifndef FBLE_CORE_STDIO_NATIVE_FBLE_H_
#define FBLE_CORE_STDIO_NATIVE_FBLE_H_

#include <fble/fble-value.h>   // for FbleValueHeap

/**
 * @func[FbleRegisterStdioForeignFunctions]
 * @ Registers foreign functions for /Core/Stdio/FFI%
 *  @arg[FbleValueHeap*][heap] The heap to register functions to.
 *  @sideeffects Registers all the /Core/Stdio/FFI% foreign functions.
 */
void FbleRegisterStdioForeignFunctions(FbleValueHeap* heap);

#endif // FBLE_CORE_STDIO_NATIVE_FBLE_H_
