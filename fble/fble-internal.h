// fble-internal.h --
//   This header file describes the internal interface used in the
//   implementation of the fble library.

#ifndef FBLE_INTERNAL_H_
#define FBLE_INTERNAL_H_

#include "fble.h"

// FbleFreeFuncValue --
//   Free memory associated with the given function value.
//
// Inputs: 
//   arena - the arena used for allocation.
//   value - the value to free.
//
// Results:
//   none.
//
// Side effects:
//   Frees all memory associated with the function value, including value
//   itself.
void FbleFreeFuncValue(FbleArena* arena, FbleFuncValue* value);

#endif // FBLE_INTERNAL_H_
