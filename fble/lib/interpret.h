// interpret.h
//   Internal header for fble interpreter related functions.

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include "execute.h"

// FbleInterpreterRunFunction --
//   An standard run function that runs an fble function by interpreting the
//   instructions in its instruction block.
//
// See documentation of FbleRunFunction in execute.h.
FbleExecStatus FbleInterpreterRunFunction(FbleValueHeap* heap, FbleThread* thread);

// FbleInterpreterAbortFunction --
//   A standard abort function for interpreted code.
//
// See documentation of FbleAbortFunction in execute.h
void FbleInterpreterAbortFunction(FbleValueHeap* heap, FbleStack* stack);

#endif // FBLE_INTERNAL_INTERPRET_H_

