// profile.c --
//   This file implements the fble profiling routines.

#include "fble-alloc.h"
#include "fble-profile.h"
#include "fble-syntax.h"
#include "fble-vector.h"

// ProfileStack -- 
//   Stack representing the current call stack for profiling purposes.
//
// Fields:
//   id - the id of the block at the top of the stack.
//   time - the amount of time spent at the top of the stack.
//   tail_call - true if the call into this block was a tail call.
//   tail - the rest of the stack.
typedef struct ProfileStack {
  FbleBlockId id;
  size_t time;
  bool tail_call;
  struct ProfileStack* tail;
} ProfileStack;

// FbleProfileThread -- see documentation in fble-profile.h
struct FbleProfileThread {
  ProfileStack* stack;
  FbleCallGraph* graph;
};

static void FixupCycles(FbleArena* arena, FbleCallGraph* graph, FbleBlockIdV* seen, FbleBlockId root);
static void MergeSortCallData(bool in_place, FbleCallData** a, FbleCallData** b, size_t size);
static void SortCallData(FbleCallDataV data);
static void PrintBlockName(FILE* fout, FbleNameV* blocks, FbleBlockId id);
static void PrintCallData(FILE* fout, FbleNameV* blocks, bool highlight, FbleCallData* call);

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

  for (size_t i = 0; i < graph->xs[root].size; ++i) {
    FbleBlockId callee = graph->xs[root].xs[i]->id;
    FbleVectorAppend(arena, *seen, callee);
    FixupCycles(arena, graph, seen, callee);
    seen->size--;
  }
}

// MergeSortCallData --
//   Does a merge sort of call data. There are two modes for sorting:
//   1. in_place - a is sorted in place, using b as a scratch buffer
//   2. !in_place - a is sorted into b, then a can be used as a scratch buffer
//
// Inputs:
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
static void MergeSortCallData(bool in_place, FbleCallData** a, FbleCallData** b, size_t size)
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
  MergeSortCallData(!in_place, a1, b1, size1);

  FbleCallData** a2 = a + size1;
  FbleCallData** b2 = b + size1;
  size_t size2 = size - size1;
  MergeSortCallData(!in_place, a2, b2, size2);

  FbleCallData** s1 = in_place ? b1 : a1;
  FbleCallData** s2 = in_place ? b2 : a2;
  FbleCallData** d = in_place ? a : b;

  FbleCallData** e1 = s1 + size1;
  FbleCallData** e2 = s2 + size2;
  while (s1 < e1 && s2 < e2) {
    if ((*s1)->time <= (*s2)->time) {
      *d++ = *s1++;
    } else {
      *d++ = *s2++;
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
//   data - the call data to sort
//
// Results:
//   none.
//
// Side effects:
//   Sorts the given array of call data in increasing order of time.
static void SortCallData(FbleCallDataV data)
{
  FbleCallData* scratch[data.size];
  MergeSortCallData(true, data.xs, scratch, data.size);
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
  thread->stack = NULL;
  FbleProfileEnterCall(arena, thread, 0);
  return thread;
}

// FbleFreeProfileThread -- see documentation in fble-profile.h
void FbleFreeProfileThread(FbleArena* arena, FbleProfileThread* thread)
{
  while (thread->stack != NULL) {
    ProfileStack* tail = thread->stack->tail;
    FbleFree(arena, thread->stack);
    thread->stack = tail;
  }
  FbleFree(arena, thread);
}

// FbleProfileEnterCall -- see documentation in fble-profile.h
void FbleProfileEnterCall(FbleArena* arena, FbleProfileThread* thread, FbleBlockId callee)
{
  ProfileStack* stack = FbleAlloc(arena, ProfileStack);
  stack->id = callee;
  stack->time = 0;
  stack->tail_call = false;
  stack->tail = thread->stack;
  thread->stack = stack;
}

// FbleProfileEnterTailCall -- see documentation in fble-profile.h
void FbleProfileEnterTailCall(FbleArena* arena, FbleProfileThread* thread, FbleBlockId callee)
{
  ProfileStack* stack = FbleAlloc(arena, ProfileStack);
  stack->id = callee;
  stack->time = 0;
  stack->tail_call = true;
  stack->tail = thread->stack;
  thread->stack = stack;
}

// FbleProfileTime -- see documentation in fble-profile.h
void FbleProfileTime(FbleArena* arena, FbleProfileThread* thread, size_t time)
{
  thread->stack->time += time;
}

// FbleProfileExitCall -- see documentation in fble-profile.h
void FbleProfileExitCall(FbleArena* arena, FbleProfileThread* thread)
{
  bool exit = true;
  while (exit) {
    exit = thread->stack->tail_call;

    FbleBlockId caller = thread->stack->tail->id;
    FbleBlockId callee = thread->stack->id;

    FbleCallData* call = NULL;
    for (size_t i = 0; i < thread->graph->xs[caller].size; ++i) {
      if (thread->graph->xs[caller].xs[i]->id == callee) {
        call = thread->graph->xs[caller].xs[i];
        break;
      }
    }

    if (call == NULL) {
      call = FbleAlloc(arena, FbleCallData);
      call->id = callee;
      call->time = 0;
      call->count = 0;
      FbleVectorAppend(arena, thread->graph->xs[caller], call);
    }

    size_t time = thread->stack->time;
    call->count++;
    call->time += time;

    ProfileStack* tail = thread->stack->tail;
    FbleFree(arena, thread->stack);
    thread->stack = tail;
    thread->stack->time += time;
  }
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

  for (size_t i = 0; i < profile->size; ++i) {
    SortCallData(profile->xs[i]->callers);
    SortCallData(profile->xs[i]->callees);
  }
  SortCallData(*(FbleCallDataV*)profile);
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
    PrintCallData(fout, blocks, true, &profile->xs[profile->size - i - 1]->block);
  }
  fprintf(fout, "\n");

  // Call Graph
  fprintf(fout, "Call Graph\n");
  fprintf(fout, "----------\n");
  for (size_t i = 0; i < profile->size; ++i) {
    FbleBlockProfile* block = profile->xs[profile->size - i - 1];
    for (size_t j = 0; j < block->callers.size; ++j) {
      PrintCallData(fout, blocks, false, block->callers.xs[j]);
    }
    PrintCallData(fout, blocks, true, &profile->xs[profile->size - i - 1]->block);
    size_t callees_size = block->callees.size;
    for (size_t j = 0; j < callees_size; ++j) {
      PrintCallData(fout, blocks, false, block->callees.xs[callees_size - j - 1]);
    }
    fprintf(fout, "--------------------------\n");
  }
  fprintf(fout, "\n");
}
