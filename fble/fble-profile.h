// fble-profile.h --
//   Header file describing profiling functionality for fble.

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint64_t
#include <stdio.h>    // for FILE

#include "fble-alloc.h"
#include "fble-syntax.h"

// FbleProfileClock --
//   The clocks used for profiling.
typedef enum {
  // The profile time clock is advanced explicitly based on the time argument
  // to FbleProfileSample. It gives deterministic timing results.
  FBLE_PROFILE_TIME_CLOCK,

  // The profile wall clock is advanced implicitly based on wall clock time
  // that has passed since the previous sample. It is not deterministic, but
  // should give a reasonable approximation of real wall clock time.
  FBLE_PROFILE_WALL_CLOCK,

  // Enum whose value is the number of profiling clocks.
  FBLE_PROFILE_NUM_CLOCKS
} FbleProfileClock;

// FbleBlockId --
//  An identifier for a program block
typedef size_t FbleBlockId;

// FBLE_ROOT_BLOCK_ID --
//   The block id for the "root" block, which is assumed to be the entry block
//   for all threads.
#define FBLE_ROOT_BLOCK_ID 0

// FbleBlockIdV --
//   A vector of BlockId
typedef struct {
  size_t size;
  FbleBlockId* xs;
} FbleBlockIdV;

// FbleCallData --
//   Represents the number of calls and time spent when calling into or from
//   another block.
//
// Fields:
//   id - the id of the caller/callee block.
//   count - the number of times the call was made.
//   time - the amount of time spent in the call, indexed by FbleProfileClock.
typedef struct {
  FbleBlockId id;
  uint64_t count;
  uint64_t time[FBLE_PROFILE_NUM_CLOCKS];
} FbleCallData;

// FbleCallDataV --
//   A vector of FbleCallData
typedef struct {
  size_t size;
  FbleCallData** xs;
} FbleCallDataV;

// FbleBlockProfile -- 
//   Profile information for a particular block.
//
// Fields:
//   call - the id, summary count and time spent in this block.
//   callees - info about calls from this block into other blocks, sorted in
//             increasing order of callee. Only callees that have been called
//             from this block are included.
typedef struct {
  FbleCallData block;
  FbleCallDataV callees;
} FbleBlockProfile;

// FbleBlockProfileV --
//   A vector of FbleBlockProfile.
typedef struct {
  size_t size;
  FbleBlockProfile** xs;
} FbleBlockProfileV;

// FbleProfile --
//   Profiling information for a program.
//
// Fields:
//   ticks - the number of ticks that have occured in this profile.
//   last - the wall clock time of the last sample.
//   period - the numper of ticks per sample.
//   blocks - blocks.xs[i] contains block and callee information for block i.
typedef struct {
  uint64_t ticks;
  uint64_t wall;
  size_t period;
  FbleBlockProfileV blocks;
} FbleProfile;

// FbleNewProfile --
//   Creates a new, empty profile for the given number of blocks.
//
// Inputs:
//   arena - arena to use for allocations.
//   blockc - the number of blocks in the program being profiled. Must be
//            greater than 0, because FBLE_ROOT_BLOCK_ID should be a valid
//            block.
//   period - the sampling period in number of ticks per sample.
//
// Results:
//   A new empty profile.
//
// Side effects:
//   Allocates a new call that should be freed with FbleFreeProfile when
//   no longer in use.
FbleProfile* FbleNewProfile(FbleArena* arena, size_t blockc, size_t period);

// FbleFreeProfile --
//   Free a profile.
//
// Input:
//   arena - arena to use for allocations
//   profile - the profile to free
//
// Results:
//   none.
//
// Side effects:
//   Frees the memory resources associated with the given profile.
void FbleFreeProfile(FbleArena* arena, FbleProfile* profile);

// FbleProfileThread --
//   A thread of calls used to generate profile data.
typedef struct FbleProfileThread FbleProfileThread;

// FbleNewProfileThread --
//   Allocate a new profile thread.
//
//   If a parent thread is provided, the new thread starts with a copy of the
//   parent thread's call stack. Otherwise the new thread starts in the
//   FBLE_ROOT_BLOCK_ID block. The profile provided must be the same as the
//   profile associated with the parent thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   parent - the parent thread to fork from. May be NULL.
//   profile - the profile to save profiling data to.
//
// Results:
//   A new profile thread.
//
// Side effects:
//   Allocates a new profile thread that should be freed with
//   FreeProfileThread when no longer in use.
FbleProfileThread* FbleNewProfileThread(FbleArena* arena, FbleProfileThread* parent, FbleProfile* profile);

// FbleFreeProfileThread --
//   Free resources associated with the given profile thread. Does not free
//   the profile associated with the given profile thread.
//
// Inputs:
//   arena - arena to use for allocations
//   thread - thread to free
//
// Results:
//   none.
//
// Side effects:
//   Frees resources associated with the given profile thread.
void FbleFreeProfileThread(FbleArena* arena, FbleProfileThread* thread);

// FbleProfileEnterBlock -- 
//   Enter a block on the given profile thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the thread to do the call on.
//   block - the block to call into.
//
// Results:
//   none.
//
// Side effects:
//   A corresponding call to FbleProfileExitBlock or FbleProfileAutoExitBlock
//   should be made when the call leaves, for proper accounting and resource
//   management.
void FbleProfileEnterBlock(FbleArena* arena, FbleProfileThread* thread, FbleBlockId block);

// FbleProfileTick --
//   Advance the profile by a single tick, triggering a sample as appropriate
//   based on the sampling period.
// 
// Inputs:
//   arena - arena to use for allocations.
//   thread - the thread to sample, if sampling is required.
//
// Side effects:
//   Advances the profiling ticks by one. Takes a profiling sample if
//   appropriate.
void FbleProfileTick(FbleArena* arena, FbleProfileThread* thread);

// FbleProfileSample --
//   Take an explicit profiling sample.
//
// See FbleProfileTick for an alternate method of triggering samples.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the profile thread to sample.
//   time - the amount of profile time to advance.
//
// Side effects:
//   Charges calls on the current thread with the given explicit time and
//   implicit wall clock time since the last sample on the thread.
void FbleProfileSample(FbleArena* arena, FbleProfileThread* thread, uint64_t time);

// FbleProfileExitBlock --
//   Exits the current block on the given profile thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the thread to exit the call on.
//
// Results:
//   none.
//
// Side effects:
//   Updates the profile data associated with the given thread.
void FbleProfileExitBlock(FbleArena* arena, FbleProfileThread* thread);

// FbleProfileAutoExitBlock --
//   Arrange for the current block to exit the next time a callee of the block
//   exits. This provides a way to express tail call.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the thread to exit the call on.
//
// Results:
//   none.
//
// Side effects:
//   Updates the profile data associated with the given thread.
void FbleProfileAutoExitBlock(FbleArena* arena, FbleProfileThread* thread);

// FbleProfileReport --
//   Generate a human readable profile report.
//
// Inputs:
//   fout - the file to output the profile report to.
//   blocks - names and locations for the blocks of a program, indexed by
//            block id.
//   profile - the profile to generate a report for.
//
// Results:
//   none.
//
// Side effects:
//   Writes a profile report to the given file.
void FbleProfileReport(FILE* fout, FbleNameV* blocks, FbleProfile* profile);

// FbleFreeBlockNames --
//   Free the names for blocks.
//
// Inputs:
//   arena - arena to use for allocations
//   blocks - the names of blocks to free
//
// Results:
//   none.
//
// Side effects:
//   Frees the name strings and the underlying array for blocks.
void FbleFreeBlockNames(FbleArena* arena, FbleNameV* blocks);

#endif // FBLE_PROFILE_H_
