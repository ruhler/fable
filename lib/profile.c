/**
 * @file profile.c
 *  Fble profiling and reporting.
 */

/**
 * Notes on profiling
 * ------------------
 * Consider a profile call graph entry such as:
 *       count     time block
 *           2       70 /b[0002X]   
 *           1       90 /a[0001X]   
 * **        3       90 /b[0002X] **
 *           2       70 /b[0002X]   
 *           1       30 /c[0003X]   
 *
 * Focusing on the highlighted line with **, this says we spent 90 profile
 * time in block 'b'. The blocks that 'b' called are listed below it. So in
 * this case we spent 70 profile time calling from 'b' into itself
 * recursively, and 30 profile time calling from 'b' into 'c'. The blocks
 * that called 'b' are listed above it. So in this case we spent 70 profile
 * time calling into 'b' from 'b' and 90 profile time calling into 'b' from
 * 'a'.
 *
 * Note that the profile time calls for callers and callees don't add up
 * to the total time spent in 'b' because this example involves recursive
 * calls. The way to read it is as follows.
 *
 * 1. For the highlighted block with **
 *   The time shown is how much time would be saved running the program if all
 *   calls to the block were removed. Or equivalently, if you could perfectly
 *   optimize the block so it ran in no time at all.
 *
 *   Given a call stack: a -> b1 -> b2 -> b3 -> c, this counts the time spent
 *   doing the initial call a -> b1, and not the calls b1 -> b2 or b2 -> b3
 *   past that. Because neither of those calls would exist if we got rid of
 *   the call a -> b1.
 *
 * 2. For callees below the higlighted block.
 *   The time shown is how much time would be saved running the program if all
 *   calls from the highlighted block to the callee block were removed.
 *
 *   Given a call stack: a -> b1 -> b2 -> b3 -> c, this counts the time spent
 *   doing the initial call b1 -> b2, but not the call from b2 -> b3 past
 *   that, because that call would not exist if we got rid of the call from
 *   b1 -> b2.
 *
 * 3. For callers above the highlighted block.
 *   The time shown is how much time would be saved running the program if all
 *   calls from the caller block to the highlighted block were removed.
 *   Exactly analogous to callees.
 *
 * There are two interesting considerations for the implementation: how to
 * properly account for time in the case of recursive calls and how to
 * properly track time in case of tail calls.
 *
 * To properly account for time in the case of recursive calls, we keep track
 * of which blocks and calls are currently running. For example, if b1 -> b2
 * is currently running, then we will not count the time spent calling 
 * b2 -> b3 for the block time of b or the call time of b -> b.
 *
 * To properly track time in case of tail calls, we record the set of calls
 * that should exit when we exit the next call. Because of the above rule, we
 * only need to keep track of one occurence of each of the calls in the set;
 * subsequent occurences in a deeply nested stack would not have their time
 * counted anyway.
 */

#include <assert.h>   // for assert
#include <inttypes.h> // for PRIu64
#include <math.h>     // for sqrt
#include <stdbool.h>  // for bool
#include <string.h>   // for memset
#include <stdlib.h>   // for rand

#include <fble/fble-alloc.h>
#include <fble/fble-name.h>
#include <fble/fble-profile.h>
#include <fble/fble-vector.h>

// The average period of time between random samples.
#define RANDOM_SAMPLE_PERIOD 1024

const FbleBlockId RootBlockId = 0;

/**
 * @struct[Call] Representation of a call in the current call stack.
 *  @field[FbleBlockId][id] The id of the current block.
 *  @field[size_t][exit]
 *   The number of elements to pop from the sample stack when exiting this
 *   call.
 */
typedef struct {
  FbleBlockId id;   
  size_t exit;
} Call;

/**
 * @struct[CallStack] A stack of calls.
 *  The call stack is organized into a linked list of chunks.
 *
 *  @field[CallStack*][tail] The next chunk down in the stack.
 *  @field[CallStack*][next]
 *   The next (unused) chunk up in the stack. May be NULL.
 *  @field[Call*][top] Pointer to the valid top entry on the stack.
 *  @field[Call*][end] Pointer past the last allocated data in this chunk.
 *  @field[Call*][data] The raw data for this chunk.
 */
typedef struct CallStack {
  struct CallStack* tail;
  struct CallStack* next;
  Call* top; 
  Call* end;
  Call data[];
} CallStack;

/**
 * @struct[Sample] Representation of a call in a sample.
 *  @field[FbleBlockId][caller] The caller for this particular call.
 *  @field[FbleCallData*][call]
 *   Cached result of GetCallData(caller, callee). call->id gives callee id
 *   for the sample.
 */
typedef struct {
  FbleBlockId caller;
  FbleCallData* call;   
} Sample;

/**
 * @struct[SampleStack] A stack of Samples.
 *  This is different from the call stack in that calls to the same
 *  caller/callee appear at most once in the sample stack, and it includes
 *  information about auto-exited calls.
 *
 *  @field[size_t][capacity]
 *   Number of samples stack space has been allocated for.
 *  @field[size_t][size]
 *   Number of samples actually on the stack.
 *  @field[Sample*][xs] Samples on the stack.
 */
typedef struct {
  size_t capacity;
  size_t size;
  Sample* xs;
} SampleStack;

/**
 * @struct[CallDataEV] An ever expanding vector of call data.
 *  Capacity may be 0 and xs NULL to indicate an empty vector.
 *
 *  @field[size_t][capacity] Number of elements of xs allocated.
 *  @field[size_t][size] Number of valid elements on the vector.
 *  @field[FbleCallData**][xs] The elements of the vector.
 */
typedef struct {
  size_t capacity;
  size_t size;
  FbleCallData** xs;
} CallDataEV;

/**
 * @struct[FbleProfileThread] Profiling state for a thread of execution.
 *  @field[FbleProfile*][profile] The profile data.
 *  @field[CallStack*][calls] The current call stack.
 *  @field[SampleStack][sample] The current sample stack.
 *  @field[CallDataEV*][sample_set]
 *   The set of samples on the sample stack, for fast lookup of FbleCallData
 *   for a (caller,callee) pair.
 *  
 *   sample_set[caller] points to list of FbleCallData where call->id is the
 *   id of the callee. FbleCallData are sorted such that most recently added
 *   to the sample_set is at the end.
 *   
 *   sample_set[caller] is empty if the pair (caller,callee) does not appear
 *   on the sample stack.
 *  @field[size_t][sample_set_size] Length of sample_set.
 *  @field[int32_t][ttrs] Time to next random sample.
 */
struct FbleProfileThread {
  FbleProfile* profile;
  CallStack* calls;
  SampleStack sample;
  CallDataEV* sample_set;
  size_t sample_set_size;
  int32_t ttrs;
};

/**
 * A sort order.
 */
typedef enum {
  ASCENDING,
  DESCENDING
} Order;

static Call* CallStackPush(FbleProfileThread* thread);
static void CallStackPop(FbleProfileThread* thread);

static FbleCallData* GetCallData(FbleProfile* profile, FbleBlockId caller, FbleBlockId callee);
static void MergeSortCallData(Order order, bool in_place, FbleCallData** a, FbleCallData** b, size_t size);
static void SortCallData(Order order, FbleCallData** data, size_t size);
static void PrintBlockName(FILE* fout, FbleProfile* profile, FbleBlockId id);
static void PrintCallData(FILE* fout, FbleProfile* profile, bool block, FbleCallData* call);

static void EnterBlock(FbleProfileThread* thread, FbleBlockId block, bool replace);

/**
 * @func[CallStackPush]
 * @ Pushes an uninitialized Call onto the call stack for the thread.
 *  @arg[FbleProfileThread*][thread]
 *   The thread whose call stack to push a value on.
 *
 *  @returns[Call*]
 *   A pointer to the memory for the newly pushed value.
 *
 *  @sideeffects
 *   Pushes a value on the stack that should be freed using CallStackPop when
 *   no longer needed.
 */
static Call* CallStackPush(FbleProfileThread* thread)
{
  if (++thread->calls->top == thread->calls->end) {
    if (thread->calls->next == NULL) {
      size_t chunk_size = 2 * (thread->calls->end - thread->calls->data);
      CallStack* next = FbleAllocExtra(CallStack, chunk_size * sizeof(Call));
      next->tail = thread->calls;
      next->next = NULL;
      next->top = next->data;
      next->end = next->data + chunk_size;
      thread->calls->next = next;
    }
    thread->calls = thread->calls->next;
  }
  return thread->calls->top;
}

/**
 * @func[CallStackPop] Pops a Call off of the call stack for the thread.
 *  @arg[FbleProfileThread*][thread] The thread whose call stack to pop.
 *
 *  @sideeffects
 *   Pops a value on the stack that. Potentially invalidates any pointers
 *   previously returned from CallStackPush.
 */
static void CallStackPop(FbleProfileThread* thread)
{
  if (thread->calls->top == thread->calls->data) {
    if (thread->calls->next != NULL) {
      FbleFree(thread->calls->next);
      thread->calls->next = NULL;
    }
    thread->calls = thread->calls->tail;
  }
  thread->calls->top--;
}

/**
 * @func[GetCallData] Look up call data for caller/callee pair.
 *  Gets the call data associated with the given caller/callee pair in the
 *  call profile. Creates new empty call data and adds it to the profile as
 *  required.
 *
 *  @arg[FbleProfile*][profile] The profile to get the data for
 *  @arg[FbleBlockId][caller] The caller of the call
 *  @arg[FbleBlockId][callee] The callee of the call
 *
 *  @returns[FbleCallData*]
 *   The call data associated with the caller/callee pair in the given
 *   profile.
 *
 *  @sideeffects
 *   Allocates new empty call data and adds it to the profile if necessary.
 */
static FbleCallData* GetCallData(FbleProfile* profile,
    FbleBlockId caller, FbleBlockId callee)
{
  FbleCallDataV* callees = &profile->blocks.xs[caller]->callees;
  FbleCallData** xs = callees->xs;

  // Binary search for the call data.
  size_t lo = 0;
  size_t hi = callees->size;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    FbleBlockId here = xs[mid]->id;
    if (here < callee) {
      lo = mid + 1;
    } else if (here == callee) {
      return xs[mid];
    } else {
      hi = mid;
    }
  }

  // Nothing was found. Allocate new call data for this callee.
  FbleCallData* call = FbleAlloc(FbleCallData);
  call->id = callee;
  call->time = 0;
  call->count = 0;

  // Insert the new call data into the callee list, preserving the sort by
  // callee id.
  FbleCallData* data = call;
  for (size_t i = lo; i < callees->size; ++i) {
    FbleCallData* tmp = xs[i];
    xs[i] = data;
    data = tmp;
  }
  FbleAppendToVector(*callees, data);

  return call;
}

/**
 * @func[MergeSortCallData] Does a merge sort of call data.
 *  There are two modes for sorting:
 *  1. in_place - a is sorted in place, using b as a scratch buffer
 *  2. !in_place - a is sorted into b, then a can be used as a scratch buffer
 *
 *  @arg[Order][order] The order to sort in.
 *  @arg[bool][in_place] Whether to use in place sort or not.
 *  @arg[FbleCallData**][a] The data to sort.
 *  @arg[FbleCallData**][b]
 *   If in_place, a scratch buffer. Otherwise the destination for the sorted
 *   values.
 *  @arg[size_t][size] The number of elements to sort.
 *
 *  @sideeffects
 *   Sorts the contents of data a, either in place or into b. Overwrites the
 *   contents of the non-sorted array with scratch data.
 */
static void MergeSortCallData(Order order, bool in_place, FbleCallData** a, FbleCallData** b, size_t size)
{
  if (size == 0) {
    return;
  }

  if (size == 1) {
    if (!in_place) {
      *b = *a;
    }
    return;
  }

  FbleCallData** a1 = a;
  FbleCallData** b1 = b;
  size_t size1 = size / 2;
  MergeSortCallData(order, !in_place, a1, b1, size1);

  FbleCallData** a2 = a + size1;
  FbleCallData** b2 = b + size1;
  size_t size2 = size - size1;
  MergeSortCallData(order, !in_place, a2, b2, size2);

  FbleCallData** s1 = in_place ? b1 : a1;
  FbleCallData** s2 = in_place ? b2 : a2;
  FbleCallData** d = in_place ? a : b;

  FbleCallData** e1 = s1 + size1;
  FbleCallData** e2 = s2 + size2;
  while (s1 < e1 && s2 < e2) {
    if (order == ASCENDING) {
      if ((*s1)->time <= (*s2)->time) {
        *d++ = *s1++;
      } else {
        *d++ = *s2++;
      }
    } else {
      if ((*s1)->time >= (*s2)->time) {
        *d++ = *s1++;
      } else {
        *d++ = *s2++;
      }
    }
  }

  while (s1 < e1) {
    *d++ = *s1++;
  }

  while (s2 < e2) {
    *d++ = *s2++;
  }
}

/**
 * @func[SortCallData] Sorts a vector of call data by time.
 *  @arg[Order][order] The order to sort in.
 *  @arg[FbleCallData**][data] The call data to sort
 *  @arg[size_t][size] The number of elements of data.
 *
 *  @sideeffects
 *   Sorts the given array of call data in increasing order of time.
 */
static void SortCallData(Order order, FbleCallData** data, size_t size)
{
  FbleCallData* scratch[size];
  MergeSortCallData(order, true, data, scratch, size);
}

/**
 * @func[PrintBlockName] Prints a block name in human readable format.
 *  @arg[FILE*][fout] Where to print the name
 *  @arg[FbleProfile*][profile] The profile, used for getting block names
 *  @arg[FbleBlockId][id] The id of the block
 *
 *  @sideeffects
 *   Prints a human readable description of the block to the given file.
 */
static void PrintBlockName(FILE* fout, FbleProfile* profile, FbleBlockId id)
{
  fprintf(fout, "%s[%04zxX]", profile->blocks.xs[id]->name.name->str, id);
}

/**
 * @func[PrintCallData] Prints a line of call data.
 *  @arg[FILE*][fout] The file to print to
 *  @arg[FbleProfile*][profile] The profile, used for getting block names
 *  @arg[bool][block]
 *   If true, print the call data for a block. This adds highlights to the
 *   line and includes information about self time.
 *  @arg[FbleCallData*][call] The call data to print
 *
 *  @sideeffects
 *   Prints a single line description of the call data to the given file.
 */
static void PrintCallData(FILE* fout, FbleProfile* profile, bool block, FbleCallData* call)
{
  char h = block ? '*' : ' ';
  fprintf(fout, "%c%c %8" PRIu64 " %8" PRIu64, h, h, call->count, call->time);

  if (block) {
    fprintf(fout, " %8" PRIu64 "  ", profile->blocks.xs[call->id]->self);
  } else {
    fprintf(fout, " %8s  ", "");
  }

  PrintBlockName(fout, profile, call->id);
  fprintf(fout, " %c%c\n", h, h);
}

/**
 * @func[EnterBlock] Enters a block on the given profile thread.
 *  @arg[FbleProfileThread*][thread] The thread to do the call on.
 *  @arg[FbleBlockId][block] The block to call into.
 *  @arg[bool][replace]
 *   If true, replace the current block with the new block being entered
 *   instead of calling into the new block.
 *
 *  @sideeffects
 *   A corresponding call to FbleProfileExitBlock or FbleProfileReplaceBlock
 *   should be made when the call leaves, for proper accounting and resource
 *   management.
 */
static void EnterBlock(FbleProfileThread* thread, FbleBlockId block, bool replace)
{
  Call* call = thread->calls->top;
  FbleBlockId caller = call->id;
  FbleBlockId callee = block;
  thread->profile->blocks.xs[callee]->block.count++;

  // Resize sample_set if needed.
  // This could happen if new blocks were added to the profile during
  // evaluation.
  if (caller >= thread->sample_set_size) {
    size_t old_size = thread->sample_set_size;
    size_t new_size = thread->profile->blocks.size;

    thread->sample_set = FbleReAllocArray(CallDataEV, thread->sample_set, new_size);
    memset(thread->sample_set + old_size, 0, sizeof(CallDataEV) * (new_size - old_size));
    thread->sample_set_size = new_size;
  }

  // Check to see if this (caller,callee) pair is already on the stack.
  FbleCallData* data = NULL;
  CallDataEV* callees = thread->sample_set + caller;
  for (size_t i = 0; i < callees->size; ++i) {
    FbleCallData* candidate = callees->xs[i];
    if (candidate->id == callee) {
      data = candidate;
      break;
    }
  }

  bool call_running = true;
  if (data == NULL) {
    call_running = false;
    data = GetCallData(thread->profile, caller, callee);
  }

  data->count++;

  if (!replace) {
    call = CallStackPush(thread);
    call->exit = 0;
  }
  call->id = callee;

  if (!call_running) {
    if (thread->sample.size == thread->sample.capacity) {
      thread->sample.capacity *= 2;
      thread->sample.xs = FbleReAllocArray(Sample, thread->sample.xs, thread->sample.capacity);
    }
    Sample* sample = thread->sample.xs + thread->sample.size++;
    sample->caller = caller;
    sample->call = data;
    call->exit++;

    if (callees->capacity == 0) {
      callees->capacity = 1;
      callees->size = 0;
      callees->xs = FbleAllocArray(FbleCallData*, 1);
    } else if (callees->size == callees->capacity) {
      callees->capacity *= 2;
      callees->xs = FbleReAllocArray(FbleCallData*, callees->xs, callees->capacity);
    }
    callees->xs[callees->size++] = data;
  }
}

// See documentation in fble-profile.h.
FbleProfile* FbleNewProfile(bool enabled)
{
  FbleProfile* profile = FbleAlloc(FbleProfile);
  FbleInitVector(profile->blocks);
  profile->enabled = enabled;

  FbleName root = {
    .name = FbleNewString("<root>"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = { .source = FbleNewString(""), .line = 0, .col = 0 }
  };
  FbleBlockId root_id = FbleAddBlockToProfile(profile, root);
  assert(root_id == RootBlockId);

  return profile;
}

// See documentation in fble-profile.h.
FbleBlockId FbleAddBlockToProfile(FbleProfile* profile, FbleName name)
{
  FbleBlockId id = profile->blocks.size;
  FbleBlockProfile* block = FbleAlloc(FbleBlockProfile);
  block->name = name;
  block->self = 0;
  block->block.id = id;
  block->block.count = 0;
  block->block.time = 0;
  FbleInitVector(block->callees);
  FbleAppendToVector(profile->blocks, block);
  return id;
}
// See documentation in fble-profile.h.
FbleBlockId FbleAddBlocksToProfile(FbleProfile* profile, FbleNameV names)
{
  size_t id = profile->blocks.size;
  for (size_t i = 0; i < names.size; ++i) {
    FbleAddBlockToProfile(profile, FbleCopyName(names.xs[i]));
  }
  return id;
}

// See documentation in fble-profile.h.
void FbleFreeProfile(FbleProfile* profile)
{
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];
    FbleFreeName(block->name);
    for (size_t j = 0; j < block->callees.size; ++j) {
      FbleFree(block->callees.xs[j]);
    }
    FbleFree(block->callees.xs);
    FbleFree(block);
  }
  FbleFreeVector(profile->blocks);
  FbleFree(profile);
}

// See documentation in fble-profile.h.
FbleProfileThread* FbleNewProfileThread(FbleProfile* profile)
{
  if (!profile->enabled) {
    return NULL;
  }

  FbleProfileThread* thread = FbleAlloc(FbleProfileThread);
  thread->profile = profile;

  thread->calls = FbleAllocExtra(CallStack, 8 * sizeof(CallStack));
  thread->calls->tail = NULL;
  thread->calls->next = NULL;
  thread->calls->top = thread->calls->data;
  thread->calls->end = thread->calls->data + 8;
  thread->calls->top->id = RootBlockId;
  thread->calls->top->exit = 0;

  thread->sample.capacity = 8;
  thread->sample.size = 0;
  thread->sample.xs = FbleAllocArray(Sample, thread->sample.capacity);

  thread->sample_set = FbleAllocArray(CallDataEV, profile->blocks.size);
  memset(thread->sample_set, 0, sizeof(CallDataEV) * profile->blocks.size);
  thread->sample_set_size = profile->blocks.size;

  thread->ttrs = rand() % (2 * RANDOM_SAMPLE_PERIOD);

  thread->profile->blocks.xs[RootBlockId]->block.count++;
  return thread;
}

// See documentation in fble-profile.h.
void FbleFreeProfileThread(FbleProfileThread* thread)
{
  if (thread == NULL) {
    return;
  }

  CallStack* calls = thread->calls;
  if (calls->next != NULL) {
    calls = calls->next;
  }
  while (calls != NULL) {
    CallStack* tail = calls->tail;
    FbleFree(calls);
    calls = tail;
  }

  FbleFree(thread->sample.xs);

  for (size_t i = 0; i < thread->sample_set_size; ++i) {
    FbleFree(thread->sample_set[i].xs);
  }
  FbleFree(thread->sample_set);

  FbleFree(thread);
}

// See documentation in fble-profile.h.
void FbleProfileSample(FbleProfileThread* thread, uint64_t time)
{
  if (thread == NULL) {
    return;
  }

  thread->profile->blocks.xs[thread->calls->top->id]->self += time;

  // Charge calls in the stack for their time.
  bool block_seen[thread->profile->blocks.size];
  memset(block_seen, 0, thread->profile->blocks.size * sizeof(bool));
  for (size_t i = 0; i < thread->sample.size; ++i) {
    Sample* sample = thread->sample.xs + i;
    FbleCallData* data = sample->call;
    FbleBlockId callee = data->id;
    if (!block_seen[callee]) {
      block_seen[callee] = true;
      thread->profile->blocks.xs[callee]->block.time += time;
    }
    data->time += time;
  }

  thread->profile->blocks.xs[RootBlockId]->block.time += time;
}

// See documentation in fble-profile.h.
void FbleProfileRandomSample(FbleProfileThread* profile, size_t count)
{
  if (profile == NULL) {
    return;
  }

  profile->ttrs -= count;
  while (profile->ttrs <= 0) {
    FbleProfileSample(profile, 1);
    profile->ttrs += rand() % (2 * RANDOM_SAMPLE_PERIOD);
  }
}

// See documentation in fble-profile.h.
void FbleProfileEnterBlock(FbleProfileThread* thread, FbleBlockId block)
{
  if (thread == NULL) {
    return;
  }
  EnterBlock(thread, block, false);
}

// See documentation in fble-profile.h.
void FbleProfileReplaceBlock(FbleProfileThread* thread, FbleBlockId block)
{
  if (thread == NULL) {
    return;
  }
  EnterBlock(thread, block, true);
}

// See documentation in fble-profile.h.
void FbleProfileExitBlock(FbleProfileThread* thread)
{
  if (thread == NULL) {
    return;
  }

  // TODO: Consider shrinking the sample stack occasionally to recover memory?
  while (thread->calls->top->exit > 0) {
    thread->calls->top->exit--;
    thread->sample.size--;
    Sample* sample = thread->sample.xs + thread->sample.size;
    thread->sample_set[sample->caller].size--;
  }

  CallStackPop(thread);
}

// See documentation in fble-profile.h.
void FbleGenerateProfileReport(FILE* fout, FbleProfile* profile)
{
  if (!profile->enabled) {
    return;
  }

  FbleCallData* calls[profile->blocks.size];
  FbleCallData* selfs[profile->blocks.size];

  // Number of blocks covered.
  size_t covered = 0;
  uint64_t total = 0;

  FbleCallDataV callers[profile->blocks.size];
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleInitVector(callers[i]);
  }

  for (size_t i = 0; i < profile->blocks.size; ++i) {
    calls[i] = &profile->blocks.xs[i]->block;
    total += profile->blocks.xs[i]->self;

    if (calls[i]->count > 0) {
      covered++;
    }

    for (size_t j = 0; j < profile->blocks.xs[i]->callees.size; ++j) {
      FbleCallData* call = profile->blocks.xs[i]->callees.xs[j];
      FbleCallData* called = FbleAlloc(FbleCallData);
      called->id = i;
      called->count = call->count;
      called->time = call->time;
      FbleAppendToVector(callers[call->id], called);
    }

    // As a convenience to be able to reuse the sorting logic for sorting
    // blocks by self time, fake up an FbleCallData for a block that stores
    // block self time in the call time field.
    selfs[i] = FbleAlloc(FbleCallData);
    selfs[i]->id = profile->blocks.xs[i]->block.id;
    selfs[i]->time = profile->blocks.xs[i]->self;
    selfs[i]->count = 0;
  }
  SortCallData(DESCENDING, calls, profile->blocks.size);
  SortCallData(DESCENDING, selfs, profile->blocks.size);

  double coverage = (double)covered / (double)profile->blocks.size;

  fprintf(fout, "Profile Report\n");
  fprintf(fout, "==============\n");
  fprintf(fout, "blocks executed: %2.2f%% of %zi\n", 100 * coverage, profile->blocks.size);
  fprintf(fout, "\n");

  // Flat Profile by Overall Time
  fprintf(fout, "Flat Profile by Overall Time\n");
  fprintf(fout, "----------------------------\n");
  fprintf(fout, "   %8s %8s %8s  %s\n", "count", "time", "self", "block");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    PrintCallData(fout, profile, true, calls[i]);
  }
  fprintf(fout, "\n");

  // Flat Profile by Self Time
  fprintf(fout, "Flat Profile by Self Time\n");
  fprintf(fout, "-------------------------\n");
  fprintf(fout, "%5s %8s  %s\n", "%", "self", "block");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    double percent = 100.0 * (double)selfs[i]->time / (double)total;
    fprintf(fout, "%5.2f %8" PRIu64 "  ", percent, selfs[i]->time);
    PrintBlockName(fout, profile, selfs[i]->id);
    fprintf(fout, "\n");
    FbleFree(selfs[i]);
  }
  fprintf(fout, "\n");

  // Call Graph
  fprintf(fout, "Call Graph\n");
  fprintf(fout, "----------\n");
  fprintf(fout, "   %8s %8s  %s\n", "count", "time", "block");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    size_t id = calls[i]->id;
    FbleBlockProfile* block = profile->blocks.xs[id];
    if (block->block.count > 0) {
      // Callers
      SortCallData(ASCENDING, callers[id].xs, callers[id].size);
      for (size_t j = 0; j < callers[id].size; ++j) {
        PrintCallData(fout, profile, false, callers[id].xs[j]);
      }

      // Block
      PrintCallData(fout, profile, true, calls[i]);

      // Callees
      FbleCallData* callees[block->callees.size];
      for (size_t j = 0; j < block->callees.size; ++j) {
        callees[j] = block->callees.xs[j];
      }
      SortCallData(DESCENDING, callees, block->callees.size);
      for (size_t j = 0; j < block->callees.size; ++j) {
        PrintCallData(fout, profile, false, callees[j]);
      }
      fprintf(fout, "-------------------------------\n");
    }

    for (size_t j = 0; j < callers[id].size; ++j) {
      FbleFree(callers[id].xs[j]);
    }
    FbleFreeVector(callers[id]);
  }
  fprintf(fout, "\n");

  // Locations
  fprintf(fout, "Block Locations\n");
  fprintf(fout, "---------------\n");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleName* name = &profile->blocks.xs[i]->name;
    PrintBlockName(fout, profile, profile->blocks.xs[i]->block.id);
    fprintf(fout, ": %s:%zi:%zi\n", name->loc.source->str, name->loc.line, name->loc.col);
  }
  fprintf(fout, "\n");
}
