/**
 * @file fble-profile.h
 *  Fble Profiling API.
 */

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint64_t
#include <stdio.h>    // for FILE

#include "fble-name.h"

/** Identifier for a program block. */
typedef size_t FbleBlockId;

/**
 * @struct[FbleBlockIdV] A vector of BlockId.
 *  @field[size_t][size] Number of elements.
 *  @field[FbleBlockid*][xs] Elements.
 */
typedef struct {
  size_t size;
  FbleBlockId* xs;
} FbleBlockIdV;

/**
 * @struct[FbleProfile] Profiling data collected for a program.
 *  A profile keeps track of the count of occurences and time spent in every
 *  call stack of the program. Because call stacks can be very long,
 *  particulary in the case of unbounded tail and non-tail recursion, call
 *  stacks are grouped together into a canonical form with cycles removed. We
 *  refer to these canonical form call stacks as call sequences.
 *
 *  For example, consider the call stack abcccdedef. The canonical form
 *  removes the cycles of c and the cycles of de, shortening to the call
 *  sequence abcdef.
 *
 *  Thus a profile keeps track of the count and time spent in every call
 *  sequences. It includes information about the names and locations of frames
 *  in the call sequence, which are called blocks.
 *
 *  @field[FbleNameV][blocks] The names of the profiling blocks.
 *  @field[bool][enabled] Indicates whether profiling is enabled or not.
 */
typedef struct {
  FbleNameV blocks;
  bool enabled;
  // Internal fields follow.
}  FbleProfile;

/**
 * @func[FbleNewProfile] Creates a new profile.
 *  Users are encouraged to create new profiles regardless of whether
 *  profiling is enabled or disabled, so that profiling will be available
 *  later if desired.
 *
 *  @arg[bool][enabled] True to enable profiling, false to disable profiling.
 *  @returns FbleProfile*
 *   A new profile with a single root block.
 *
 *  @sideeffects
 *   Allocates a new profile that should be freed with FbleFreeProfile() when
 *   no longer in use.
 */
FbleProfile* FbleNewProfile(bool enabled);

/**
 * @func[FbleAddBlockToProfile] Adds a block to the profile.
 *  Note: It is acceptable to add blocks in the middle of a profiling run.
 *
 *  @arg[FbleProfile*] profile
 *   The profile to add the block to. Must not be NULL.
 *  @arg[FbleName] name
 *   The name of the block
 *
 *  @returns FbleBlockId
 *   The id of the newly added block.
 *
 *  @sideeffects
 *   Takes ownership of name, which will be freed when FbleFreeProfile() is
 *   called.
 */
FbleBlockId FbleAddBlockToProfile(FbleProfile* profile, FbleName name);

/**
 * @func[FbleAddBlocksToProfile] Add blocks to the profile.
 *  Add multiple new block to the profile using a contiguous range of block
 *  ids.
 *
 *  Note: It is acceptable to add blocks in the middle of a profiling run.
 *
 *  @arg[FbleProfile*] profile
 *   The profile to add the blocks to. Must not be NULL.
 *  @arg[FbleNameV] names
 *   The names of the block to add. Borrowed.
 *
 *  @returns FbleBlockId
 *   The id of the first added block.
 *
 *  @sideeffects
 *   Adds blocks to the profile.
 */
FbleBlockId FbleAddBlocksToProfile(FbleProfile* profile, FbleNameV names);

/**
 * @func[FbleEnableProfiling] Turn on profiling for the given profile.
 *  @arg[FbleProfile*][profile] The profile to enable profiling on.
 *  @sideeffects Enables profiling on the profile.
 */
void FbleEnableProfiling(FbleProfile* profile);

/**
 * @func[FbleDisableProfiling] Turn off profiling for the given profile.
 *  @arg[FbleProfile*][profile] The profile to disable profiling on.
 *  @sideeffects
 *   Turns off profiling on the profile. Profiling operations involving the
 *   given profile will turn into no-ops.
 */
void FbleDisableProfiling(FbleProfile* profile);

/**
 * @func[FbleFreeProfile] Frees a profile.
 *  @arg[FbleProfile*][profile] The profile to free. Must not be NULL.
 *
 *  @sideeffects
 *   Frees the memory resources associated with the given profile, including
 *   the memory for the block names supplied to FbleAddBlockToProfile.
 */
void FbleFreeProfile(FbleProfile* profile);

/**
 * Profiling state for a running thread.
 *
 * By convention, a NULL value is used for FbleProfileThread* to indicate that
 * profiling is disabled.
 */
typedef struct FbleProfileThread FbleProfileThread;

/**
 * @func[FbleNewProfileThread] Allocates a new profile thread.
 *  The new thread starts in the FBLE_ROOT_BLOCK_ID block.
 *
 *  @arg[FbleProfile*][profile] The profile to save profiling data to.
 *
 *  @returns FbleProfileThread*
 *   A new profile thread. NULL if profiling is disabled.
 *
 *  @sideeffects
 *   Allocates a new profile thread that should be freed with
 *   FreeProfileThread when no longer in use.
 */
FbleProfileThread* FbleNewProfileThread(FbleProfile* profile);

/**
 * @func[FbleFreeProfileThread] Frees a profile thread.
 *  Frees resources associated with the given profile thread. Does not free
 *  the profile associated with the given profile thread.
 *
 *  @arg[FbleProfileThread*][thread] Thread to free. May be NULL.
 *
 *  @sideeffects
 *   Frees resources associated with the given profile thread.
 */
void FbleFreeProfileThread(FbleProfileThread* thread);

/**
 * @func[FbleProfileSample] Takes a profiling sample.
 *  @arg[FbleProfileThread*] thread
 *   The profile thread to sample. May be NULL.
 *  @arg[uint64_t] time
 *   The amount of profile time to advance.
 *
 *  @sideeffects
 *   Charges calls on the current thread with the given time.
 */
void FbleProfileSample(FbleProfileThread* thread, uint64_t time);

/**
 * @func[FbleProfileEnterBlock] Enters a profiling block.
 *  When calling into a function, use this function to tell the profiling
 *  logic what block is being called into.
 *
 *  @arg[FbleProfileThread*] thread
 *   The thread to do the call on. May be NULL.
 *  @arg[FbleBlockId] block
 *   The block to call into.
 *
 *  @sideeffects
 *   A corresponding call to FbleProfileExitBlock or FbleProfileReplaceBlock
 *   should be made when the call leaves, for proper accounting and resource
 *   management.
 */
void FbleProfileEnterBlock(FbleProfileThread* thread, FbleBlockId block);

/**
 * @func[FbleProfileReplaceBlock] Replaces a profiling block.
 *  When tail-calling into a function, use this function to tell the profiling
 *  logic what block is being called into.
 *
 *  @arg[FbleProfileThread*] thread
 *   The thread to do the call on. May be NULL.
 *  @arg[FbleBlockId] block
 *   The block to tail call into.
 *
 *  @sideeffects
 *   @i Replaces the current profiling block with the new block.
 *   @item
 *    Frees resources associated with the block being replaced, but a
 *    corresponding call to FbleProfileExitBlock or FbleProfileReplaceBlock
 *    will still be needed to free resources associated with the replacement
 *    block.
 */
void FbleProfileReplaceBlock(FbleProfileThread* thread, FbleBlockId block);

/**
 * @func[FbleProfileExitBlock] Exits a profiling block.
 *  When returning from a function, use this function to tell the profiling
 *  logic what block is being exited.
 *
 *  @arg[FbleProfileThread*][thread]
 *   The thread to exit the call on. May be NULL.
 *
 *  @sideeffects
 *   Updates the profile data associated with the given thread.
 */
void FbleProfileExitBlock(FbleProfileThread* thread);

/**
 * @func[FbleProfileBlockName] Gets the name for a profile block.
 *  @arg[FbleProfile*][profile] The profile.
 *  @arg[FbleBlockId][id] The id of the block.
 *  @returns[FbleName*]
 *   The name of the block with given id, NULL if not found. The returned name
 *   is a pointer into the profile that may be invalidated when adding blocks
 *   to the profile and will be invalidated when freeing the profile.
 *  @sideeffects None
 */
FbleName* FbleProfileBlockName(FbleProfile* profile, FbleBlockId id);

/**
 * @func[FbleLookupProfileBlockId] Looks up the block id of a named block.
 *  @arg[FbleProfile*][profile] The profile to look in.
 *  @arg[const char*][name] The name of the block to look up.
 *  @returns[FbleBlockId] The id of the block, 0 if not found.
 *  @sideeffects
 *   This is an expensive function intended for use in test code. Avoid using
 *   it in performance sensitive code, but it will have the side effect of
 *   making your code run very slowly.
 */
FbleBlockId FbleLookupProfileBlockId(FbleProfile* profile, const char* name);

/**
 * @func[FbleProfileQuery] Callback function for profile queries.
 *  The FbleQueryProfile function will call this function once for each unique
 *  canonical trace in the profile.
 *
 *  @arg[FbleProfile*][profile] The profile being queried.
 *  @arg[void*][userdata] User data passed to FbleQueryProfile.
 *  @arg[FbleBlockIdV][seq]
 *   A canonical trace in the profile. This memory will not last beyond the
 *   call to the query function, make copies if needed.
 *  @arg[uint64_t][count] The number of times called into this trace.
 *  @arg[uint64_t][time] The total time spent at this trace.
 *  @sideeffects Updates fields of userdata as desired to query the profile.
 */
typedef void FbleProfileQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time);

/**
 * @func[FbleQueryProfile] Query the profile for trace counts and time.
 *  @arg[FbleProfile*][profile] The profile to query.
 *  @arg[FbleProfileQuery*][query] The profile query function.
 *  @arg[void*][userdata] User data to pass to the query function.
 *  @sideeffects
 *   Calls the query function repeatedly, once for each canonical trace in the
 *   profile.
 */
void FbleQueryProfile(FbleProfile* profile, FbleProfileQuery* query, void* userdata);

/**
 * @func[FbleOutputProfile] Outputs the profile in the fble profiling format.
 *  Has no effect if profiling is disabled.
 *
 *  The profile output format consists of lines describing the program blocks,
 *  followed by lines describing call sequence counts and times. For example:
 *
 *  @code[txt] @
 *   # Blocks: id name file line column
 *   1 Foo Foo.fble 12 4
 *   2 Bar Bar.fble 13 5
 *   3 Sludge Sludge.fble 14 6
 *
 *   # Call sequences: count time [block...]
 *   1 20 1 3 2
 *   1 30 1 3 3 
 *
 *  Comments in the file start with @l{#} and go to end of line and are
 *  otherwise ignored. An empty line separates the list of blocks from the
 *  list of call sequences.
 *
 *  @arg[FILE*][fout] The file to output the profile to.
 *  @arg[FbleProfile*][profile] The profile to output
 *  @sideeffects Outputs the profile to the given file.
 */
void FbleOutputProfile(FILE* fout, FbleProfile* profile);

#endif // FBLE_PROFILE_H_
