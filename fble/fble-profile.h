// fble-profile.h --
//   Header file describing profiling functionality for fble.

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

#include <stdint.h>   // for uint64_t
#include <stdio.h>    // for FILE

#include "fble-alloc.h"
#include "fble-syntax.h"

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

// FbleCallGraph --
//   A vector of FbleCallDataV.
//   xs[i] is an unordered list of callees from block i.
typedef struct {
  size_t size;
  FbleCallDataV* xs;
} FbleCallGraph;

// FbleProfileClock --
//   The clock to use for profiling.
typedef enum {
  // Use FbleProfileTime for the profiling clock.
  // This should give a consistent result for every run, but is only as
  // accurate as the calls to FbleProfileTime.
  FBLE_PROFILE_TIME_CLOCK,

  // Use wall clock time for the profiling clock.
  // This should give an accurate wall clock time, but will vary from run to
  // run.
  // TODO: add 'Suspend' and 'Resume' options to threads to fix double
  // counting issues with parallel process execution.
  FBLE_PROFILE_WALL_CLOCK,
} FbleProfileClock;

// FbleNewCallGraph --
//   Creates a new, empty call graph for the given number of blocks.
//
// Inputs:
//   arena - arena to use for allocations.
//   blockc - the number of blocks in the call graph.
//
// Results:
//   A new empty call graph.
//
// Side effects:
//   Allocates a new call graph that should be freed with FbleFreeCallGraph
//   when no longer in use.
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
//   Spend time on the current profile thread. Advances the
//   FBLE_PROFILE_TIME_CLOCK for this thread. Has no effect if
//   FBLE_PROFILE_WALL_CLOCK is selected for the thread.
//
// Inputs:
//   arena - arena to use for allocations.
//   thread - the profile thread to spend time on
//   time - the amount of time to record has having been spent in the current
//   call.
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

// FbleProfile --
//   A profile for a program.
//
// Blocks are sorted in descending order of time.
// Callers within blocks are sorted in ascending order of time.
// Callees within blocks are sorted in descending order of time.
typedef FbleBlockProfileV FbleProfile;

// FbleComputeProfile --
//   Compute the profile for a given call graph. Block id 0 is assumed to
//   exist and belong to a root node for the graph, such that all nodes of the
//   graph are reachable from the root node.
//
// Inputs:
//   arena - arena to use for allocations.
//   graph - the call graph to compute the profile of.
//
// Results:
//   A newly allocated profile for the given call graph.
//
// Side effects:
//   Removes double counting of time from cycles in the call graph.
//   Creates a new profile that should be freed with FbleFreeProfile when no
//   longer in use.
FbleProfile* FbleComputeProfile(FbleArena* arena, FbleCallGraph* graph);

// FbleFreeProfile --
//   Free memory associated with the given profile.
//
// Inputs:
//   arena - arena to use for allocations
//   profile - the profile to free
//
// Results:
//   none.
//
// Side effects:
//   Frees memory resources associated with the given profile.
void FbleFreeProfile(FbleArena* arena, FbleProfile* profile);

// FbleDumpProfile --
//   Dump human readable profiling information to a file.
//
// Inputs:
//   fout - the file to dump the profile to.
//   blocks - names and locations for the blocks of a program, indexed by
//            block id.
//   profile - the profile to dump.
//
// Results:
//   none.
//
// Side effects:
//   Writes a profile dump to the given file.
void FbleDumpProfile(FILE* fout, FbleNameV* blocks, FbleProfile* profile);

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
