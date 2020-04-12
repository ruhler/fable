// fble-profile-test.c --
//   This file implements the main entry point for the fble-profile-test
//   program.

#include "fble-profile.h"

static bool sTestsFailed = false;

#define ASSERT(p) { \
  if (!(p)) { \
    Fail(__FILE__, __LINE__, #p); \
  } \
}

// Fail --
//   Report a test failure.
static void Fail(const char* file, int line, const char* msg)
{
  fprintf(stderr, "%s:%i: assert failure: %s\n", file, line, msg);
  sTestsFailed = true;
}

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
  FbleProfile* profile = FbleNewProfile(arena, 2);
  FbleProfileThread* thread = FbleNewProfileThread(arena, NULL, profile);
  FbleProfileEnterBlock(arena, thread, 1);
  FbleProfileSample(arena, thread, 10);

  for (size_t i = 0; i < n; ++i) {
    FbleProfileAutoExitBlock(arena, thread);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
  }
  FbleProfileExitBlock(arena, thread);
  FbleFreeProfileThread(arena, thread);

  ASSERT(profile->size == 2);
  ASSERT(profile->xs[0]->block.id == 0);
  ASSERT(profile->xs[0]->block.count == 1);
  ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 10 * (n + 1));
  ASSERT(profile->xs[0]->callees.size == 1);
  ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
  ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
  ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 10 * (n + 1));

  ASSERT(profile->xs[1]->block.id == 1);
  ASSERT(profile->xs[1]->block.count == n + 1);
  ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 10 * (n + 1));
  ASSERT(profile->xs[1]->callees.size == 1);
  ASSERT(profile->xs[1]->callees.xs[0]->id == 1);
  ASSERT(profile->xs[1]->callees.xs[0]->count == n);
  ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 10 * n);

  FbleFreeProfile(arena, profile);
  FbleAssertEmptyArena(arena);
  size_t memory = FbleArenaMaxSize(arena);
  FbleFreeArena(arena);
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
    // Test a simple call profile:
    // 0 -> 1 -> 2 -> 3
    //             -> 4
    //        -> 3
    FbleProfile* profile = FbleNewProfile(arena, 5);
    FbleProfileThread* thread = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 30);
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileEnterBlock(arena, thread, 4);
    FbleProfileSample(arena, thread, 40);
    FbleProfileExitBlock(arena, thread); // 4
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 31);
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    ASSERT(profile->size == 5);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 1);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 131);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 131);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 1);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 131);
    ASSERT(profile->xs[1]->callees.size == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);
    ASSERT(profile->xs[1]->callees.xs[1]->id == 3);
    ASSERT(profile->xs[1]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 31);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 1);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 90);
    ASSERT(profile->xs[2]->callees.size == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->id == 3);
    ASSERT(profile->xs[2]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 30);
    ASSERT(profile->xs[2]->callees.xs[1]->id == 4);
    ASSERT(profile->xs[2]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[2]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 40);

    ASSERT(profile->xs[3]->block.id == 3);
    ASSERT(profile->xs[3]->block.count == 2);
    ASSERT(profile->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 61);
    ASSERT(profile->xs[3]->callees.size == 0);

    ASSERT(profile->xs[4]->block.id == 4);
    ASSERT(profile->xs[4]->block.count == 1);
    ASSERT(profile->xs[4]->block.time[FBLE_PROFILE_TIME_CLOCK] == 40);
    ASSERT(profile->xs[4]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
        { .name = "d", .loc = { .source = "foo.c", .line = 4, .col = 40}}
    };
    FbleNameV blocks = { .size = 5, .xs = names };
    FbleProfileReport(stdout, &blocks, profile);
    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a profile with tail calls
    // 0 -> 1 -> 2 => 3 -> 4
    //                  => 5
    //        -> 6
    FbleAssertEmptyArena(arena);
    FbleProfile* profile = FbleNewProfile(arena, 7);
    FbleProfileThread* thread = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileAutoExitBlock(arena, thread);  // 2
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 30);
    FbleProfileEnterBlock(arena, thread, 4);
    FbleProfileSample(arena, thread, 40);
    FbleProfileExitBlock(arena, thread); // 4
    FbleProfileAutoExitBlock(arena, thread);  // 3
    FbleProfileEnterBlock(arena, thread, 5);
    FbleProfileSample(arena, thread, 50);
    FbleProfileExitBlock(arena, thread); // 5
    FbleProfileEnterBlock(arena, thread, 6);
    FbleProfileSample(arena, thread, 60);
    FbleProfileExitBlock(arena, thread); // 6
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    ASSERT(profile->size == 7);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 1);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 210);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 210);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 1);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 210);
    ASSERT(profile->xs[1]->callees.size == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 140);
    ASSERT(profile->xs[1]->callees.xs[1]->id == 6);
    ASSERT(profile->xs[1]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 60);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 1);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 140);
    ASSERT(profile->xs[2]->callees.size == 1);
    ASSERT(profile->xs[2]->callees.xs[0]->id == 3);
    ASSERT(profile->xs[2]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 120);

    ASSERT(profile->xs[3]->block.id == 3);
    ASSERT(profile->xs[3]->block.count == 1);
    ASSERT(profile->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 120);
    ASSERT(profile->xs[3]->callees.size == 2);
    ASSERT(profile->xs[3]->callees.xs[0]->id == 4);
    ASSERT(profile->xs[3]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[3]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 40);
    ASSERT(profile->xs[3]->callees.xs[1]->id == 5);
    ASSERT(profile->xs[3]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[3]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 50);

    ASSERT(profile->xs[4]->block.id == 4);
    ASSERT(profile->xs[4]->block.count == 1);
    ASSERT(profile->xs[4]->block.time[FBLE_PROFILE_TIME_CLOCK] == 40);
    ASSERT(profile->xs[4]->callees.size == 0);

    ASSERT(profile->xs[5]->block.id == 5);
    ASSERT(profile->xs[5]->block.count == 1);
    ASSERT(profile->xs[5]->block.time[FBLE_PROFILE_TIME_CLOCK] == 50);
    ASSERT(profile->xs[5]->callees.size == 0);

    ASSERT(profile->xs[6]->block.id == 6);
    ASSERT(profile->xs[6]->block.count == 1);
    ASSERT(profile->xs[6]->block.time[FBLE_PROFILE_TIME_CLOCK] == 60);
    ASSERT(profile->xs[6]->callees.size == 0);

    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a profile with self recursion
    // 0 -> 1 -> 2 -> 2 -> 2 -> 3
    FbleAssertEmptyArena(arena);
    FbleProfile* profile = FbleNewProfile(arena, 4);
    FbleProfileThread* thread = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 30);
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    ASSERT(profile->size == 4);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 1);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 100);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 100);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 1);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 100);
    ASSERT(profile->xs[1]->callees.size == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 3);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 90);
    ASSERT(profile->xs[2]->callees.size == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->count == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 70);
    ASSERT(profile->xs[2]->callees.xs[1]->id == 3);
    ASSERT(profile->xs[2]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[2]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 30);

    ASSERT(profile->xs[3]->block.id == 3);
    ASSERT(profile->xs[3]->block.count == 1);
    ASSERT(profile->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 30);
    ASSERT(profile->xs[3]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
    };
    FbleNameV blocks = { .size = 4, .xs = names };
    FbleProfileReport(stdout, &blocks, profile);
    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a profile with self recursion and tail calls
    // 0 -> 1 => 2 => 2 => 2 => 3
    FbleAssertEmptyArena(arena);
    FbleProfile* profile = FbleNewProfile(arena, 4);
    FbleProfileThread* thread = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
    FbleProfileAutoExitBlock(arena, thread);  // 1
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileAutoExitBlock(arena, thread); // 2
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileAutoExitBlock(arena, thread); // 2
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileAutoExitBlock(arena, thread); // 2
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 30);
    FbleProfileExitBlock(arena, thread); // 3
    FbleFreeProfileThread(arena, thread);

    ASSERT(profile->size == 4);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 1);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 100);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 100);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 1);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 100);
    ASSERT(profile->xs[1]->callees.size == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 3);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 90);
    ASSERT(profile->xs[2]->callees.size == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->count == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 70);
    ASSERT(profile->xs[2]->callees.xs[1]->id == 3);
    ASSERT(profile->xs[2]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[2]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 30);

    ASSERT(profile->xs[3]->block.id == 3);
    ASSERT(profile->xs[3]->block.count == 1);
    ASSERT(profile->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 30);
    ASSERT(profile->xs[3]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
    };
    FbleNameV blocks = { .size = 4, .xs = names };
    FbleProfileReport(stdout, &blocks, profile);
    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test a profile with mutual recursion
    // 0 -> 1 -> 2 -> 3 -> 2 -> 3 -> 4
    FbleAssertEmptyArena(arena);
    FbleProfile* profile = FbleNewProfile(arena, 5);
    FbleProfileThread* thread = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 30);
    FbleProfileEnterBlock(arena, thread, 2);
    FbleProfileSample(arena, thread, 20);
    FbleProfileEnterBlock(arena, thread, 3);
    FbleProfileSample(arena, thread, 30);
    FbleProfileEnterBlock(arena, thread, 4);
    FbleProfileSample(arena, thread, 40);
    FbleProfileExitBlock(arena, thread); // 4
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 3
    FbleProfileExitBlock(arena, thread); // 2
    FbleProfileExitBlock(arena, thread); // 1
    FbleFreeProfileThread(arena, thread);

    ASSERT(profile->size == 5);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 1);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 150);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 150);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 1);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 150);
    ASSERT(profile->xs[1]->callees.size == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 140);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 2);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 140);
    ASSERT(profile->xs[2]->callees.size == 1);
    ASSERT(profile->xs[2]->callees.xs[0]->id == 3);
    ASSERT(profile->xs[2]->callees.xs[0]->count == 2);
    ASSERT(profile->xs[2]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 120);

    ASSERT(profile->xs[3]->block.id == 3);
    ASSERT(profile->xs[3]->block.count == 2);
    ASSERT(profile->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 120);
    ASSERT(profile->xs[3]->callees.size == 2);
    ASSERT(profile->xs[3]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[3]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[3]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 90);
    ASSERT(profile->xs[3]->callees.xs[1]->id == 4);
    ASSERT(profile->xs[3]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[3]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 40);

    ASSERT(profile->xs[4]->block.id == 4);
    ASSERT(profile->xs[4]->block.count == 1);
    ASSERT(profile->xs[4]->block.time[FBLE_PROFILE_TIME_CLOCK] == 40);
    ASSERT(profile->xs[4]->callees.size == 0);

    FbleName names[] = {
        { .name = ".", .loc = { .source = "foo.c", .line = 0, .col = 0}},
        { .name = "a", .loc = { .source = "foo.c", .line = 1, .col = 10}},
        { .name = "b", .loc = { .source = "foo.c", .line = 2, .col = 20}},
        { .name = "c", .loc = { .source = "foo.c", .line = 3, .col = 30}},
        { .name = "d", .loc = { .source = "foo.c", .line = 4, .col = 40}},
    };
    FbleNameV blocks = { .size = 5, .xs = names };
    FbleProfileReport(stdout, &blocks, profile);
    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test that tail calls have O(1) memory.
    size_t mem_100 = AutoExitMaxMem(100);
    size_t mem_200 = AutoExitMaxMem(200);
    ASSERT(mem_100 == mem_200);
  }

  {
    // Test multithreaded profiling.
    // a: 0 -> 1 -> 2
    // b: 0 -> 1 -> 2
    FbleAssertEmptyArena(arena);
    FbleProfile* profile = FbleNewProfile(arena, 3);
    FbleProfileThread* a = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileThread* b = FbleNewProfileThread(arena, NULL, profile);

    FbleProfileEnterBlock(arena, a, 1);
    FbleProfileSample(arena, a, 1);
    FbleProfileEnterBlock(arena, a, 2);
    FbleProfileSample(arena, a, 2);

    // We had a bug in the past where this sample wouldn't count everything
    // because it thought it was nested under the sample from thread a.
    FbleProfileEnterBlock(arena, b, 1);
    FbleProfileSample(arena, b, 10);
    FbleProfileEnterBlock(arena, b, 2);
    FbleProfileSample(arena, b, 20);

    FbleProfileExitBlock(arena, a); // 2
    FbleProfileExitBlock(arena, a); // 1
    FbleFreeProfileThread(arena, a);

    FbleProfileExitBlock(arena, b); // 2
    FbleProfileExitBlock(arena, b); // 1
    FbleFreeProfileThread(arena, b);

    ASSERT(profile->size == 3);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 2);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 33);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 2);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 33);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 2);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 33);
    ASSERT(profile->xs[1]->callees.size == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 22);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 2);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 22);
    ASSERT(profile->xs[2]->callees.size == 0);

    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  {
    // Test forking of threads.
    // parent: 0 -> 1 -> 2
    // child:       \--> 3
    FbleAssertEmptyArena(arena);
    FbleProfile* profile = FbleNewProfile(arena, 4);

    FbleProfileThread* parent = FbleNewProfileThread(arena, NULL, profile);
    FbleProfileEnterBlock(arena, parent, 1);
    FbleProfileSample(arena, parent, 1);

    FbleProfileThread* child = FbleNewProfileThread(arena, parent, profile);

    FbleProfileEnterBlock(arena, parent, 2);
    FbleProfileSample(arena, parent, 2);

    FbleProfileEnterBlock(arena, child, 3);
    FbleProfileSample(arena, child, 30);

    FbleProfileExitBlock(arena, parent); // 2
    FbleProfileExitBlock(arena, parent); // 1
    FbleFreeProfileThread(arena, parent);

    FbleProfileExitBlock(arena, child); // 3
    FbleFreeProfileThread(arena, child);

    ASSERT(profile->size == 4);
    ASSERT(profile->xs[0]->block.id == 0);
    ASSERT(profile->xs[0]->block.count == 1);
    ASSERT(profile->xs[0]->block.time[FBLE_PROFILE_TIME_CLOCK] == 33);
    ASSERT(profile->xs[0]->callees.size == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[0]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 33);

    ASSERT(profile->xs[1]->block.id == 1);
    ASSERT(profile->xs[1]->block.count == 1);
    ASSERT(profile->xs[1]->block.time[FBLE_PROFILE_TIME_CLOCK] == 33);
    ASSERT(profile->xs[1]->callees.size == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[0]->time[FBLE_PROFILE_TIME_CLOCK] == 2);
    ASSERT(profile->xs[1]->callees.xs[1]->id == 3);
    ASSERT(profile->xs[1]->callees.xs[1]->count == 1);
    ASSERT(profile->xs[1]->callees.xs[1]->time[FBLE_PROFILE_TIME_CLOCK] == 30);

    ASSERT(profile->xs[2]->block.id == 2);
    ASSERT(profile->xs[2]->block.count == 1);
    ASSERT(profile->xs[2]->block.time[FBLE_PROFILE_TIME_CLOCK] == 2);
    ASSERT(profile->xs[2]->callees.size == 0);

    ASSERT(profile->xs[3]->block.id == 3);
    ASSERT(profile->xs[3]->block.count == 1);
    ASSERT(profile->xs[3]->block.time[FBLE_PROFILE_TIME_CLOCK] == 30);
    ASSERT(profile->xs[3]->callees.size == 0);

    FbleFreeProfile(arena, profile);
    FbleAssertEmptyArena(arena);
  }

  FbleFreeArena(arena);
  return sTestsFailed ? 1 : 0;
}
