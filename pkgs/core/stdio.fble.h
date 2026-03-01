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

extern FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetStdin;
extern FbleForeignFunction _Fble_2f_Core_2f_Stdio_2f_FFI_25__2e_GetChar;

#endif // FBLE_CORE_STDIO_NATIVE_FBLE_H_
