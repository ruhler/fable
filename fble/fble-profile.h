// fble-profile.h --
//   Header file describing profiling functionality for fble.

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint64_t
#include <stdio.h>    // for FILE

#include "fble-alloc.h"
#include "fble-name.h"

// FbleBlockId --
//  An identifier for a program block
typedef size_t FbleBlockId;

// FBLE_ROOT_BLOCK_ID --
//   The block id for the "root" block, which is the initial block for new
//   threads.
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
//   time - the amount of time spent in the call.
typedef struct {
  FbleBlockId id;
  uint64_t count;
  uint64_t time;
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
//   name - the name of this block.
//   block - the id, summary count and time spent in this block.
//   callees - info about calls from this block into other blocks, sorted in
//             increasing order of callee. Only callees that have been called
//             from this block are included.
typedef struct {
  FbleName name;
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
//   last - the wall clock time of the last sample.
//   blocks - blocks.xs[i] contains block and callee information for block i.
typedef struct {
  FbleBlockProfileV blocks;
} FbleProfile;

// FbleNewProfile --
//   Creates a new profile with a single root block.
//
// Inputs:
//   arena - arena to use for allocations.
//
// Results:
//   A new profile with a single root block.
//
// Side effects:
//   Allocates a new profile that should be freed with FbleFreeProfile when
//   no longer in use.
FbleProfile* FbleNewProfile(FbleArena* arena);

// FbleProfileAddBlock --
//   Add a new block to the profile.
//
// Inputs:
//   arena - arena to use for allocations
//   profile - the profile to add the block to
//   name - the name of the block
//
// Results:
//   The id of the newly added block.
//
// Side effects:
// * Takes ownership of name, which will be freed when FbleFreeProfile is
// called.
FbleBlockId FbleProfileAddBlock(FbleArena* arena, FbleProfile* profile, FbleName name);

// FbleFreeProfile --
//   Free a profile.
//
// Input:
//   arena - arena to use for allocations
//   profile - the profile to free. May be NULL.
//
// Results:
//   none.
//
// Side effects:
//   Frees the memory resources associated with the given profile, including
//   the memory for the block names supplied to FbleProfileAddBlock.
void FbleFreeProfile(FbleArena* arena, FbleProfile* profile);

// FbleProfileThread --
//   A thread of calls used to generate profile data.
typedef struct FbleProfileThread FbleProfileThread;

// FbleNewProfileThread --
//   Allocate a new profile thread.
//
//   The new thread starts in the FBLE_ROOT_BLOCK_ID block. See
//   FbleForkProfileThread for an alternative way to create a new profile
//   thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   profile - the profile to save profiling data to.
//
// Results:
//   A new profile thread.
//
// Side effects:
//   Allocates a new profile thread that should be freed with
//   FreeProfileThread when no longer in use.
FbleProfileThread* FbleNewProfileThread(FbleArena* arena, FbleProfile* profile);

// FbleForkProfileThread --
//   Allocate a new profile thread.
//
//   The new thread starts with a copy of the parent thread's call stack.
//
// Inputs:
//   arena - arena to use for allocations.
//   parent - the parent thread to fork from.
//
// Results:
//   A new profile thread.
//
// Side effects:
//   Allocates a new profile thread that should be freed with
//   FreeProfileThread when no longer in use.
FbleProfileThread* FbleForkProfileThread(FbleArena* arena, FbleProfileThread* parent);

// FbleFreeProfileThread --
//   Free resources associated with the given profile thread. Does not free
//   the profile associated with the given profile thread.
//
// Inputs:
//   arena - arena to use for allocations
//   thread - thread to free. May be NULL.
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

// FbleProfileSample --
//   Take an explicit profiling sample.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the profile thread to sample.
//   time - the amount of profile time to advance.
//
// Side effects:
//   Charges calls on the current thread with the given time.
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
//   profile - the profile to generate a report for.
//
// Results:
//   none.
//
// Side effects:
//   Writes a profile report to the given file.
void FbleProfileReport(FILE* fout, FbleProfile* profile);

#endif // FBLE_PROFILE_H_
