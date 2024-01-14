/**
 * @file interpret.h
 *  Header for fble interpreter related functions.
 */

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include <fble/fble-function.h>   // for FbleFunction, etc.

/**
 * An FbleRunFunction for interpreting FbleCode fble bytecode.
 * See documentation of FbleRunFunction in fble-function.h.
 */
FbleValue* FbleInterpreterRunFunction(
    FbleValueHeap* heap,
    FbleProfileThread* profile,
    FbleValue** tail_call_buffer,
    FbleFunction* function,
    FbleValue** args);

#endif // FBLE_INTERNAL_INTERPRET_H_
