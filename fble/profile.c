// profile.c --
//   This file implements the fble profiling routines.

#include <assert.h>   // for assert

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
//   count - the number of times caller called callee
//   tail - the rest of the calls in the set.
typedef struct CallList {
  FbleBlockId caller;
  FbleBlockId callee;
  size_t count;
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
  size_t time;
  bool auto_exit;
  CallList* exit_calls;
  struct ProfileStack* tail;
} ProfileStack;

// FbleProfileThread -- see documentation in fble-profile.h
struct FbleProfileThread {
  ProfileStack* stack;
  FbleCallGraph* graph;
};

static FbleCallData* GetCallData(FbleArena* arena, FbleCallGraph* graph, FbleBlockId caller, FbleBlockId callee);
static void FixupCycles(FbleArena* arena, FbleCallGraph* graph, FbleBlockIdV* seen, FbleBlockId root);
static void MergeSortCallData(bool ascending, bool in_place, FbleCallData** a, FbleCallData** b, size_t size);
static void SortCallData(bool ascending, FbleCallDataV data);
static void PrintBlockName(FILE* fout, FbleNameV* blocks, FbleBlockId id);
static void PrintCallData(FILE* fout, FbleNameV* blocks, bool highlight, FbleCallData* call);

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
  for (size_t i = 0; i < graph->xs[caller].size; ++i) {
    if (graph->xs[caller].xs[i]->id == callee) {
      return graph->xs[caller].xs[i];
    }
  }

  FbleCallData* call = FbleAlloc(arena, FbleCallData);
  call->id = callee;
  call->time = 0;
  call->count = 0;
  FbleVectorAppend(arena, graph->xs[caller], call);
  return call;
}

// FixupCycles --
//   Removes double-counting of time due to cycles in the given call graph of
//   blocks.
//
// Inputs:
//   arena - arena used for allocations.
//   graph - the call graph of blocks to fix up.
//   seen - the set of ids seen so far.
//   root - the root block to fixup cycles for.
//
// Results:
//   none.
//
// Side effects:
//   Removes double counting of time in callees due to cycles.
static void FixupCycles(FbleArena* arena, FbleCallGraph* graph, FbleBlockIdV* seen, FbleBlockId root)
{
  for (size_t i = 0; i < seen->size; ++i) {
    if (root == seen->xs[i]) {
      // We have encountered a cycle. Remove all internal references.
      for (size_t j = i; j < seen->size; ++j) {
        FbleBlockId x = seen->xs[j];
        for (size_t k = i; k < seen->size; ++k) {
          FbleBlockId y = seen->xs[k];
          for (size_t l = 0; l < graph->xs[x].size; ++l) {
            if (graph->xs[x].xs[l]->id == y) {
              graph->xs[x].xs[l]->time = 0;
            }
          }
        }
      }
      return;
    }
  }

  FbleVectorAppend(arena, *seen, root);
  for (size_t i = 0; i < graph->xs[root].size; ++i) {
    FbleBlockId callee = graph->xs[root].xs[i]->id;
    FixupCycles(arena, graph, seen, callee);
  }
  seen->size--;
}

// MergeSortCallData --
//   Does a merge sort of call data. There are two modes for sorting:
//   1. in_place - a is sorted in place, using b as a scratch buffer
//   2. !in_place - a is sorted into b, then a can be used as a scratch buffer
//
// Inputs:
//   ascending - if true, sort in ascending order, otherwise in descending order
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
static void MergeSortCallData(bool ascending, bool in_place, FbleCallData** a, FbleCallData** b, size_t size)
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
  MergeSortCallData(ascending, !in_place, a1, b1, size1);

  FbleCallData** a2 = a + size1;
  FbleCallData** b2 = b + size1;
  size_t size2 = size - size1;
  MergeSortCallData(ascending, !in_place, a2, b2, size2);

  FbleCallData** s1 = in_place ? b1 : a1;
  FbleCallData** s2 = in_place ? b2 : a2;
  FbleCallData** d = in_place ? a : b;

  FbleCallData** e1 = s1 + size1;
  FbleCallData** e2 = s2 + size2;
  while (s1 < e1 && s2 < e2) {
    if (ascending) {
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

// SortCallData --
//   Sort a vector of call data by time.
//
// Inputs:
//   ascending - if true, sort in ascending order, otherwise in descending order
//   data - the call data to sort
//
// Results:
//   none.
//
// Side effects:
//   Sorts the given array of call data in increasing order of time.
static void SortCallData(bool ascending, FbleCallDataV data)
{
  FbleCallData* scratch[data.size];
  MergeSortCallData(ascending, true, data.xs, scratch, data.size);
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
  fprintf(fout, "[%04x]%s@%s:%d:%d", id, name->name, name->loc.source, name->loc.line, name->loc.col);
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
  char h = highlight ? '*' : ' ';
  fprintf(fout, " %c % 10d % 10d ", h, call->count, call->time);
  PrintBlockName(fout, blocks, call->id);
  fprintf(fout, "\n");
}

// FbleNewCallGraph -- see documentation in fble-profile.h
FbleCallGraph* FbleNewCallGraph(FbleArena* arena, size_t blockc)
{
  FbleCallGraph* graph = FbleAlloc(arena, FbleCallGraph);
  graph->size = blockc;
  graph->xs = FbleArrayAlloc(arena, FbleCallDataV, blockc);
  for (size_t i = 0; i < blockc; ++i) {
    FbleVectorInit(arena, graph->xs[i]);
  }
  return graph;
}

// FbleFreeCallGraph -- see documentation in fble-profile.h
void FbleFreeCallGraph(FbleArena* arena, FbleCallGraph* graph)
{
  for (size_t i = 0; i < graph->size; ++i) {
    for (size_t j = 0; j < graph->xs[i].size; ++j) {
      FbleFree(arena, graph->xs[i].xs[j]);
    }
    FbleFree(arena, graph->xs[i].xs);
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
  thread->stack->time = 0;
  thread->stack->auto_exit = false;
  thread->stack->exit_calls = NULL;
  thread->stack->tail = NULL;
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

// FbleProfileEnterBlock -- see documentation in fble-profile.h
void FbleProfileEnterBlock(FbleArena* arena, FbleProfileThread* thread, FbleBlockId block)
{
  assert(thread->stack != NULL);

  FbleBlockId caller = thread->stack->id;
  FbleBlockId callee = block;
  GetCallData(arena, thread->graph, caller, callee)->count++;

  if (thread->stack->auto_exit) {
    for (CallList* c = thread->stack->exit_calls; c != NULL; c = c->tail) {
      FbleCallData* call = GetCallData(arena, thread->graph, c->caller, c->callee);
      call->time += thread->stack->time * c->count;
    }

    assert(thread->stack->tail != NULL);
    thread->stack->tail->time += thread->stack->time;

    thread->stack->id = callee;
    thread->stack->time = 0;
    thread->stack->auto_exit = false;
  } else {
    ProfileStack* stack = FbleAlloc(arena, ProfileStack);
    stack->id = callee;
    stack->time = 0;
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
    c->count = 0;
    c->tail = thread->stack->exit_calls;
    thread->stack->exit_calls = c;
  }
  c->count++;
}

// FbleProfileTime -- see documentation in fble-profile.h
void FbleProfileTime(FbleArena* arena, FbleProfileThread* thread, size_t time)
{
  thread->stack->time += time;
}

// FbleProfileExitBlock -- see documentation in fble-profile.h
void FbleProfileExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  assert(thread->stack != NULL);
  while (thread->stack->exit_calls != NULL) {
    CallList* c = thread->stack->exit_calls;
    FbleCallData* call = GetCallData(arena, thread->graph, c->caller, c->callee);
    call->time += thread->stack->time * c->count;

    thread->stack->exit_calls = c->tail;
    FbleFree(arena, c);
  }

  assert(thread->stack->tail != NULL);
  thread->stack->tail->time += thread->stack->time;

  ProfileStack* stack = thread->stack;
  thread->stack = thread->stack->tail;
  FbleFree(arena, stack);
}

// FbleProfileAutoExitBlock -- see documentation in fble-profile.h
void FbleProfileAutoExitBlock(FbleArena* arena, FbleProfileThread* thread)
{
  thread->stack->auto_exit = true;
}

// FbleComputeProfile -- see documentation in fble-profile.h
FbleProfile* FbleComputeProfile(FbleArena* arena, FbleCallGraph* graph)
{
  FbleBlockIdV seen;
  FbleVectorInit(arena, seen);
  FixupCycles(arena, graph, &seen, 0);
  FbleFree(arena, seen.xs);

  FbleProfile* profile = FbleAlloc(arena, FbleProfile);
  profile->size = graph->size;
  profile->xs = FbleArrayAlloc(arena, FbleBlockProfile*, graph->size);
  for (size_t i = 0; i < graph->size; ++i) {
    profile->xs[i] = FbleAlloc(arena, FbleBlockProfile);
    profile->xs[i]->block.id = i;
    profile->xs[i]->block.count = 0;
    profile->xs[i]->block.time = 0;
    FbleVectorInit(arena, profile->xs[i]->callers);
    FbleVectorInit(arena, profile->xs[i]->callees);
  }

  for (FbleBlockId caller = 0; caller < graph->size; ++caller) {
    for (size_t i = 0; i < graph->xs[caller].size; ++i) {
      FbleBlockId callee = graph->xs[caller].xs[i]->id;
      FbleCallData* call = graph->xs[caller].xs[i];

      FbleCallData* a = &profile->xs[callee]->block;
      a->count += call->count;
      a->time += call->time;

      FbleCallData* b = NULL;
      for (size_t j = 0; j < profile->xs[callee]->callers.size; ++j) {
        if (profile->xs[callee]->callers.xs[j]->id == caller) {
          b = profile->xs[callee]->callers.xs[j];
          break;
        }
      }
      if (b == NULL) {
        b = FbleAlloc(arena, FbleCallData);
        b->id = caller;
        b->time = 0;
        b->count = 0;
        FbleVectorAppend(arena, profile->xs[callee]->callers, b);
      }
      b->count += call->count;
      b->time += call->time;

      FbleCallData* c = NULL;
      for (size_t j = 0; j < profile->xs[caller]->callees.size; ++j) {
        if (profile->xs[caller]->callees.xs[j]->id == callee) {
          c = profile->xs[caller]->callees.xs[j];
          break;
        }
      }
      if (c == NULL) {
        c = FbleAlloc(arena, FbleCallData);
        c->id = callee;
        c->time = 0;
        c->count = 0;
        FbleVectorAppend(arena, profile->xs[caller]->callees, c);
      }
      c->count += call->count;
      c->time += call->time;
    }
  }

  // Treat block 0 specially so it represents the total execution for the
  // program. We assume there are no incoming calls to block 0.
  for (size_t i = 0; i < graph->xs[0].size; ++i) {
    profile->xs[0]->block.count += graph->xs[0].xs[i]->count;
    profile->xs[0]->block.time += graph->xs[0].xs[i]->time;
  }

  for (size_t i = 0; i < profile->size; ++i) {
    SortCallData(true /* ascending */, profile->xs[i]->callers);
    SortCallData(false /*ascending */, profile->xs[i]->callees);
  }
  SortCallData(false /* ascending */, *(FbleCallDataV*)profile);
  return profile;
}

// FbleFreeProfile -- see documentation in fble-profile.h
void FbleFreeProfile(FbleArena* arena, FbleProfile* profile)
{
  for (size_t i = 0; i < profile->size; ++i) {
    FbleBlockProfile* block = profile->xs[i];

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
  FbleFree(arena, profile->xs);
  FbleFree(arena, profile);
}

// FbleDumpProfile -- see documentation in fble-profile.h
void FbleDumpProfile(FILE* fout, FbleNameV* blocks, FbleProfile* profile)
{
  // Code Coverage
  size_t covered = 0;
  for (size_t i = 0; i < profile->size; ++i) {
    if (profile->xs[i]->block.count > 0) {
      covered++;
    }
  }

  double coverage = (double)covered / (double)profile->size;
  fprintf(fout, "Code Coverage\n");
  fprintf(fout, "-------------\n");
  fprintf(fout, "Blocks executed: %2.2f%% of %zi\n\n", 100 * coverage, profile->size);

  // Uncovered blocks
  fprintf(fout, "Uncovered Blocks\n");
  fprintf(fout, "----------------\n");
  for (size_t i = 0; i < profile->size; ++i) {
    if (profile->xs[i]->block.count == 0) {
      FbleBlockId id = profile->xs[i]->block.id;
      PrintBlockName(fout, blocks, id);
      fprintf(fout, "\n");
    }
  }
  fprintf(fout, "\n");

  // Flat Profile
  fprintf(fout, "Flat Profile\n");
  fprintf(fout, "------------\n");
  fprintf(fout, "   %10s %10s %s\n", "count", "time", "block");
  for (size_t i = 0; i < profile->size; ++i) {
    PrintCallData(fout, blocks, true, &profile->xs[i]->block);
  }
  fprintf(fout, "\n");

  // Call Graph
  fprintf(fout, "Call Graph\n");
  fprintf(fout, "----------\n");
  fprintf(fout, "   %10s %10s %s\n", "count", "time", "block");
  for (size_t i = 0; i < profile->size; ++i) {
    FbleBlockProfile* block = profile->xs[i];
    for (size_t j = 0; j < block->callers.size; ++j) {
      PrintCallData(fout, blocks, false, block->callers.xs[j]);
    }
    PrintCallData(fout, blocks, true, &profile->xs[i]->block);
    size_t callees_size = block->callees.size;
    for (size_t j = 0; j < callees_size; ++j) {
      PrintCallData(fout, blocks, false, block->callees.xs[j]);
    }
    fprintf(fout, "--------------------------\n");
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
