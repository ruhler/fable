// interpret.h
//   Internal header for fble interpreter related functions.

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include "execute.h"

// FbleInterprerRunFunction --
//   An standard run function that runs an fble function by interpreting the
//   instructions in its instruction block.
//
// See documentation of FbleRunFunction in execute.h.
FbleExecStatus FbleInterpreterRunFunction(FbleValueHeap* heap, FbleThreadV* threads, FbleThread* thread, bool* io_activity);

#endif // FBLE_INTERNAL_INTERPRET_H_

