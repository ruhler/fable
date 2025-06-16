/**
 * @file fble-profile.h
 *  Fble Profiling API.
 */

#ifndef FBLE_PROFILE_H_
#define FBLE_PROFILE_H_

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint64_t

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
 *  A profile keeps track of the number of calls into and samples recorded
 *  in every call stack of the program. Because call stacks can be very long,
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
 *  later if desired. Profiling on a profile can be disabled by setting the
 *  enabled field to falls.
 *
 *  @returns FbleProfile*
 *   A new profile with a single root block and profiling enabled.
 *
 *  @sideeffects
 *   Allocates a new profile that should be freed with FbleFreeProfile() when
 *   no longer in use.
 */
FbleProfile* FbleNewProfile();

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
 *  @arg[FbleProfileThread*][thread] The profile thread to sample. May be NULL.
 *  @arg[uint64_t][count]
 *   The count of samples to charge against the current call sequence.
 *
 *  @sideeffects
 *   Charges the call sequence on the given thread with the given number of
 *   samples.
 */
void FbleProfileSample(FbleProfileThread* thread, size_t count);

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
 * @func[FbleProfileQuery] Callback function for profile queries.
 *  The FbleQueryProfile function will call this function once for each unique
 *  canonical trace in the profile.
 *
 *  @arg[FbleProfile*][profile] The profile being queried.
 *  @arg[void*][userdata] User data passed to FbleQueryProfile.
 *  @arg[FbleBlockIdV][seq]
 *   A canonical trace in the profile. This memory will not last beyond the
 *   call to the query function, make copies if needed.
 *  @arg[uint64_t][calls] The number of calls into this sequence.
 *  @arg[uint64_t][samples] The number of samples charged to this sequence.
 *  @sideeffects Updates fields of userdata as desired to query the profile.
 */
typedef void FbleProfileQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t calls, uint64_t samples);

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
 * @func[FbleOutputProfile] Outputs the profile to a file.
 *  Has no effect if profiling is disabled.
 *
 *  The profile is output in uncompressed binary encoded google/pprof proto
 *  format. See https://github.com/google/pprof/blob/main/proto/profile.proto
 *  for the proto format. See https://protobuf.dev/programming-guides/encoding/
 *  for the wire format.
 *
 *  To view in google/pprof, you'll need to gzip the output file first.
 *
 *  @arg[const char*][path] The path to the file to output the profile to.
 *  @arg[FbleProfile*][profile] The profile to output
 *  @sideeffects Outputs the profile to the given file.
 */
void FbleOutputProfile(const char* path, FbleProfile* profile);

#endif // FBLE_PROFILE_H_
