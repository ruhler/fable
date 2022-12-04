// interpret.h
//   Internal header for fble interpreter related functions.

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include <fble/fble-execute.h>

// FbleInterpreterRunFunction --
//   An standard run function that runs an fble function by interpreting the
//   instructions in its instruction block.
//
// See documentation of FbleRunFunction in execute.h.
FbleExecStatus FbleInterpreterRunFunction(
    FbleValueHeap* heap,
    FbleThread* thread,
    FbleExecutable* executable,
    FbleValue** locals,
    FbleValue** statics,
    FbleBlockId profile_block_offset);

#endif // FBLE_INTERNAL_INTERPRET_H_

