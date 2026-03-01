/**
 * @file stdio.fble.h
 *  Implementation of @l{/Core/Stdio/Native%} module.
 */

#ifndef FBLE_CORE_STDIO_NATIVE_FBLE_H_
#define FBLE_CORE_STDIO_NATIVE_FBLE_H_

#include <fble/fble-program.h>   // for FblePreloadedModule

/**
 * @value[_Fble_2f_Core_2f_Stdio_2f_Native_] @l{/Core/Stdio/Native%} implementation.
 *  @type[FblePreloadedModule]
 */
extern FblePreloadedModule _Fble_2f_Core_2f_Stdio_2f_Native_25_;

/**
 * @func[FbleRegisterStdioForeignFunctions]
 * @ Registers foreign functions for /Core/Stdio/FFI%
 *  @arg[FbleValueHeap*][heap] The heap to register functions to.
 *  @sideeffects Registers all the /Core/Stdio/FFI% foreign functions.
 */
void FbleRegisterStdioForeignFunctions(FbleValueHeap* heap);

#endif // FBLE_CORE_STDIO_NATIVE_FBLE_H_
