// profile.c --
//   This file implements the fble profiling routines.

#include <assert.h>   // for assert
#include <inttypes.h> // for PRIu64
#include <math.h>     // for sqrt
#include <stdbool.h>  // for bool
#include <string.h>   // for memset
#include <sys/time.h> // for gettimeofday

#include "fble-alloc.h"
#include "fble-profile.h"
#include "fble-syntax.h"
#include "fble-vector.h"

// Notes on profiling
// ------------------
// Consider a profile call graph entry such as:
//       count     wall     time block
//           2        0       70 b[0002]   
//           1        0       90 a[0001]   
// **        3        0       90 b[0002] **
//           2        0       70 b[0002]   
//           1        0       30 c[0003]   
//
// Focusing on the highlighted line with **, this says we spent 90 profile
// time in block 'b'. The blocks that 'b' called are listed below it. So in
// this case we spent 70 profile time calling from 'b' into itself
// recursively, and 30 profile time calling from 'b' into 'c'. The blocks
// that called 'b' are listed above it. So in this case we spent 70 profile
// time calling into 'b' from 'b' and 90 profile time calling into 'b' from
// 'a'.
//
// Note that the profile time calls for callers and callees don't add up
// to the total time spent in 'b' because this example involves recursive
// calls. The way to read it is as follows.
//
// 1. For the highlighted block with **
//   The time shown is how much time would be saved running the program if all
//   calls to the block were removed. Or equivalently, if you could perfectly
//   optimize the block so it ran in no time at all.
//
//   Given a call stack: a -> b1 -> b2 -> b3 -> c, this counts the time spent
//   doing the initial call a -> b1, and not the calls b1 -> b2 or b2 -> b3
//   past that. Because neither of those calls would exist if we got rid of
//   the call a -> b1.
//
// 2. For callees below the higlighted block.
//   The time shown is how much time would be saved running the program if all
//   calls from the highlighted block to the callee block were removed.
//
//   Given a call stack: a -> b1 -> b2 -> b3 -> c, this counts the time spent
//   doing the initial call b1 -> b2, but not the call from b2 -> b3 past
//   that, because that call would not exist if we got rid of the call from
//   b1 -> b2.
//
// 3. For callers above the highlighted block.
//   The time shown is how much time would be saved running the program if all
//   calls from the caller block to the highlighted block were removed.
//   Exactly analogous to callees.
//
// There are two interesting considerations for the implementation: how to
// properly account for time in the case of recursive calls and how to
// properly track time in case of tail calls.
//
// To properly account for time in the case of recursive calls, we keep track
// of which blocks and calls are currently running. For example, if b1 -> b2
// is currently running, then we will not count the time spent calling 
// b2 -> b3 for the block time of b or the call time of b -> b.
//
// To properly track time in case of tail calls, we record the set of calls
// that should exit when we exit the next call. Because of the above rule, we
// only need to keep track of one occurence of each of the calls in the set;
// subsequent occurences in a deeply nested stack would not have their time
// counted anyway.

// Call -- 
//   Representation of a call in the current call stack.
//
// Fields:
//   id - the id of the current block.
//   exit - the number of elements to pop from the sample stack when exiting
//          this call.
typedef struct {
  FbleBlockId id;
  size_t exit;
} Call;

// CallV --
//   A vector of calls. Also known as the CallStack.
typedef struct {
  size_t size;
  Call* xs;
} CallV;

// Sample --
//   Representation of a call in a sample.
//
// Fields:
//   caller - the caller for this particular call
//   callee - the callee for this particular call
//   call - cached result of GetCallData(caller, callee)
//   new_block - false if this block was called recursively from itself.
typedef struct {
  FbleBlockId caller;
  FbleBlockId callee;
  FbleCallData* call;
  bool new_block;
} Sample;

// SampleV --
//   A vector of Sample. Also known as the sample stack. This is different
//   from the call stack in that calls to the same caller/callee appear at
//   most once in the sample stack, and it includes information about
//   auto-exited calls.
typedef struct {
  size_t size;
  Sample* xs;
} SampleV;

// FbleProfileThread -- see documentation in fble-profile.h
struct FbleProfileThread {
  CallV calls;
  SampleV sample;

  // Set of currently running calls.
  // * running[x] is non-null if block x is running.
  // * Bit caller % 64 of running[callee][caller / 64] is set if a call from
  //   caller to callee is running.
  uint64_t** running;

  bool auto_exit;

  FbleProfile* profile;
};

typedef enum {
  ASCENDING,
  DESCENDING
} Order;

static FbleCallData* GetCallData(FbleArena* arena, FbleProfile* profile, FbleBlockId caller, FbleBlockId callee);
static void MergeSortCallData(FbleProfileClock clock, Order order, bool in_place, FbleCallData** a, FbleCallData** b, size_t size);
static void SortCallData(FbleProfileClock clock, Order order, FbleCallData** data, size_t size);
static void PrintBlockName(FILE* fout, FbleNameV* blocks, FbleBlockId id);
static void PrintCallData(FILE* fout, FbleNameV* blocks, bool highlight, FbleCallData* call);

static uint64_t GetTimeMillis();

// GetCallData --
//   Get the call data associated with the given caller/callee pair in the
//   call profile. Creates new empty call data and adds it to the profile as
//   required.
//
// Inputs:
//   arena - arena to use for allocations
//   profile - the profile to get the data for
//   caller - the caller of the call
//   callee - the callee of the call
//
// Results:
//   The call data associated with the caller/callee pair in the given
//   profile.
//
// Side effects:
//   Allocates new empty call data and adds it to the profile if necessary.
static FbleCallData* GetCallData(FbleArena* arena, FbleProfile* profile,
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
  FbleCallData* call = FbleAlloc(arena, FbleCallData);
  call->id = callee;
  for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
    call->time[clock] = 0;
  }
  call->count = 0;

  // Insert the new call data into the callee list, preserving the sort by
  // callee id.
  FbleCallData* data = call;
  for (size_t i = lo; i < callees->size; ++i) {
    FbleCallData* tmp = xs[i];
    xs[i] = data;
    data = tmp;
  }
  FbleVectorAppend(arena, *callees, data);

  return call;
}

// MergeSortCallData --
//   Does a merge sort of call data. There are two modes for sorting:
//   1. in_place - a is sorted in place, using b as a scratch buffer
//   2. !in_place - a is sorted into b, then a can be used as a scratch buffer
//
// Inputs:
//   clock - the clock to sort by.
//   order - the order to sort in.
//   in_place - whether to use in place sort or not.
//   a - the data to sort
//   b - if in_place, a scratch buffer. Otherwise the destination for the
//       sorted values
//   size - the number of elements to sort.
//
// Results:
//   none.
//
// Side effects:
//   Sorts the contents of data a, either in place or into b. Overwrites the
//   contents of the non-sorted array with scratch data.
static void MergeSortCallData(FbleProfileClock clock, Order order, bool in_place, FbleCallData** a, FbleCallData** b, size_t size)
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
  MergeSortCallData(clock, order, !in_place, a1, b1, size1);

  FbleCallData** a2 = a + size1;
  FbleCallData** b2 = b + size1;
  size_t size2 = size - size1;
  MergeSortCallData(clock, order, !in_place, a2, b2, size2);

  FbleCallData** s1 = in_place ? b1 : a1;
  FbleCallData** s2 = in_place ? b2 : a2;
  FbleCallData** d = in_place ? a : b;

  FbleCallData** e1 = s1 + size1;
  FbleCallData** e2 = s2 + size2;
  while (s1 < e1 && s2 < e2) {
    if (order == ASCENDING) {
      if ((*s1)->time[clock] <= (*s2)->time[clock]) {
        *d++ = *s1++;
      } else {
        *d++ = *s2++;
      }
    } else {
      if ((*s1)->time[clock] >= (*s2)->time[clock]) {
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

// SortCallData --
//   Sort a vector of call data by time.
//
// Inputs:
//   clock - the clock to sort by.
//   order - the order to sort in.
//   data - the call data to sort
//   size - the number of elements of data.
//
// Results:
//   none.
//
// Side effects:
//   Sorts the given array of call data in increasing order of time.
static void SortCallData(FbleProfileClock clock, Order order, FbleCallData** data, size_t size)
{
  FbleCallData* scratch[size];
  MergeSortCallData(clock, order, true, data, scratch, size);
}

// PrintBlockName --
//   Prints a block name in human readable format.
// 
// Inputs:
//   fout - where to print the name
//   blocks - the names of block indexed by id
//   id - the id of the block
//
// Results:
//   none.
//
// Side effects:
//   Prints a human readable description of the block to the given file.
static void PrintBlockName(FILE* fout, FbleNameV* blocks, FbleBlockId id)
{
  FbleName* name = blocks->xs + id;
  fprintf(fout, "%s[%04x]", name->name, id);
}

// PrintCallData --
//   Prints a line of call data.
//
// Inputs:
//   fout - the file to print to
//   blocks - the names of blocks, indexed by id
//   highlight - if true, highlight the line
//   call - the call data to print
//
// Results:
//   none.
//
// Side effects:
//   Prints a single line description of the call data to the given file.
static void PrintCallData(FILE* fout, FbleNameV* blocks, bool highlight, FbleCallData* call)
{
  uint64_t wall = call->time[FBLE_PROFILE_WALL_CLOCK];
  uint64_t time = call->time[FBLE_PROFILE_TIME_CLOCK];
  char h = highlight ? '*' : ' ';
  fprintf(fout, "%c%c %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " ",
      h, h, call->count, wall, time);
  PrintBlockName(fout, blocks, call->id);
  fprintf(fout, " %c%c\n", h, h);
}

// GetTimeMillis -
//   Get the current time in milliseconds.
//
// Inputs:
//   None.
//
// Results:
//   The current wall clock time in milliseconds, relative to some arbitrary,
//   fixed point in time.
//
// Side effects:
//   None.
static uint64_t GetTimeMillis()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t seconds = tv.tv_sec;
  uint64_t millis = tv.tv_usec / 1000;
  uint64_t time = 1000 * seconds + millis;
  return time;
}

// FbleNewProfile -- see documentation in fble-profile.h
FbleProfile* FbleNewProfile(FbleArena* arena, size_t blockc, size_t period)
{
  FbleProfile* profile = FbleAlloc(arena, FbleProfile);
  profile->ticks = 0;
  profile->wall = 0;
  profile->period = period;
  profile->blocks.size = blockc;
  profile->blocks.xs = FbleArrayAlloc(arena, FbleBlockProfile*, blockc);
  for (size_t i = 0; i < blockc; ++i) {
    profile->blocks.xs[i] = FbleAlloc(arena, FbleBlockProfile);
    profile->blocks.xs[i]->block.id = i;
    profile->blocks.xs[i]->block.count = 0;
    for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
      profile->blocks.xs[i]->block.time[clock] = 0;
    }
    FbleVectorInit(arena, profile->blocks.xs[i]->callees);
  }
  return profile;
}

// FbleFreeProfile -- see documentation in fble-profile.h
void FbleFreeProfile(FbleArena* arena, FbleProfile* profile)
{
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];

    for (size_t j = 0; j < block->callees.size; ++j) {
      FbleFree(arena, block->callees.xs[j]);
    }
    FbleFree(arena, block->callees.xs);

    FbleFree(arena, block);
  }
  FbleFree(arena, profile->blocks.xs);
  FbleFree(arena, profile);
}

// FbleNewProfileThread -- see documentation in fble-profile.h
FbleProfileThread* FbleNewProfileThread(FbleArena* arena, FbleProfileThread* parent, FbleProfile* profile)
{
  FbleProfileThread* thread = FbleAlloc(arena, FbleProfileThread);
  thread->auto_exit = false;
  thread->profile = profile;
  FbleVectorInit(arena, thread->calls);
  FbleVectorInit(arena, thread->sample);

  size_t n = profile->blocks.size;
  thread->running = FbleArrayAlloc(arena, uint64_t*, n);
  memset(thread->running, 0, n * sizeof(uint64_t*));

  if (parent == NULL) {
    Call call = { .id = FBLE_ROOT_BLOCK_ID, .exit = 0 };
    FbleVectorAppend(arena, thread->calls, call);

    thread->profile->blocks.xs[FBLE_ROOT_BLOCK_ID]->block.count++;
  } else {
    assert(parent->profile == profile);

    // TODO: We could make these copies more efficient if they are a
    // performance issue, by pre-allocated the array of the vector. Just make
    // sure to preallocate it to a power of two (or add a library function to
    // fble-alloc.h to take care of this for us).
    for (size_t i = 0; i < parent->calls.size; ++i) {
      FbleVectorAppend(arena, thread->calls, parent->calls.xs[i]);
    }
    for (size_t i = 0; i < parent->sample.size; ++i) {
      Sample s = parent->sample.xs[i];
      if (thread->running[s.callee] == NULL) {
        thread->running[s.callee] = FbleArrayAlloc(arena, uint64_t, (n/64) + 1);
        memset(thread->running[s.callee], 0, ((n/64)+1) * sizeof(uint64_t));
      }
      thread->running[s.callee][s.caller / 64] |= (1 << (s.caller % 64));

      FbleVectorAppend(arena, thread->sample, parent->sample.xs[i]);
    }
  }
  return thread;
}

// FbleFreeProfileThread -- see documentation in fble-profile.h
void FbleFreeProfileThread(FbleArena* arena, FbleProfileThread* thread)
{
  for (size_t i = 0; i < thread->sample.size; ++i) {
    Sample s = thread->sample.xs[i];
    if (s.new_block) {
      FbleFree(arena, thread->running[s.callee]);
      thread->running[s.callee] = NULL;
    }
  }
  FbleFree(arena, thread->running);
  FbleFree(arena, thread->calls.xs);
  FbleFree(arena, thread->sample.xs);
  FbleFree(arena, thread);
}

// FbleProfileEnterBlock -- see documentation in fble-profile.h
void FbleProfileEnterBlock(FbleArena* arena, FbleProfileThread* thread, FbleBlockId block)
{
  assert(thread->calls.size > 0);

  Call* call = thread->calls.xs + thread->calls.size - 1;
  FbleBlockId caller = call->id;
  FbleBlockId callee = block;
  thread->profile->blocks.xs[callee]->block.count++;
  FbleCallData* data = GetCallData(arena, thread->profile, caller, callee);
  data->count++;

  if (!thread->auto_exit) {
    call = FbleVectorExtend(arena, thread->calls);
    call->exit = 0;
  }
  call->id = callee;
  thread->auto_exit = false;

  bool block_running = true;
  if (thread->running[callee] == NULL) {
    block_running = false;
    size_t n = thread->profile->blocks.size;
    thread->running[callee] = FbleArrayAlloc(arena, uint64_t, (n/64) + 1);
    memset(thread->running[callee], 0, ((n/64)+1) * sizeof(uint64_t));
  }

  uint64_t mask = 1 << (caller % 64);
  bool call_running = (thread->running[callee][caller / 64] & mask) != 0;
  if (!call_running) {
    thread->running[callee][caller / 64] |= mask;

    Sample* c = FbleVectorExtend(arena, thread->sample);
    c->caller = caller;
    c->callee = callee;
    c->call = data;
    c->new_block = !block_running;
    call->exit++;
  }
}

// FbleProfileTick -- see documentation in fble-profile.h
void FbleProfileTick(FbleArena* arena, FbleProfileThread* thread)
{
  if (thread->profile->ticks++ % thread->profile->period == 0) {
    FbleProfileSample(arena, thread, thread->profile->period);
  }
}

// FbleProfileSample -- see documentation in fble-profile.h
void FbleProfileSample(FbleArena* arena, FbleProfileThread* thread, uint64_t time)
{
  // Get the wall clock time since the last sample on this thread.
  uint64_t now = GetTimeMillis();
  if (thread->profile->wall == 0) {
    thread->profile->wall = now;
  }

  uint64_t wall = now - thread->profile->wall;
  thread->profile->wall = now;
  
  // Charge calls in the stack for their time.
  for (size_t i = 0; i < thread->sample.size; ++i) {
    Sample* c = thread->sample.xs + i;
    FbleCallData* data = c->call;
    if (c->new_block) {
      thread->profile->blocks.xs[c->callee]->block.time[FBLE_PROFILE_WALL_CLOCK] += wall;
      thread->profile->blocks.xs[c->callee]->block.time[FBLE_PROFILE_TIME_CLOCK] += time;
    }
    data->time[FBLE_PROFILE_WALL_CLOCK] += wall;
    data->time[FBLE_PROFILE_TIME_CLOCK] += time;
  }

  thread->profile->blocks.xs[FBLE_ROOT_BLOCK_ID]->block.time[FBLE_PROFILE_WALL_CLOCK] += wall;
  thread->profile->blocks.xs[FBLE_ROOT_BLOCK_ID]->block.time[FBLE_PROFILE_TIME_CLOCK] += time;
}

// FbleProfileExitBlock -- see documentation in fble-profile.h
void FbleProfileExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  assert(thread->calls.size > 0);
  thread->calls.size--;
  size_t exit = thread->calls.xs[thread->calls.size].exit;
  thread->sample.size -= exit;

  for (size_t i = 0; i < exit; ++i) {
    Sample s = thread->sample.xs[thread->sample.size + exit - i - 1];
    if (s.new_block) {
      FbleFree(arena, thread->running[s.callee]);
      thread->running[s.callee] = NULL;
    } else if (thread->running[s.callee] != NULL) {
      thread->running[s.callee][s.caller / 64] &= ~(1 << (s.caller % 64));
    }
  }
}

// FbleProfileAutoExitBlock -- see documentation in fble-profile.h
void FbleProfileAutoExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  thread->auto_exit = true;
}

// FbleProfileReport -- see documentation in fble-profile.h
void FbleProfileReport(FILE* fout, FbleNameV* blocks, FbleProfile* profile)
{
  FbleArena* arena = FbleNewArena();
  FbleCallData* calls[profile->blocks.size];

  // Number of blocks covered.
  size_t covered = 0;

  // Stats for correlating profile time (x) with wall clock time (y)
  uint64_t n = 0;
  uint64_t x = 0;  // sum of x_i
  uint64_t y = 0;  // sum of y_i
  uint64_t xx = 0;  // sum of x_i * x_i
  uint64_t yy = 0;  // sum of y_i * y_i
  uint64_t xy = 0;  // sum of x_i * y_i

  FbleCallDataV callers[profile->blocks.size];
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleVectorInit(arena, callers[i]);
  }

  for (size_t i = 0; i < profile->blocks.size; ++i) {
    calls[i] = &profile->blocks.xs[i]->block;

    if (calls[i]->count > 0) {
      covered++;
    }

    uint64_t x_i = calls[i]->time[FBLE_PROFILE_TIME_CLOCK];
    uint64_t y_i = calls[i]->time[FBLE_PROFILE_WALL_CLOCK];
    n++;
    x += x_i;
    y += y_i;
    xx += x_i * x_i;
    yy += y_i * y_i;
    xy += x_i * y_i;

    for (size_t j = 0; j < profile->blocks.xs[i]->callees.size; ++j) {
      FbleCallData* call = profile->blocks.xs[i]->callees.xs[j];
      FbleCallData* called = FbleAlloc(arena, FbleCallData);
      called->id = i;
      called->count = call->count;
      for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
        called->time[clock] = call->time[clock];
      }
      FbleVectorAppend(arena, callers[call->id], called);
    }
  }
  SortCallData(FBLE_PROFILE_TIME_CLOCK, DESCENDING, calls, profile->blocks.size);

  double coverage = (double)covered / (double)profile->blocks.size;
  double m = (double)xy / (double)xx;
  double r = (double)(n * xy - x * y)
           / (  sqrt((double)n*xx - (double)x*x)
              * sqrt((double)n*yy - (double)y*y));

  fprintf(fout, "Profile Report\n");
  fprintf(fout, "==============\n");
  fprintf(fout, "blocks executed: %2.2f%% of %zi\n", 100 * coverage, profile->blocks.size);
  fprintf(fout, "wall / time clock ratio: %f (r^2 = %f)\n", m, r*r);
  fprintf(fout, "\n");

  // Flat Profile
  fprintf(fout, "Flat Profile\n");
  fprintf(fout, "------------\n");
  fprintf(fout, "   %8s %8s %8s %s\n", "count", "wall", "time", "block");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    PrintCallData(fout, blocks, true, calls[i]);
  }
  fprintf(fout, "\n");

  // Call Graph
  fprintf(fout, "Call Graph\n");
  fprintf(fout, "----------\n");
  fprintf(fout, "   %8s %8s %8s %s\n", "count", "wall", "time", "block");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    size_t id = calls[i]->id;
    FbleBlockProfile* block = profile->blocks.xs[id];
    if (block->block.count > 0) {
      // Callers
      SortCallData(FBLE_PROFILE_TIME_CLOCK, ASCENDING, callers[id].xs, callers[id].size);
      for (size_t j = 0; j < callers[id].size; ++j) {
        PrintCallData(fout, blocks, false, callers[id].xs[j]);
      }

      // Block
      PrintCallData(fout, blocks, true, calls[i]);

      // Callees
      FbleCallData* callees[block->callees.size];
      for (size_t j = 0; j < block->callees.size; ++j) {
        callees[j] = block->callees.xs[j];
      }
      SortCallData(FBLE_PROFILE_TIME_CLOCK, DESCENDING, callees, block->callees.size);
      for (size_t j = 0; j < block->callees.size; ++j) {
        PrintCallData(fout, blocks, false, callees[j]);
      }
      fprintf(fout, "-------------------------------\n");
    }

    for (size_t j = 0; j < callers[id].size; ++j) {
      FbleFree(arena, callers[id].xs[j]);
    }
    FbleFree(arena, callers[id].xs);
  }
  fprintf(fout, "\n");
  FbleAssertEmptyArena(arena);
  FbleFreeArena(arena);

  // Locations
  fprintf(fout, "Block Locations\n");
  fprintf(fout, "---------------\n");
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleName* name = blocks->xs + profile->blocks.xs[i]->block.id;
    PrintBlockName(fout, blocks, profile->blocks.xs[i]->block.id);
    fprintf(fout, ": %s:%d:%d\n", name->loc.source, name->loc.line, name->loc.col);
  }
  fprintf(fout, "\n");
}

// FbleFreeBlockNames -- see documentation in fble-profile.h
void FbleFreeBlockNames(FbleArena* arena, FbleNameV* blocks)
{
  for (size_t i = 0; i < blocks->size; ++i) {
    FbleFree(arena, (char*)blocks->xs[i].name);
  }
  FbleFree(arena, blocks->xs);
}
