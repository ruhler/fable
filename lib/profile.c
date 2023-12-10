/**
 * @file profile.c
 * Fble profiling and reporting.
 *
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
#include <string.h>   // for memset, memcpy
#include <stdlib.h>   // for rand
#include <sys/time.h> // for gettimeofday

#include <fble/fble-alloc.h>
#include <fble/fble-name.h>
#include <fble/fble-profile.h>
#include <fble/fble-vector.h>

/** Representation of a call in the current call stack. */
typedef struct {
  /** The id of the current block. */
  FbleBlockId id;   

  /** The number of elements to pop from the sample stack when exiting this call. */
  size_t exit;
} Call;

/**
 * A stack of calls.
 *
 * The call stack is organized into a linked list of chunks.
 */
typedef struct CallStack {
  struct CallStack* tail; /**< The next chunk down in the stack. */
  struct CallStack* next; /**< The next (unused) chunk up in the stack. May be NULL. */
  Call* top;              /**< Pointer to the valid top entry on the stack. */
  Call* end;              /**< Pointer past the last allocated data in this chunk. */
  Call data[];            /**< The raw data for this chunk. */
} CallStack;

/** Representation of a call in a sample. */
typedef struct {
  FbleBlockId caller;   /**< The caller for this particular call. */
  FbleBlockId callee;   /**< The callee for this particular call. */
  FbleCallData* call;   /**< Cached result of GetCallData(caller, callee). */
} Sample;

/**
 * A stack of Samples.
 *
 * This is different from the call stack in that calls to the same
 * caller/callee appear at most once in the sample stack, and it includes
 * information about auto-exited calls.
 */
typedef struct {
  size_t capacity;    /**< Number of samples stack space has been allocated for. */
  size_t size;        /**< Number of samples actually on the stack. */
  Sample* xs;         /**< Samples on the stack. */
} SampleStack;

/** Profiling state for a thread of execution. */
struct FbleProfileThread {
  FbleProfile* profile;   /**< The profile data. */

  CallStack* calls;       /**< The current call stack. */
  SampleStack sample;     /**< The current sample stack. */
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
 * Pushes an uninitialized Call onto the call stack for the thread.
 *
 * @param thread  The thread whose call stack to push a value on.
 *
 * @returns
 *   A pointer to the memory for the newly pushed value.
 *
 * @sideeffects
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
 * Pops a Call off of the call stack for the thread.
 *
 * @param thread  The thread whose call stack to pop.
 *
 * @sideeffects
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
 * Gets the call data associated with the given caller/callee pair in the call
 * profile. Creates new empty call data and adds it to the profile as
 * required.
 *
 * @param profile  The profile to get the data for
 * @param caller  The caller of the call
 * @param callee  The callee of the call
 *
 * @returns
 *   The call data associated with the caller/callee pair in the given
 *   profile.
 *
 * @sideeffects
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
  FbleVectorAppend(*callees, data);

  return call;
}

/**
 * Does a merge sort of call data.
 *
 * There are two modes for sorting:
 * 1. in_place - a is sorted in place, using b as a scratch buffer
 * 2. !in_place - a is sorted into b, then a can be used as a scratch buffer
 *
 * @param order  The order to sort in.
 * @param in_place  Whether to use in place sort or not.
 * @param a  The data to sort
 * @param b  If in_place, a scratch buffer. Otherwise the destination for the
 *    sorted values
 * @param size  The number of elements to sort.
 *
 * @sideeffects
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
 * Sorts a vector of call data by time.
 *
 * @param order  The order to sort in.
 * @param data  The call data to sort
 * @param size  The number of elements of data.
 *
 * @sideeffects
 *   Sorts the given array of call data in increasing order of time.
 */
static void SortCallData(Order order, FbleCallData** data, size_t size)
{
  FbleCallData* scratch[size];
  MergeSortCallData(order, true, data, scratch, size);
}

/**
 * Prints a block name in human readable format.
 * 
 * @param fout  Where to print the name
 * @param profile  The profile, used for getting block names
 * @param id  The id of the block
 *
 * @sideeffects
 *   Prints a human readable description of the block to the given file.
 */
static void PrintBlockName(FILE* fout, FbleProfile* profile, FbleBlockId id)
{
  fprintf(fout, "%s[%04zxX]", profile->blocks.xs[id]->name.name->str, id);
}

/**
 * Prints a line of call data.
 *
 * @param fout  The file to print to
 * @param profile  The profile, used for getting block names
 * @param block  If true, print the call data for a block. This adds highlights to the
 *   line and includes information about self time.
 * @param call  The call data to print
 *
 * @sideeffects
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
 * Enters a block on the given profile thread.
 *
 * @param thread  The thread to do the call on.
 * @param block  The block to call into.
 * @param replace  If true, replace the current block with the new block being
 *   entered instead of calling into the new block.
 *
 * @sideeffects
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

  // Check to see if this (caller,callee) pair is already on the stack.
  FbleCallData* data = NULL;
  for (size_t i = 0; i < thread->sample.size; ++i) {
    Sample* sample = thread->sample.xs + thread->sample.size - i - 1;
    if (sample->caller == caller && sample->callee == callee) {
      data = sample->call;
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
      Sample* xs = FbleArrayAlloc(Sample, thread->sample.capacity);
      memcpy(xs, thread->sample.xs, thread->sample.size * sizeof(Sample));
      FbleFree(thread->sample.xs);
      thread->sample.xs = xs;
    }
    Sample* sample = thread->sample.xs + thread->sample.size++;
    sample->caller = caller;
    sample->callee = callee;
    sample->call = data;
    call->exit++;
  }
}

// See documentation in fble-profile.h.
FbleProfile* FbleNewProfile(bool enabled)
{
  FbleProfile* profile = FbleAlloc(FbleProfile);
  FbleVectorInit(profile->blocks);
  profile->enabled = enabled;

  FbleName root = {
    .name = FbleNewString("<root>"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = { .source = FbleNewString(""), .line = 0, .col = 0 }
  };
  FbleBlockId root_id = FbleProfileAddBlock(profile, root);
  assert(root_id == FBLE_ROOT_BLOCK_ID);

  return profile;
}

// See documentation in fble-profile.h.
FbleBlockId FbleProfileAddBlock(FbleProfile* profile, FbleName name)
{
  FbleBlockId id = profile->blocks.size;
  FbleBlockProfile* block = FbleAlloc(FbleBlockProfile);
  block->name = name;
  block->self = 0;
  block->block.id = id;
  block->block.count = 0;
  block->block.time = 0;
  FbleVectorInit(block->callees);
  FbleVectorAppend(profile->blocks, block);
  return id;
}
// See documentation in fble-profile.h.
FbleBlockId FbleProfileAddBlocks(FbleProfile* profile, FbleNameV names)
{
  size_t id = profile->blocks.size;
  for (size_t i = 0; i < names.size; ++i) {
    FbleProfileAddBlock(profile, FbleCopyName(names.xs[i]));
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
  thread->calls->top->id = FBLE_ROOT_BLOCK_ID;
  thread->calls->top->exit = 0;

  thread->sample.capacity = 8;
  thread->sample.size = 0;
  thread->sample.xs = FbleArrayAlloc(Sample, thread->sample.capacity);

  thread->profile->blocks.xs[FBLE_ROOT_BLOCK_ID]->block.count++;
  return thread;
}

// See documentation in fble-profile.h.
FbleProfileThread* FbleForkProfileThread(FbleProfileThread* parent)
{
  if (parent == NULL) {
    return NULL;
  }

  FbleProfileThread* thread = FbleAlloc(FbleProfileThread);
  thread->profile = parent->profile;

  // Copy the call stack.
  {
    CallStack* next = NULL;
    for (CallStack* p = parent->calls; p != NULL; p = p->tail) {
      size_t chunk_size = p->end - p->data;
      CallStack* c = FbleAllocExtra(CallStack, chunk_size * sizeof(Call));
      if (next == NULL) {
        thread->calls = c;
      } else {
        next->tail = c;
      }

      c->tail = NULL;
      c->next = next;
      c->top = c->data + (p->top - p->data);
      c->end = c->data + chunk_size;
      memcpy(c->data, p->data, chunk_size * sizeof(Call));
      next = c;
    }
  }

  // Copy the sample stack.
  thread->sample.capacity = parent->sample.capacity;
  thread->sample.size = parent->sample.size;
  thread->sample.xs = FbleArrayAlloc(Sample, thread->sample.capacity);
  memcpy(thread->sample.xs, parent->sample.xs, parent->sample.size * sizeof(Sample));
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
    if (!block_seen[sample->callee]) {
      block_seen[sample->callee] = true;
      thread->profile->blocks.xs[sample->callee]->block.time += time;
    }
    data->time += time;
  }

  thread->profile->blocks.xs[FBLE_ROOT_BLOCK_ID]->block.time += time;
}

// See documentation in fble-profile.h.
void FbleProfileRandomSample(FbleProfileThread* profile, size_t count)
{
  if (profile == NULL) {
    return;
  }

  size_t time = 0;
  for (size_t i = 0; i < count; ++i) {
    if (rand() % 1024 == 0) {
      time++;
    }
  }

  if (time > 0) {
    FbleProfileSample(profile, time);
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
  thread->sample.size -= thread->calls->top->exit;
  CallStackPop(thread);
}

// See documentation in fble-profile.h.
void FbleProfileReport(FILE* fout, FbleProfile* profile)
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
    FbleVectorInit(callers[i]);
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
      FbleVectorAppend(callers[call->id], called);
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
