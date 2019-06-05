// fble-profile-test.c --
//   This file implements the main entry point for the fble-profile-test
//   program.

#include <assert.h>   // for assert

#include "fble-profile.h"

// main --
//   The main entry point for the fble-profile-test program.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Prints an error to stderr and exits the program in the case of error.
int main(int argc, char* argv[])
{
  FbleArena* arena = FbleNewArena();

  {
    // Test a simple call graph:
    // 0 -> 1 -> 2 -> 3
    //             -> 4
    //        -> 3
    FbleCallGraph* graph = FbleNewCallGraph(arena, 5);
    FbleProfileThread* thread = FbleNewProfileThread(arena, graph);
    FbleProfileEnterCall(arena, thread, 1);
    FbleProfileTime(arena, thread, 10);
    FbleProfileEnterCall(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterCall(arena, thread, 3);
    FbleProfileTime(arena, thread, 30);
    FbleProfileExitCall(arena, thread); // 3
    FbleProfileEnterCall(arena, thread, 4);
    FbleProfileTime(arena, thread, 40);
    FbleProfileExitCall(arena, thread); // 4
    FbleProfileExitCall(arena, thread); // 2
    FbleProfileEnterCall(arena, thread, 3);
    FbleProfileTime(arena, thread, 31);
    FbleProfileExitCall(arena, thread); // 3
    FbleProfileExitCall(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    assert(graph->size == 5);
    assert(graph->xs[0].size == 1);
    assert(graph->xs[0].xs[0]->id == 1);
    assert(graph->xs[0].xs[0]->count == 1);
    assert(graph->xs[0].xs[0]->time == 131);
    assert(graph->xs[1].size == 2);
    assert(graph->xs[1].xs[0]->id == 2);
    assert(graph->xs[1].xs[0]->count == 1);
    assert(graph->xs[1].xs[0]->time == 90);
    assert(graph->xs[1].xs[1]->id == 3);
    assert(graph->xs[1].xs[1]->count == 1);
    assert(graph->xs[1].xs[1]->time == 31);
    assert(graph->xs[2].size == 2);
    assert(graph->xs[2].xs[0]->id == 3);
    assert(graph->xs[2].xs[0]->count == 1);
    assert(graph->xs[2].xs[0]->time == 30);
    assert(graph->xs[2].xs[1]->id == 4);
    assert(graph->xs[2].xs[1]->count == 1);
    assert(graph->xs[2].xs[1]->time == 40);
    assert(graph->xs[3].size == 0);
    assert(graph->xs[4].size == 0);

    FbleProfile* profile = FbleComputeProfile(arena, graph);
    assert(profile->size == 5);
    assert(profile->xs[0]->callers.size == 0);
    assert(profile->xs[0]->block.id == 0);
    assert(profile->xs[0]->block.count == 1);
    assert(profile->xs[0]->block.time == 131);
    assert(profile->xs[0]->callees.size == 1);
    assert(profile->xs[0]->callees.xs[0]->id == 1);
    assert(profile->xs[0]->callees.xs[0]->count == 1);
    assert(profile->xs[0]->callees.xs[0]->time == 131);

    assert(profile->xs[1]->callers.size == 1);
    assert(profile->xs[1]->callers.xs[0]->id == 0);
    assert(profile->xs[1]->callers.xs[0]->count == 1);
    assert(profile->xs[1]->callers.xs[0]->time == 131);
    assert(profile->xs[1]->block.id == 1);
    assert(profile->xs[1]->block.count == 1);
    assert(profile->xs[1]->block.time == 131);
    assert(profile->xs[1]->callees.size == 2);
    assert(profile->xs[1]->callees.xs[0]->id == 2);
    assert(profile->xs[1]->callees.xs[0]->count == 1);
    assert(profile->xs[1]->callees.xs[0]->time == 90);
    assert(profile->xs[1]->callees.xs[1]->id == 3);
    assert(profile->xs[1]->callees.xs[1]->count == 1);
    assert(profile->xs[1]->callees.xs[1]->time == 31);

    assert(profile->xs[2]->callers.size == 1);
    assert(profile->xs[2]->callers.xs[0]->id == 1);
    assert(profile->xs[2]->callers.xs[0]->count == 1);
    assert(profile->xs[2]->callers.xs[0]->time == 90);
    assert(profile->xs[2]->block.id == 2);
    assert(profile->xs[2]->block.count == 1);
    assert(profile->xs[2]->block.time == 90);
    assert(profile->xs[2]->callees.size == 2);
    assert(profile->xs[2]->callees.xs[0]->id == 4);
    assert(profile->xs[2]->callees.xs[0]->count == 1);
    assert(profile->xs[2]->callees.xs[0]->time == 40);
    assert(profile->xs[2]->callees.xs[1]->id == 3);
    assert(profile->xs[2]->callees.xs[1]->count == 1);
    assert(profile->xs[2]->callees.xs[1]->time == 30);

    assert(profile->xs[3]->callers.size == 2);
    assert(profile->xs[3]->callers.xs[0]->id == 2);
    assert(profile->xs[3]->callers.xs[0]->count == 1);
    assert(profile->xs[3]->callers.xs[0]->time == 30);
    assert(profile->xs[3]->callers.xs[1]->id == 1);
    assert(profile->xs[3]->callers.xs[1]->count == 1);
    assert(profile->xs[3]->callers.xs[1]->time == 31);
    assert(profile->xs[3]->block.id == 3);
    assert(profile->xs[3]->block.count == 2);
    assert(profile->xs[3]->block.time == 61);
    assert(profile->xs[3]->callees.size == 0);

    assert(profile->xs[4]->callers.size == 1);
    assert(profile->xs[4]->callers.xs[0]->id == 2);
    assert(profile->xs[4]->callers.xs[0]->count == 1);
    assert(profile->xs[4]->callers.xs[0]->time == 40);
    assert(profile->xs[4]->block.id == 4);
    assert(profile->xs[4]->block.count == 1);
    assert(profile->xs[4]->block.time == 40);
    assert(profile->xs[4]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
        { .name = "d", .loc = { .source = "foo.c", .line = 4, .col = 40}}
    };
    FbleNameV blocks = { .size = 5, .xs = names };
    FbleDumpProfile(stdout, &blocks, profile);

    FbleFreeProfile(arena, profile);
    FbleFreeCallGraph(arena, graph);
    FbleAssertEmptyArena(arena);
    FbleDeleteArena(arena);
  }

  return 0;
}


