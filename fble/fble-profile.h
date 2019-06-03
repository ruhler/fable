// fble-profile.h --
//   Header file describing profiling functionality for fble.

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

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
  size_t count;
  size_t time;
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
// The blocks and the callees and callers within blocks will all be sorted in
// increasing order of time.
typedef FbleBlockProfileV FbleProfile;

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

#endif // FBLE_PROFILE_H_
