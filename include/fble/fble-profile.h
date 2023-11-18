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
 * FbleBlockId of the root block.
 *
 * The root block is the initial block for new threads.
 */
#define FBLE_ROOT_BLOCK_ID 0

/** A vector of BlockId. */
typedef struct {
  size_t size;      /**< Number of elements. */
  FbleBlockId* xs;  /**< Elements. */
} FbleBlockIdV;

/**
 * Info about calls to a block.
 *
 * Represents the number of calls and time spent when calling into or from
 * another block.
 * Fields:
 *   id - The id of the caller/callee block.
 *   count - The number of times the call was made.
 *   time - The amount of time spent in the call.
 */
typedef struct {
  /** Id of the caller/callee block. */
  FbleBlockId id;

  /** Number of times the call was made. */
  uint64_t count;

  /** Amount of time spent in the call. */
  uint64_t time;
} FbleCallData;

/** A vector of FbleCallData. */
typedef struct {
  size_t size;        /**< Number of elements. */
  FbleCallData** xs;  /**< Elements. */
} FbleCallDataV;

/** Profile information for a particular block. */
typedef struct {
  /** Name of the block. */
  FbleName name;

  /** Time spent in the block. Not including callees of this block. */
  uint64_t self;

  /** Id, summary count, and time spent in this block. */
  FbleCallData block;

  /**
   * Info about calls from this block into other blocks. Sorted in increasing
   * order of callee. Only callees that have been called from this block are
   * included.
   */
  FbleCallDataV callees;
} FbleBlockProfile;

/** A vector of FbleBlockProfile. */
typedef struct {
  size_t size;            /**< Number of elements. */
  FbleBlockProfile** xs;  /**< Elements. */
} FbleBlockProfileV;

/** Profiling data collected for a program. */
typedef struct {
  /**
   * Profiling blocks. blocks.xs[i] contains block and callee information for
   * block i.
   */
  FbleBlockProfileV blocks;
} FbleProfile;

/**
 * @func[FbleNewProfile] Creates a new profile.
 *  @returns FbleProfile*
 *   A new profile with a single root block.
 *
 *  @sideeffects
 *   Allocates a new profile that should be freed with FbleFreeProfile() when
 *   no longer in use.
 */
FbleProfile* FbleNewProfile();

/**
 * @func[FbleProfileAddBlock] Adds a block to the profile.
 *  Note: It is acceptable to add blocks in the middle of a profiling run.
 *
 *  @arg[FbleProfile*] profile
 *   The profile to add the block to.
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
FbleBlockId FbleProfileAddBlock(FbleProfile* profile, FbleName name);

/**
 * @func[FbleProfileAddBlocks] Add blocks to the profile.
 *  Add multiple new block to the profile using a contiguous range of block
 *  ids.
 *
 *  Note: It is acceptable to add blocks in the middle of a profiling run.
 *
 *  @arg[FbleProfile*] profile
 *   The profile to add the blocks to
 *  @arg[FbleNameV] names
 *   The names of the block to add. Borrowed.
 *
 *  @returns FbleBlockId
 *   The id of the first added block.
 *
 *  @sideeffects
 *   Adds blocks to the profile.
 */
FbleBlockId FbleProfileAddBlocks(FbleProfile* profile, FbleNameV names);

/**
 * @func[FbleFreeProfile] Frees a profile.
 *  @arg[FbleProfile*][profile] The profile to free. May be NULL.
 *
 *  @sideeffects
 *   Frees the memory resources associated with the given profile, including
 *   the memory for the block names supplied to FbleProfileAddBlock.
 */
void FbleFreeProfile(FbleProfile* profile);

/** Profiling state for a running thread. */
typedef struct FbleProfileThread FbleProfileThread;

/**
 * @func[FbleNewProfileThread] Allocates a new profile thread.
 *  The new thread starts in the FBLE_ROOT_BLOCK_ID block. See
 *  FbleForkProfileThread() for an alternative way to create a new profile
 *  thread.
 *
 *  @arg[FbleProfile*][profile] The profile to save profiling data to.
 *
 *  @returns FbleProfileThread*
 *   A new profile thread.
 *
 *  @sideeffects
 *   Allocates a new profile thread that should be freed with
 *   FreeProfileThread when no longer in use.
 */
FbleProfileThread* FbleNewProfileThread(FbleProfile* profile);

/**
 * @func[FbleForkProfileThread] Allocates a new profile thread by forking.
 *  The new thread starts with a copy of the parent thread's call stack.
 *
 *  @arg[FbleProfileThread*][parent] The parent thread to fork from.
 *
 *  @returns FbleProfileThread*
 *   A new profile thread.
 *
 *  @sideeffects
 *   Allocates a new profile thread that should be freed with
 *   FreeProfileThread when no longer in use.
 */
FbleProfileThread* FbleForkProfileThread(FbleProfileThread* parent);

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
 *   The profile thread to sample.
 *  @arg[uint64_t] time
 *   The amount of profile time to advance.
 *
 *  @sideeffects
 *   Charges calls on the current thread with the given time.
 */
void FbleProfileSample(FbleProfileThread* thread, uint64_t time);

/**
 * @func[FbleProfileRandomSample] Takes a random profiling sample.
 *  Reduces overheads associated with profiling by randomly sampling.
 *
 *  @arg[FbleProfileThread*] thread
 *   The profile thread to sample.
 *  @arg[size_t] count
 *   The number of random samples to take.
 *
 *  @sideeffects
 *   Charges calls on the current thread with the given time.
 */
void FbleProfileRandomSample(FbleProfileThread* thread, size_t count);

/**
 * @func[FbleProfileEnterBlock] Enters a profiling block.
 *  When calling into a function, use this function to tell the profiling
 *  logic what block is being called into.
 *
 *  @arg[FbleProfileThread*] thread
 *   The thread to do the call on.
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
 *   The thread to do the call on.
 *  @arg[FbleBlockId] block
 *   The block to tail call into.
 *
 *  @sideeffects
 *   @i Replaces the current profiling block with the new block.
 *   @i
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
 *  @arg[FbleProfileThread*][thread] The thread to exit the call on.
 *
 *  @sideeffects
 *   Updates the profile data associated with the given thread.
 */
void FbleProfileExitBlock(FbleProfileThread* thread);

/**
 * @func[FbleProfileReport] Generates a profiling report.
 *  @arg[FILE*] fout
 *   The file to output the profile report to.
 *  @arg[FbleProfile*] profile
 *   The profile to generate a report for.
 *
 *  @sideeffects
 *   Writes a profile report to the given file.
 */
void FbleProfileReport(FILE* fout, FbleProfile* profile);

#endif // FBLE_PROFILE_H_
