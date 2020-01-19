// fble-profile-test.c --
//   This file implements the main entry point for the fble-profile-test
//   program.

#include <assert.h>   // for assert

#include "fble-profile.h"

// AutoExitMaxMem --
//   Returns the maximum memory required for an n deep auto exit self
//   recursive call. For the purposes of testing that tail calls can be done
//   using O(1) memory.
//
// Inputs:
//   n - the depth of recursion
//
// Results:
//   The max number of bytes allocated during the recursion.
//
// Side effects:
//   None.
static size_t AutoExitMaxMem(size_t n)
{
  // 0 -> 1 -> 1 -> ... -> 1
  FbleArena* arena = FbleNewArena();
  FbleCallGraph* graph = FbleNewCallGraph(arena, 2);
  FbleProfileThread* thread = FbleNewProfileThread(arena, graph);
  FbleProfileEnterBlock(arena, thread, 1);
  FbleProfileTime(arena, thread, 10);

  for (size_t i = 0; i < n; ++i) {
    FbleProfileAutoExitBlock(arena, thread);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileTime(arena, thread, 10);
  }
  FbleProfileExitBlock(arena, thread);
  FbleFreeProfileThread(arena, thread);

  assert(graph->size == 2);
  assert(graph->xs[0]->block.id == 0);
  assert(graph->xs[0]->block.count == 1);
  assert(graph->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 10 * (n + 1));
  assert(graph->xs[0]->callees.size == 1);
  assert(graph->xs[0]->callees.xs[0]->id == 1);
  assert(graph->xs[0]->callees.xs[0]->count == 1);
  assert(graph->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 10 * (n + 1));

  assert(graph->xs[1]->block.id == 1);
  assert(graph->xs[1]->block.count == n + 1);
  assert(graph->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 10 * (n + 1));
  assert(graph->xs[1]->callees.size == 1);
  assert(graph->xs[1]->callees.xs[0]->id == 1);
  assert(graph->xs[1]->callees.xs[0]->count == n);
  assert(graph->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 10 * n);

  FbleFreeCallGraph(arena, graph);
  FbleAssertEmptyArena(arena);
  size_t memory = FbleArenaMaxSize(arena);
  FbleDeleteArena(arena);
  return memory;
}

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
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileTime(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileTime(arena, thread, 30);
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileEnterBlock(arena, thread, 4);
    FbleProfileTime(arena, thread, 40);
    FbleProfileExitBlock(arena, thread); // 4
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileTime(arena, thread, 31);
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    assert(graph->size == 5);
    assert(graph->xs[0]->block.id == 0);
    assert(graph->xs[0]->block.count == 1);
    assert(graph->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 131);
    assert(graph->xs[0]->callees.size == 1);
    assert(graph->xs[0]->callees.xs[0]->id == 1);
    assert(graph->xs[0]->callees.xs[0]->count == 1);
    assert(graph->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 131);

    assert(graph->xs[1]->block.id == 1);
    assert(graph->xs[1]->block.count == 1);
    assert(graph->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 131);
    assert(graph->xs[1]->callees.size == 2);
    assert(graph->xs[1]->callees.xs[0]->id == 2);
    assert(graph->xs[1]->callees.xs[0]->count == 1);
    assert(graph->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);
    assert(graph->xs[1]->callees.xs[1]->id == 3);
    assert(graph->xs[1]->callees.xs[1]->count == 1);
    assert(graph->xs[1]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 31);

    assert(graph->xs[2]->block.id == 2);
    assert(graph->xs[2]->block.count == 1);
    assert(graph->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 90);
    assert(graph->xs[2]->callees.size == 2);
    assert(graph->xs[2]->callees.xs[0]->id == 3);
    assert(graph->xs[2]->callees.xs[0]->count == 1);
    assert(graph->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 30);
    assert(graph->xs[2]->callees.xs[1]->id == 4);
    assert(graph->xs[2]->callees.xs[1]->count == 1);
    assert(graph->xs[2]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 40);

    assert(graph->xs[3]->block.id == 3);
    assert(graph->xs[3]->block.count == 2);
    assert(graph->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 61);
    assert(graph->xs[3]->callees.size == 0);

    assert(graph->xs[4]->block.id == 4);
    assert(graph->xs[4]->block.count == 1);
    assert(graph->xs[4]->block.time[FBLE_PROFILE_TIME_CLOCK] == 40);
    assert(graph->xs[4]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
        { .name = "d", .loc = { .source = "foo.c", .line = 4, .col = 40}}
    };
    FbleNameV blocks = { .size = 5, .xs = names };
    FbleProfileReport(stdout, &blocks, graph);
    FbleFreeCallGraph(arena, graph);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a graph with tail calls
    // 0 -> 1 -> 2 => 3 -> 4
    //                  => 5
    //        -> 6
    FbleAssertEmptyArena(arena);
    FbleCallGraph* graph = FbleNewCallGraph(arena, 7);
    FbleProfileThread* thread = FbleNewProfileThread(arena, graph);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileTime(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileAutoExitBlock(arena, thread);  // 2
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileTime(arena, thread, 30);
    FbleProfileEnterBlock(arena, thread, 4);
    FbleProfileTime(arena, thread, 40);
    FbleProfileExitBlock(arena, thread); // 4
    FbleProfileAutoExitBlock(arena, thread);  // 3
    FbleProfileEnterBlock(arena, thread, 5);
    FbleProfileTime(arena, thread, 50);
    FbleProfileExitBlock(arena, thread); // 5
    FbleProfileEnterBlock(arena, thread, 6);
    FbleProfileTime(arena, thread, 60);
    FbleProfileExitBlock(arena, thread); // 6
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    assert(graph->size == 7);
    assert(graph->xs[0]->block.id == 0);
    assert(graph->xs[0]->block.count == 1);
    assert(graph->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 210);
    assert(graph->xs[0]->callees.size == 1);
    assert(graph->xs[0]->callees.xs[0]->id == 1);
    assert(graph->xs[0]->callees.xs[0]->count == 1);
    assert(graph->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 210);

    assert(graph->xs[1]->block.id == 1);
    assert(graph->xs[1]->block.count == 1);
    assert(graph->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 210);
    assert(graph->xs[1]->callees.size == 2);
    assert(graph->xs[1]->callees.xs[0]->id == 2);
    assert(graph->xs[1]->callees.xs[0]->count == 1);
    assert(graph->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 140);
    assert(graph->xs[1]->callees.xs[1]->id == 6);
    assert(graph->xs[1]->callees.xs[1]->count == 1);
    assert(graph->xs[1]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 60);

    assert(graph->xs[2]->block.id == 2);
    assert(graph->xs[2]->block.count == 1);
    assert(graph->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 140);
    assert(graph->xs[2]->callees.size == 1);
    assert(graph->xs[2]->callees.xs[0]->id == 3);
    assert(graph->xs[2]->callees.xs[0]->count == 1);
    assert(graph->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 120);

    assert(graph->xs[3]->block.id == 3);
    assert(graph->xs[3]->block.count == 1);
    assert(graph->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 120);
    assert(graph->xs[3]->callees.size == 2);
    assert(graph->xs[3]->callees.xs[0]->id == 4);
    assert(graph->xs[3]->callees.xs[0]->count == 1);
    assert(graph->xs[3]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 40);
    assert(graph->xs[3]->callees.xs[1]->id == 5);
    assert(graph->xs[3]->callees.xs[1]->count == 1);
    assert(graph->xs[3]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 50);

    assert(graph->xs[4]->block.id == 4);
    assert(graph->xs[4]->block.count == 1);
    assert(graph->xs[4]->block.time[FBLE_PROFILE_TIME_CLOCK] == 40);
    assert(graph->xs[4]->callees.size == 0);

    assert(graph->xs[5]->block.id == 5);
    assert(graph->xs[5]->block.count == 1);
    assert(graph->xs[5]->block.time[FBLE_PROFILE_TIME_CLOCK] == 50);
    assert(graph->xs[5]->callees.size == 0);

    assert(graph->xs[6]->block.id == 6);
    assert(graph->xs[6]->block.count == 1);
    assert(graph->xs[6]->block.time[FBLE_PROFILE_TIME_CLOCK] == 60);
    assert(graph->xs[6]->callees.size == 0);

    FbleFreeCallGraph(arena, graph);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a graph with self recursion
    // 0 -> 1 -> 2 -> 2 -> 2 -> 3
    FbleAssertEmptyArena(arena);
    FbleCallGraph* graph = FbleNewCallGraph(arena, 4);
    FbleProfileThread* thread = FbleNewProfileThread(arena, graph);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileTime(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileTime(arena, thread, 30);
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    assert(graph->size == 4);
    assert(graph->xs[0]->block.id == 0);
    assert(graph->xs[0]->block.count == 1);
    assert(graph->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 100);
    assert(graph->xs[0]->callees.size == 1);
    assert(graph->xs[0]->callees.xs[0]->id == 1);
    assert(graph->xs[0]->callees.xs[0]->count == 1);
    assert(graph->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 100);

    assert(graph->xs[1]->block.id == 1);
    assert(graph->xs[1]->block.count == 1);
    assert(graph->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 100);
    assert(graph->xs[1]->callees.size == 1);
    assert(graph->xs[1]->callees.xs[0]->id == 2);
    assert(graph->xs[1]->callees.xs[0]->count == 1);
    assert(graph->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);

    assert(graph->xs[2]->block.id == 2);
    assert(graph->xs[2]->block.count == 3);
    assert(graph->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 90);
    assert(graph->xs[2]->callees.size == 2);
    assert(graph->xs[2]->callees.xs[0]->id == 2);
    assert(graph->xs[2]->callees.xs[0]->count == 2);
    assert(graph->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 70);
    assert(graph->xs[2]->callees.xs[1]->id == 3);
    assert(graph->xs[2]->callees.xs[1]->count == 1);
    assert(graph->xs[2]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 30);

    assert(graph->xs[3]->block.id == 3);
    assert(graph->xs[3]->block.count == 1);
    assert(graph->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 30);
    assert(graph->xs[3]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
    };
    FbleNameV blocks = { .size = 4, .xs = names };
    FbleProfileReport(stdout, &blocks, graph);
    FbleFreeCallGraph(arena, graph);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a graph with mutual recursion
    // 0 -> 1 -> 2 -> 3 -> 2 -> 3 -> 4
    FbleAssertEmptyArena(arena);
    FbleCallGraph* graph = FbleNewCallGraph(arena, 5);
    FbleProfileThread* thread = FbleNewProfileThread(arena, graph);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileTime(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileTime(arena, thread, 30);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileTime(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileTime(arena, thread, 30);
    FbleProfileEnterBlock(arena, thread, 4);
    FbleProfileTime(arena, thread, 40);
    FbleProfileExitBlock(arena, thread); // 4
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    assert(graph->size == 5);
    assert(graph->xs[0]->block.id == 0);
    assert(graph->xs[0]->block.count == 1);
    assert(graph->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 150);
    assert(graph->xs[0]->callees.size == 1);
    assert(graph->xs[0]->callees.xs[0]->id == 1);
    assert(graph->xs[0]->callees.xs[0]->count == 1);
    assert(graph->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 150);

    assert(graph->xs[1]->block.id == 1);
    assert(graph->xs[1]->block.count == 1);
    assert(graph->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 150);
    assert(graph->xs[1]->callees.size == 1);
    assert(graph->xs[1]->callees.xs[0]->id == 2);
    assert(graph->xs[1]->callees.xs[0]->count == 1);
    assert(graph->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 140);

    assert(graph->xs[2]->block.id == 2);
    assert(graph->xs[2]->block.count == 2);
    assert(graph->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 140);
    assert(graph->xs[2]->callees.size == 1);
    assert(graph->xs[2]->callees.xs[0]->id == 3);
    assert(graph->xs[2]->callees.xs[0]->count == 2);
    assert(graph->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 120);

    assert(graph->xs[3]->block.id == 3);
    assert(graph->xs[3]->block.count == 2);
    assert(graph->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 120);
    assert(graph->xs[3]->callees.size == 2);
    assert(graph->xs[3]->callees.xs[0]->id == 2);
    assert(graph->xs[3]->callees.xs[0]->count == 1);
    assert(graph->xs[3]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);
    assert(graph->xs[3]->callees.xs[1]->id == 4);
    assert(graph->xs[3]->callees.xs[1]->count == 1);
    assert(graph->xs[3]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 40);

    assert(graph->xs[4]->block.id == 4);
    assert(graph->xs[4]->block.count == 1);
    assert(graph->xs[4]->block.time[FBLE_PROFILE_TIME_CLOCK] == 40);
    assert(graph->xs[4]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
        { .name = "d", .loc = { .source = "foo.c", .line = 4, .col = 40}},
    };
    FbleNameV blocks = { .size = 5, .xs = names };
    FbleProfileReport(stdout, &blocks, graph);
    FbleFreeCallGraph(arena, graph);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test that tail calls have O(1) memory.
    size_t mem_100 = AutoExitMaxMem(100);
    size_t mem_200 = AutoExitMaxMem(200);
    assert(mem_100 == mem_200);
  }

  FbleDeleteArena(arena);
  return 0;
}

