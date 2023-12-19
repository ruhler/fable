/**
 * @file interpret.h
 *  Header for fble interpreter related functions.
 */

#ifndef FBLE_INTERNAL_INTERPRET_H_
#define FBLE_INTERNAL_INTERPRET_H_

#include <fble/fble-execute.h>

/**
 * An FbleRunFunction for interpreting FbleCode fble bytecode.
 *
 * @param heap        The value heap.
 * @param tail_call_buffer  The tail call buffer.
 * @param executable  The FbleCode to run.
 * @param args        Arguments to the function. Borrowed.
 * @param statics     The function's static variables. Borrowed.
 * @param profile_block_offset  The function profile block offset.
 * @param profile  The profile thread, or NULL if profiling is enabled.
 *
 * @returns
 * * The result of executing the function.
 * * NULL if the function aborts.
 * * A special sentinal value to indicate a tail call is required.
 *
 * @sideeffects
 * * May call FbleThreadTailCall to indicate tail call required.
 * * Executes the fble function, with whatever side effects that may have.
 */
FbleValue* FbleInterpreterRunFunction(
    FbleValueHeap* heap,
    FbleValue** tail_call_buffer,
    FbleExecutable* executable,
    FbleValue** args,
    FbleValue** statics,
    FbleBlockId profile_block_offset,
    FbleProfileThread* profile);

#endif // FBLE_INTERNAL_INTERPRET_H_

