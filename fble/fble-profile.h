// fble-profile.h --
//   Header file describing profiling functionality for fble.

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

#include <stdint.h>   // for uint64_t
#include <stdio.h>    // for FILE

#include "fble-alloc.h"
#include "fble-syntax.h"

// FbleProfileClock --
//   The clocks used for profiling.
typedef enum {
  // The profile time clock is advanced with explicit calls to FbleProfileTime.
  // This should give a consistent result for every run, but is only as
  // accurate as the calls to FbleProfileTime.
  FBLE_PROFILE_TIME_CLOCK,

  // Wall clock. This should give a reasonably accurate wall clock time, but
  // will vary from run to run.
  FBLE_PROFILE_WALL_CLOCK,

  // Enum whose value is the number of profiling clocks.
  FBLE_PROFILE_NUM_CLOCKS
} FbleProfileClock;

// FbleBlockId --
//  An identifier for a program block
typedef size_t FbleBlockId;

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
//   recursive - dynamic state tracking whether this call is being made
//               recursively.
typedef struct {
  FbleBlockId id;
  uint64_t count;
  uint64_t time[FBLE_PROFILE_NUM_CLOCKS];
  bool recursive;
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
//   callers - info about calls from other blocks into this block.
//   callers - info about calls from this block into other blocks.
typedef struct {
  FbleCallData block;
  FbleCallDataV callers;
  FbleCallDataV callees;
} FbleBlockProfile;

// FbleBlockProfileV --
//   A vector of FbleBlockProfile.
typedef struct {
  size_t size;
  FbleBlockProfile** xs;
} FbleBlockProfileV;

// FbleCallGraph --
//   Call graph inforomation profile for a program.
//
// Initially as we run the program we populate a profile with information
// about time spent in each block and callees. The blocks are sorted by block
// id and callers are unsorted.
//
// After processing a call graph, information about callers is updated. Blocks
// are sorted in descending order of time, callers within blocks are sorted
// inascending order of time, and callees within blocks are sorted in
// descending order of time.
typedef FbleBlockProfileV FbleCallGraph;

// FbleNewCallGraph --
//   Creates a new, empty profile for the given number of blocks.
//
// Inputs:
//   arena - arena to use for allocations.
//   blockc - the number of blocks in the call graph.
//
// Results:
//   A new empty profile.
//
// Side effects:
//   Allocates a new call that should be freed with FbleFreeCallGraph when
//   no longer in use.
FbleCallGraph* FbleNewCallGraph(FbleArena* arena, size_t blockc);

// FbleFreeCallGraph --
//   Free a call graph.
//
// Input:
//   arena - arena to use for allocations
//   graph - the graph to free
//
// Results:
//   none.
//
// Side effects:
//   Frees the memory resources associated with the given call graph.
void FbleFreeCallGraph(FbleArena* arena, FbleCallGraph* graph);

// FbleProfileThread --
//   A thread of calls used to generate call graph data.
typedef struct FbleProfileThread FbleProfileThread;

// FbleNewProfileThread --
//   Allocate a new profile thread.
//
//   The thread is allocated in a suspended state. FbleResumeProfileThread
//   must be called before making any calls or advancing profile time in the
//   thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   graph - the call graph to save profiling data to.
//
// Results:
//   A new profile thread.
//
// Side effects:
//   Allocates a new profile thread that should be freed with
//   FreeProfileThread when no longer in use.
FbleProfileThread* FbleNewProfileThread(FbleArena* arena, FbleCallGraph* graph);

// FbleFreeProfileThread --
//   Free resources associated with the given profile thread. Does not free
//   the call graph associated with the given profile thread.
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

// FbleSuspendProfileThread --
//   This pauses the wall clock time on the thread, to avoid double counting
//   wall clock time for interleaved execution of threads.
//
// Inputs:
//   thread - the thread to suspend. May be NULL.
//
// Results:
//   none
//
// Side effects:
//   Pauses the wall clock time on the thread.
void FbleSuspendProfileThread(FbleProfileThread* thread);

// FbleResumeProfileThread --
//   Resumes wall clock time on the thread.
//
// Inputs:
//   thread - the thread to resume. May be NULL.
//
// Results:
//   none
//
// Side effects: 
//   Resumes wall clock time on the thread.
void FbleResumeProfileThread(FbleProfileThread* thread);

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

// FbleProfileTime --
//   Advances the FBLE_PROFILE_TIME_CLOCK for this thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the profile thread to spend time on
//   time - the amount of time to advance.
//
// Results:
//   none.
//
// Side effects:
//   Increments recorded time spent in the current call.
void FbleProfileTime(FbleArena* arena, FbleProfileThread* thread, uint64_t time);

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
//   Updates the call graph data associated with the given thread.
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
//   Updates the call graph data associated with the given thread.
void FbleProfileAutoExitBlock(FbleArena* arena, FbleProfileThread* thread);

// FbleProcessCallGraph --
//   Process the given call graph.
//
// Inputs:
//   arena - arena to use for allocations.
//   graph - the call graph to process.
//
// Results:
//   None.
//
// Side effects:
//   Updates callers information and resorts blocks, callers, and callees from
//   the graph.
void FbleProcessCallGraph(FbleArena* arena, FbleCallGraph* graph);

// FbleDumpProfile --
//   Dump human readable profiling information to a file.
//
// Inputs:
//   fout - the file to dump the profile to.
//   blocks - names and locations for the blocks of a program, indexed by
//            block id.
//   graph - the call graph to dump.
//
// Results:
//   none.
//
// Side effects:
//   Writes a profile dump to the given file.
void FbleDumpProfile(FILE* fout, FbleNameV* blocks, FbleCallGraph* graph);

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
