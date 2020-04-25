// profile.c --
//   This file implements the fble profiling routines.

#include <assert.h>   // for assert
#include <inttypes.h> // for PRIu64
#include <math.h>     // for sqrt
#include <stdbool.h>  // for bool
#include <string.h>   // for memset, memcpy
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

// CallStack --
//   A stack of calls.
//
// The call stack is organized into a linked list of chunks.
//
// Fields:
//   tail - the next chunk down in the stack.
//   next - the next (unused) chunk up in the stack. May be NULL.
//   top - pointer to the valid top entry on the stack.
//   end - pointer past the last allocated data in this chunk.
//   data - the raw data for this chunk.
typedef struct CallStack {
  struct CallStack* tail;
  struct CallStack* next;
  Call* top;
  Call* end;
  Call data[];
} CallStack;

// Sample --
//   Representation of a call in a sample.
//
// Fields:
//   caller - the caller for this particular call
//   callee - the callee for this particular call
//   call - cached result of GetCallData(caller, callee)
typedef struct {
  FbleBlockId caller;
  FbleBlockId callee;
  FbleCallData* call;
} Sample;

// SampleStack --
//   A stack of Samples. This is different from the call stack in that calls
//   to the same caller/callee appear at most once in the sample stack, and it
//   includes information about auto-exited calls.
typedef struct {
  size_t capacity;
  size_t size;
  Sample* xs;
} SampleStack;

// Entry --
//   Table entry used in a hash map from (caller,callee) pair.
//
// Fields:
//   sample - the index into the sample stack where we most recently stored a
//            call from caller to callee.
//   caller - the caller in the (caller, callee) pair.
//   data - cached FbleCallData for caller to callee. NULL to indicate invalid
//          entry.
typedef struct {
  size_t sample;
  size_t caller;
  FbleCallData* data;
} Entry;

// Table --
//   Hash map whose values are Entrys.
typedef struct {
  size_t capacity;
  size_t size;
  Entry* xs;
} Table;

// FbleProfileThread -- see documentation in fble-profile.h
struct FbleProfileThread {
  FbleProfile* profile;
  bool auto_exit;

  // Hash map from (caller,callee) to (sample, data).
  // Where sample is the index into sample.xs where we last had a sample call
  // from caller to callee.
  Table table;

  CallStack* calls;
  SampleStack sample;
};

typedef enum {
  ASCENDING,
  DESCENDING
} Order;

static Call* CallStackPush(FbleArena* arena, FbleProfileThread* thread);
static void CallStackPop(FbleArena* arena, FbleProfileThread* thread);

static FbleCallData* GetCallData(FbleArena* arena, FbleProfile* profile, FbleBlockId caller, FbleBlockId callee);
static void MergeSortCallData(FbleProfileClock clock, Order order, bool in_place, FbleCallData** a, FbleCallData** b, size_t size);
static void SortCallData(FbleProfileClock clock, Order order, FbleCallData** data, size_t size);
static void PrintBlockName(FILE* fout, FbleProfile* profile, FbleBlockId id);
static void PrintCallData(FILE* fout, FbleProfile* profile, bool highlight, FbleCallData* call);

static uint64_t GetTimeMillis();
static Entry* TableEntry(Table* table, FbleBlockId caller, FbleBlockId callee, size_t blockc);

// CallStackPush --
//   Push an uninitialized Call onto the call stack for the thread.
//
// Inputs:
//   arena - the arena to use for allocations.
//   thread - the thread whose call stack to push a value on.
//
// Results:
//   A pointer to the memory for the newly pushed value.
//
// Side effects:
//   Pushes a value on the stack that should be freed using CallStackPop when
//   no longer needed.
static Call* CallStackPush(FbleArena* arena, FbleProfileThread* thread)
{
  if (++thread->calls->top == thread->calls->end) {
    if (thread->calls->next == NULL) {
      size_t chunk_size = 2 * (thread->calls->end - thread->calls->data);
      CallStack* next = FbleAllocExtra(arena, CallStack, chunk_size * sizeof(Call));
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

// CallStackPop --
//   Pop a Call off of the call stack for the thread.
//
// Inputs:
//   arena - the arena to use for allocations.
//   thread - the thread whose call stack to pop.
//
// Side effects:
//   Pops a value on the stack that. Potentially invalidates any pointers
//   previously returned from CallStackPush.
static void CallStackPop(FbleArena* arena, FbleProfileThread* thread)
{
  if (thread->calls->top == thread->calls->data) {
    if (thread->calls->next != NULL) {
      FbleFree(arena, thread->calls->next);
      thread->calls->next = NULL;
    }
    thread->calls = thread->calls->tail;
  }
  thread->calls->top--;
}

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
//   profile - the profile, used for getting block names
//   id - the id of the block
//
// Results:
//   none.
//
// Side effects:
//   Prints a human readable description of the block to the given file.
static void PrintBlockName(FILE* fout, FbleProfile* profile, FbleBlockId id)
{
  fprintf(fout, "%s[%04x]", profile->blocks.xs[id]->name.name, id);
}

// PrintCallData --
//   Prints a line of call data.
//
// Inputs:
//   fout - the file to print to
//   profile - the profile, used for getting block names
//   highlight - if true, highlight the line
//   call - the call data to print
//
// Results:
//   none.
//
// Side effects:
//   Prints a single line description of the call data to the given file.
static void PrintCallData(FILE* fout, FbleProfile* profile, bool highlight, FbleCallData* call)
{
  uint64_t wall = call->time[FBLE_PROFILE_WALL_CLOCK];
  uint64_t time = call->time[FBLE_PROFILE_TIME_CLOCK];
  char h = highlight ? '*' : ' ';
  fprintf(fout, "%c%c %8" PRIu64 " %8" PRIu64 " %8" PRIu64 " ",
      h, h, call->count, wall, time);
  PrintBlockName(fout, profile, call->id);
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

// TableEntry --
//   Look up the entry for the given (caller,callee) pair.
//
// Inputs:
//   table - the table to look up in.
//   caller - the caller to look up.
//   callee - the callee to look up.
//   blockc - the number of blocks in the profile, for the purposes of
//            computing a hash.
//
// Results:
//   The entry that contains the (caller, callee) pair, or the entry where
//   the (caller,callee) pair should be inserted into the table if it is not
//   in the table.
static Entry* TableEntry(Table* table, FbleBlockId caller, FbleBlockId callee, size_t blockc)
{
  size_t hash = caller * blockc + callee;
  size_t bucket = hash % table->capacity;
  Entry* entry = table->xs + bucket;
  while (entry->data != NULL
      && (entry->caller != caller || entry->data->id != callee)) {
    bucket = (bucket + 1) % table->capacity;
    entry = table->xs + bucket;
  }
  return entry;
}

// FbleNewProfile -- see documentation in fble-profile.h
FbleProfile* FbleNewProfile(FbleArena* arena)
{
  FbleProfile* profile = FbleAlloc(arena, FbleProfile);
  profile->wall = 0;
  FbleVectorInit(arena, profile->blocks);
  return profile;
}

// FbleProfileAddBlock -- see documentation in fble-profile.h
FbleBlockId FbleProfileAddBlock(FbleArena* arena, FbleProfile* profile, FbleName name)
{
  FbleBlockId id = profile->blocks.size;
  FbleBlockProfile* block = FbleAlloc(arena, FbleBlockProfile);
  block->name = name;
  block->block.id = id;
  block->block.count = 0;
  for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
    block->block.time[clock] = 0;
  }
  FbleVectorInit(arena, block->callees);
  FbleVectorAppend(arena, profile->blocks, block);
  return id;
}

// FbleFreeProfile -- see documentation in fble-profile.h
void FbleFreeProfile(FbleArena* arena, FbleProfile* profile)
{
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];

    FbleFree(arena, (char*)block->name.name);
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
  thread->profile = profile;
  thread->auto_exit = false;

  if (parent == NULL) {
    thread->table.capacity = 10;
    thread->table.size = 0;
    thread->table.xs = FbleArrayAlloc(arena, Entry, thread->table.capacity);
    memset(thread->table.xs, 0, thread->table.capacity * sizeof(Entry));

    thread->calls = FbleAllocExtra(arena, CallStack, 8 * sizeof(CallStack));
    thread->calls->tail = NULL;
    thread->calls->next = NULL;
    thread->calls->top = thread->calls->data;
    thread->calls->end = thread->calls->data + 8;
    thread->calls->top->id = FBLE_ROOT_BLOCK_ID;
    thread->calls->top->exit = 0;

    thread->sample.capacity = 8;
    thread->sample.size = 0;
    thread->sample.xs = FbleArrayAlloc(arena, Sample, thread->sample.capacity);

    thread->profile->blocks.xs[FBLE_ROOT_BLOCK_ID]->block.count++;
  } else {
    assert(parent->profile == profile);

    thread->table.capacity = parent->table.capacity;
    thread->table.size = parent->table.size;
    thread->table.xs = FbleArrayAlloc(arena, Entry, thread->table.capacity);
    memcpy(thread->table.xs, parent->table.xs, thread->table.capacity * sizeof(Entry));

    // Copy the call stack.
    {
      CallStack* next = NULL;
      for (CallStack* p = parent->calls; p != NULL; p = p->tail) {
        size_t chunk_size = p->end - p->data;
        CallStack* c = FbleAllocExtra(arena, CallStack, chunk_size * sizeof(Call));
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
    thread->sample.xs = FbleArrayAlloc(arena, Sample, thread->sample.capacity);
    memcpy(thread->sample.xs, parent->sample.xs, parent->sample.size * sizeof(Sample));
  }
  return thread;
}

// FbleFreeProfileThread -- see documentation in fble-profile.h
void FbleFreeProfileThread(FbleArena* arena, FbleProfileThread* thread)
{
  CallStack* calls = thread->calls;
  if (calls->next != NULL) {
    calls = calls->next;
  }
  while (calls != NULL) {
    CallStack* tail = calls->tail;
    FbleFree(arena, calls);
    calls = tail;
  }

  FbleFree(arena, thread->table.xs);
  FbleFree(arena, thread->sample.xs);
  FbleFree(arena, thread);
}

// FbleProfileEnterBlock -- see documentation in fble-profile.h
void FbleProfileEnterBlock(FbleArena* arena, FbleProfileThread* thread, FbleBlockId block)
{
  Call* call = thread->calls->top;
  FbleBlockId caller = call->id;
  FbleBlockId callee = block;
  thread->profile->blocks.xs[callee]->block.count++;

  // Lookup the entry for this (caller,callee) in our table.
  Entry* entry = TableEntry(&thread->table, caller, callee, thread->profile->blocks.size);

  bool resize = false;
  if (entry->data == NULL) {
    // Entry not found. Insert it now.
    entry->sample = thread->sample.size;
    entry->caller = caller;
    entry->data = GetCallData(arena, thread->profile, caller, callee);
    thread->table.size++;

    // Check now if we should resize after this insertion. We won't resize
    // until after we're done updating the entry though.
    resize = (3 * thread->table.size > thread->table.capacity);
  }

  entry->data->count++;

  if (!thread->auto_exit) {
    call = CallStackPush(arena, thread);
    call->exit = 0;
  }
  call->id = callee;
  thread->auto_exit = false;

  // If the call was already running, we will find it on the sample stack
  // where our hash table says it should be.
  bool call_running = 
    entry->sample < thread->sample.size
    && entry->data == thread->sample.xs[entry->sample].call;

  if (!call_running) {
    entry->sample = thread->sample.size;

    if (thread->sample.size == thread->sample.capacity) {
      thread->sample.capacity *= 2;
      Sample* xs = FbleArrayAlloc(arena, Sample, thread->sample.capacity);
      memcpy(xs, thread->sample.xs, thread->sample.size * sizeof(Sample));
      FbleFree(arena, thread->sample.xs);
      thread->sample.xs = xs;
    }
    Sample* sample = thread->sample.xs + thread->sample.size++;
    sample->caller = caller;
    sample->callee = callee;
    sample->call = entry->data;
    call->exit++;
  }

  if (resize) {
    Table new_table;
    new_table.capacity = 2 * thread->table.capacity - 1;
    new_table.size = thread->table.size;
    new_table.xs = FbleArrayAlloc(arena, Entry, new_table.capacity);
    memset(new_table.xs, 0, new_table.capacity * sizeof(Entry));

    for (size_t i = 0; i < thread->table.capacity; ++i) {
      Entry e = thread->table.xs[i];
      if (e.data != NULL) {
        *TableEntry(&new_table, e.caller, e.data->id, thread->profile->blocks.size) = e;
      }
    }

    FbleFree(arena, thread->table.xs);
    thread->table = new_table;
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
  bool block_seen[thread->profile->blocks.size];
  memset(block_seen, 0, thread->profile->blocks.size * sizeof(bool));
  for (size_t i = 0; i < thread->sample.size; ++i) {
    Sample* sample = thread->sample.xs + i;
    FbleCallData* data = sample->call;
    if (!block_seen[sample->callee]) {
      block_seen[sample->callee] = true;
      thread->profile->blocks.xs[sample->callee]->block.time[FBLE_PROFILE_WALL_CLOCK] += wall;
      thread->profile->blocks.xs[sample->callee]->block.time[FBLE_PROFILE_TIME_CLOCK] += time;
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
  // TODO: Consider shrinking the sample stack occasionally to recover memory?
  thread->sample.size -= thread->calls->top->exit;
  CallStackPop(arena, thread);
}

// FbleProfileAutoExitBlock -- see documentation in fble-profile.h
void FbleProfileAutoExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  thread->auto_exit = true;
}

// FbleProfileReport -- see documentation in fble-profile.h
void FbleProfileReport(FILE* fout, FbleProfile* profile)
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
    PrintCallData(fout, profile, true, calls[i]);
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
        PrintCallData(fout, profile, false, callers[id].xs[j]);
      }

      // Block
      PrintCallData(fout, profile, true, calls[i]);

      // Callees
      FbleCallData* callees[block->callees.size];
      for (size_t j = 0; j < block->callees.size; ++j) {
        callees[j] = block->callees.xs[j];
      }
      SortCallData(FBLE_PROFILE_TIME_CLOCK, DESCENDING, callees, block->callees.size);
      for (size_t j = 0; j < block->callees.size; ++j) {
        PrintCallData(fout, profile, false, callees[j]);
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
    FbleName* name = &profile->blocks.xs[i]->name;
    PrintBlockName(fout, profile, profile->blocks.xs[i]->block.id);
    fprintf(fout, ": %s:%d:%d\n", name->loc.source, name->loc.line, name->loc.col);
  }
  fprintf(fout, "\n");
}
