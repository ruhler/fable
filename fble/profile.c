// profile.c --
//   This file implements the fble profiling routines.

#include <assert.h>   // for assert
#include <inttypes.h> // for PRIu64
#include <math.h>     // for sqrt
#include <stdbool.h>  // for bool
#include <sys/time.h> // for gettimeofday

#include "fble-alloc.h"
#include "fble-profile.h"
#include "fble-syntax.h"
#include "fble-vector.h"

// CallList --
//   A set of calls represented as a singly linked list.
//
// Fields:
//   caller - the caller for this particular call
//   callee - the callee for this particular call
//   new_block - true if this block was called recursively from itself.
//   new_call - true if this call was called recursively from another
//                    caller -> callee call.
//   tail - the rest of the calls in the set.
typedef struct CallList {
  FbleBlockId caller;
  FbleBlockId callee;
  bool new_block;
  bool new_call;
  struct CallList* tail;
} CallList;

// ProfileStack -- 
//   Stack representing the current call stack for profiling purposes.
//
// Fields:
//   id - the id of the block at the top of the stack.
//   time - the amount of time spent at the top of the stack.
//   auto_exit - true if we should automatically exit from this block.
//   exit_calls - the set of calls that should be exited when this call exits.
//   tail - the rest of the stack.
typedef struct ProfileStack {
  FbleBlockId id;
  uint64_t time[FBLE_PROFILE_NUM_CLOCKS];
  bool auto_exit;
  CallList* exit_calls;
  struct ProfileStack* tail;
} ProfileStack;

#define THREAD_SUSPENDED 0

// FbleProfileThread -- see documentation in fble-profile.h
struct FbleProfileThread {
  ProfileStack* stack;
  FbleCallGraph* graph;
  
  // The GetTimeMillis of the last call event for this thread. Set to
  // THREAD_SUSPENDED to indicate the thread is suspended.
  uint64_t start;
};

typedef enum {
  ASCENDING,
  DESCENDING
} Order;

static FbleCallData* GetCallData(FbleArena* arena, FbleCallGraph* graph, FbleBlockId caller, FbleBlockId callee);
static void MergeSortCallData(FbleProfileClock clock, Order order, bool in_place, FbleCallData** a, FbleCallData** b, size_t size);
static void SortCallData(FbleProfileClock clock, Order order, FbleCallData** data, size_t size);
static void PrintBlockName(FILE* fout, FbleNameV* blocks, FbleBlockId id);
static void PrintCallData(FILE* fout, FbleNameV* blocks, bool highlight, FbleCallData* call);

static uint64_t GetTimeMillis();
static void CallEvent(FbleProfileThread* thread);

// GetCallData --
//   Get the call data associated with the given caller/callee pair in the
//   call graph. Creates new empty call data and adds it to the graph as
//   required.
//
// Inputs:
//   arena - arena to use for allocations
//   graph - the call graph to get the data for
//   caller - the caller of the call
//   callee - the callee of the call
//
// Results:
//   The call data associated with the caller/callee pair in the given call
//   graph.
//
// Side effects:
//   Allocates new empty call data and adds it to the graph if necessary.
static FbleCallData* GetCallData(FbleArena* arena, FbleCallGraph* graph, FbleBlockId caller, FbleBlockId callee)
{
  for (size_t i = 0; i < graph->xs[caller]->callees.size; ++i) {
    if (graph->xs[caller]->callees.xs[i]->id == callee) {
      return graph->xs[caller]->callees.xs[i];
    }
  }

  FbleCallData* call = FbleAlloc(arena, FbleCallData);
  call->id = callee;
  for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
    call->time[clock] = 0;
  }
  call->count = 0;
  call->running = false;
  FbleVectorAppend(arena, graph->xs[caller]->callees, call);
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
  assert(time != THREAD_SUSPENDED && "uh oh. It's thread suspended time. :(");
  return time;
}

// CallEvent --
//   Record wall clock time spent in the current frame.
//
// Inputs:
//   thread - the current profile thread.
//
// Results:
//   none.
//
// Side effects:
//   Increments recorded time spent in the current call. Updates the start
//   counter for the current thread.
static void CallEvent(FbleProfileThread* thread)
{
  uint64_t now = GetTimeMillis();
  if (thread->start != THREAD_SUSPENDED) {
    thread->stack->time[FBLE_PROFILE_WALL_CLOCK] += (now - thread->start);
  }
  thread->start = now;
}

// FbleNewCallGraph -- see documentation in fble-profile.h
FbleCallGraph* FbleNewCallGraph(FbleArena* arena, size_t blockc)
{
  FbleCallGraph* graph = FbleAlloc(arena, FbleCallGraph);
  graph->size = blockc;
  graph->xs = FbleArrayAlloc(arena, FbleBlockProfile*, blockc);
  for (size_t i = 0; i < blockc; ++i) {
    graph->xs[i] = FbleAlloc(arena, FbleBlockProfile);
    graph->xs[i]->block.id = i;
    graph->xs[i]->block.count = 0;
    graph->xs[i]->block.running = false;
    for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
      graph->xs[i]->block.time[clock] = 0;
    }
    FbleVectorInit(arena, graph->xs[i]->callees);
    FbleVectorInit(arena, graph->xs[i]->callers);
  }
  return graph;
}

// FbleFreeCallGraph -- see documentation in fble-profile.h
void FbleFreeCallGraph(FbleArena* arena, FbleCallGraph* graph)
{
  for (size_t i = 0; i < graph->size; ++i) {
    FbleBlockProfile* block = graph->xs[i];

    for (size_t j = 0; j < block->callers.size; ++j) {
      FbleFree(arena, block->callers.xs[j]);
    }
    FbleFree(arena, block->callers.xs);

    for (size_t j = 0; j < block->callees.size; ++j) {
      FbleFree(arena, block->callees.xs[j]);
    }
    FbleFree(arena, block->callees.xs);

    FbleFree(arena, block);
  }
  FbleFree(arena, graph->xs);
  FbleFree(arena, graph);
}

// FbleNewProfileThread -- see documentation in fble-profile.h
FbleProfileThread* FbleNewProfileThread(FbleArena* arena, FbleCallGraph* graph)
{
  FbleProfileThread* thread = FbleAlloc(arena, FbleProfileThread);
  thread->graph = graph;
  thread->stack = FbleAlloc(arena, ProfileStack);
  thread->stack->id = 0;
  for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
    thread->stack->time[clock] = 0;
  }
  thread->stack->auto_exit = false;
  thread->stack->exit_calls = NULL;
  thread->stack->tail = NULL;
  thread->start = THREAD_SUSPENDED;
  return thread;
}

// FbleFreeProfileThread -- see documentation in fble-profile.h
void FbleFreeProfileThread(FbleArena* arena, FbleProfileThread* thread)
{
  assert(thread->stack != NULL);
  while (thread->stack->tail != NULL) {
    FbleProfileExitBlock(arena, thread);
  }
  assert(thread->stack->exit_calls == NULL);
  FbleFree(arena, thread->stack);
  FbleFree(arena, thread);
}

// FbleSuspendProfileThread -- see documentation in fble-profile.h
void FbleSuspendProfileThread(FbleProfileThread* thread)
{
  if (thread) {
    CallEvent(thread);
    thread->start = THREAD_SUSPENDED;
  }
}

// FbleResumeProfileThread -- see documentation in fble-profile.h
void FbleResumeProfileThread(FbleProfileThread* thread)
{
  if (thread && thread->start != THREAD_SUSPENDED) {
    thread->start = GetTimeMillis();
  }
}

// FbleProfileEnterBlock -- see documentation in fble-profile.h
void FbleProfileEnterBlock(FbleArena* arena, FbleProfileThread* thread, FbleBlockId block)
{
  assert(thread->stack != NULL);

  CallEvent(thread);

  FbleBlockId caller = thread->stack->id;
  FbleBlockId callee = block;
  thread->graph->xs[callee]->block.count++;
  FbleCallData* call = GetCallData(arena, thread->graph, caller, callee);
  call->count++;

  if (thread->stack->auto_exit) {
    for (CallList* c = thread->stack->exit_calls; c != NULL; c = c->tail) {
      FbleCallData* exit_call = GetCallData(arena, thread->graph, c->caller, c->callee);
      for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
        uint64_t advance = thread->stack->time[clock];
        if (c->new_block) {
          thread->graph->xs[c->callee]->block.time[clock] += advance;
        }

        if (c->new_call) {
          exit_call->time[clock] += advance;
        }
      }
    }

    assert(thread->stack->tail != NULL);
    for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
      thread->stack->tail->time[clock] += thread->stack->time[clock];
      thread->stack->time[clock] = 0;
    }

    thread->stack->id = callee;
    thread->stack->auto_exit = false;
  } else {
    ProfileStack* stack = FbleAlloc(arena, ProfileStack);
    stack->id = callee;
    for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
      stack->time[clock] = 0;
    }
    stack->auto_exit = false;
    stack->exit_calls = NULL;
    stack->tail = thread->stack;
    thread->stack = stack;
  }

  CallList* c = thread->stack->exit_calls;
  for (; c != NULL; c = c->tail) {
    if (c->caller == caller && c->callee == callee) {
      break;
    }
  }

  if (c == NULL) {
    c = FbleAlloc(arena, CallList);
    c->caller = caller;
    c->callee = callee;
    c->new_block = !thread->graph->xs[callee]->block.running;
    c->new_call = !call->running;
    c->tail = thread->stack->exit_calls;
    thread->stack->exit_calls = c;
  }
  thread->graph->xs[callee]->block.running = true;
  call->running = true;
}

// FbleProfileTime -- see documentation in fble-profile.h
void FbleProfileTime(FbleArena* arena, FbleProfileThread* thread, uint64_t time)
{
  thread->stack->time[FBLE_PROFILE_TIME_CLOCK] += time;
}

// FbleProfileExitBlock -- see documentation in fble-profile.h
void FbleProfileExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  assert(thread->stack != NULL);
  CallEvent(thread);
  while (thread->stack->exit_calls != NULL) {
    CallList* c = thread->stack->exit_calls;
    FbleCallData* call = GetCallData(arena, thread->graph, c->caller, c->callee);
    for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
      uint64_t advance = thread->stack->time[clock];
      if (c->new_block) {
        thread->graph->xs[c->callee]->block.time[clock] += advance;
        thread->graph->xs[c->callee]->block.running = false;
      }
      if (c->new_call) {
        call->time[clock] += advance;
        call->running = false;
      }
    }

    thread->stack->exit_calls = c->tail;
    FbleFree(arena, c);
  }

  assert(thread->stack->tail != NULL);
  for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
    thread->stack->tail->time[clock] += thread->stack->time[clock];
  }

  ProfileStack* stack = thread->stack;
  thread->stack = thread->stack->tail;
  FbleFree(arena, stack);
}

// FbleProfileAutoExitBlock -- see documentation in fble-profile.h
void FbleProfileAutoExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  thread->stack->auto_exit = true;
}

// FbleProcessCallGraph -- see documentation in fble-profile.h
void FbleProcessCallGraph(FbleArena* arena, FbleCallGraph* graph)
{
  // Treat block 0 specially so it represents the total execution for the
  // program. We assume there are no incoming calls to block 0.
  for (size_t i = 0; i < graph->xs[0]->callees.size; ++i) {
    graph->xs[0]->block.count += graph->xs[0]->callees.xs[i]->count;
    for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
      graph->xs[0]->block.time[clock] += graph->xs[0]->callees.xs[i]->time[clock];
    }
  }

  SortCallData(FBLE_PROFILE_TIME_CLOCK, DESCENDING, ((FbleCallDataV*)graph)->xs, ((FbleCallDataV*)graph)->size);
}

// FbleDumpProfile -- see documentation in fble-profile.h
void FbleDumpProfile(FILE* fout, FbleNameV* blocks, FbleCallGraph* graph)
{
  // Code Coverage
  size_t covered = 0;
  for (size_t i = 0; i < graph->size; ++i) {
    if (graph->xs[i]->block.count > 0) {
      covered++;
    }
  }

  double coverage = (double)covered / (double)graph->size;
  fprintf(fout, "Code Coverage\n");
  fprintf(fout, "-------------\n");
  fprintf(fout, "Blocks executed: %2.2f%% of %zi\n\n", 100 * coverage, graph->size);

  // Flat Profile
  fprintf(fout, "Flat Profile\n");
  fprintf(fout, "------------\n");
  fprintf(fout, "   %8s %8s %8s %s\n", "count", "wall", "time", "block");
  for (size_t i = 0; i < graph->size; ++i) {
    PrintCallData(fout, blocks, true, &graph->xs[i]->block);
  }
  fprintf(fout, "\n");

  // Compute correlation between wall time and profile time.
  fprintf(fout, "Wall / Profile Time Correlation\n");
  fprintf(fout, "-------------------------------\n");
  uint64_t n = 0;
  uint64_t x = 0;  // sum of x_i
  uint64_t y = 0;  // sum of y_i
  uint64_t xx = 0;  // sum of x_i * x_i
  uint64_t yy = 0;  // sum of y_i * y_i
  uint64_t xy = 0;  // sum of x_i * y_i
  for (size_t i = 0; i < graph->size; ++i) {
    FbleCallData* call = &graph->xs[i]->block;
    uint64_t x_i = call->time[FBLE_PROFILE_TIME_CLOCK];
    uint64_t y_i = call->time[FBLE_PROFILE_WALL_CLOCK];
    n++;
    x += x_i;
    y += y_i;
    xx += x_i * x_i;
    yy += y_i * y_i;
    xy += x_i * y_i;
  }
  double m = (double)xy / (double)xx;
  double r = (double)(n * xy - x * y)
           / (  sqrt((double)n*xx - (double)x*x)
              * sqrt((double)n*yy - (double)y*y));
  fprintf(fout, "m = %f\nr = %f\n\n", m, r);

  // Call Graph
  fprintf(fout, "Call Graph\n");
  fprintf(fout, "----------\n");
  fprintf(fout, "   %8s %8s %8s %s\n", "count", "wall", "time", "block");

  FbleArena* arena = FbleNewArena();
  FbleCallDataV callers[graph->size];
  for (size_t i = 0; i < graph->size; ++i) {
    FbleVectorInit(arena, callers[i]);
  }

  for (size_t i = 0; i < graph->size; ++i) {
    for (size_t j = 0; j < graph->xs[i]->callees.size; ++j) {
      FbleCallData* call = graph->xs[i]->callees.xs[j];
      FbleCallData* called = FbleAlloc(arena, FbleCallData);
      called->id = i;
      called->count = call->count;
      for (FbleProfileClock clock = 0; clock < FBLE_PROFILE_NUM_CLOCKS; ++clock) {
        called->time[clock] = call->time[clock];
      }
      FbleVectorAppend(arena, callers[call->id], called);
    }
  }

  for (size_t i = 0; i < graph->size; ++i) {
    FbleBlockProfile* block = graph->xs[i];
    if (block->block.count > 0) {
      // Callers
      SortCallData(FBLE_PROFILE_TIME_CLOCK, ASCENDING, callers[i].xs, callers[i].size);

      // Block
      PrintCallData(fout, blocks, true, &graph->xs[i]->block);

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

    for (size_t j = 0; j < callers[i].size; ++j) {
      FbleFree(arena, callers[i].xs[j]);
    }
    FbleFree(arena, callers[i].xs);
  }
  fprintf(fout, "\n");
  FbleAssertEmptyArena(arena);
  FbleDeleteArena(arena);

  // Locations
  fprintf(fout, "Block Locations\n");
  fprintf(fout, "---------------\n");
  for (size_t i = 0; i < graph->size; ++i) {
    FbleName* name = blocks->xs + graph->xs[i]->block.id;
    PrintBlockName(fout, blocks, graph->xs[i]->block.id);
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
