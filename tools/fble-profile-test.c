// fble-profile-test.c --
//   This file implements the main entry point for the fble-profile-test
//   program.

#include <string.h>   // for strcpy
#include <stdlib.h>   // for rand

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

// FbleName --
//   Create a name to use in FbleProfileAddBlock.
//
// Inputs:
//   arena - The arena to use for allocations.
//   name - The name to use for the block. Typically a string literal.
//
// Results:
//   An FbleName that can be used in FbleProfileAddBlock.
//
// Side effects:
//   Allocates memory for the name that we expect to be freed by
//   FbleFreeProfile.
static FbleName Name(FbleArena* arena, const char* name)
{
  FbleName nm = {
    .name = FbleNewString(arena, name),
    .loc = { .source = FbleNewString(arena, __FILE__), .line = rand(), .col = rand() }
  };
  return nm;
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
  // <root> -> 1 -> 1 -> ... -> 1
  FbleArena* arena = FbleNewArena();
  FbleProfile* profile = FbleNewProfile(arena);
  FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 

  FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
  FbleProfileEnterBlock(arena, thread, 1);
  FbleProfileSample(arena, thread, 10);

  for (size_t i = 0; i < n; ++i) {
    FbleProfileAutoExitBlock(arena, thread);
    FbleProfileEnterBlock(arena, thread, 1);
    FbleProfileSample(arena, thread, 10);
  }
  FbleProfileExitBlock(arena, thread);
  FbleFreeProfileThread(arena, thread);

  ASSERT(profile->blocks.size == 2);
  ASSERT(profile->blocks.xs[0]->block.id == 0);
  ASSERT(profile->blocks.xs[0]->block.count == 1);
  ASSERT(profile->blocks.xs[0]->block.time == 10 * (n + 1));
  ASSERT(profile->blocks.xs[0]->callees.size == 1);
  ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
  ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
  ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 10 * (n + 1));

  ASSERT(profile->blocks.xs[1]->block.id == 1);
  ASSERT(profile->blocks.xs[1]->block.count == n + 1);
  ASSERT(profile->blocks.xs[1]->block.time == 10 * (n + 1));
  ASSERT(profile->blocks.xs[1]->callees.size == 1);
  ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 1);
  ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == n);
  ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 10 * n);

  FbleFreeProfile(arena, profile);
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
  {
    // Test a simple call profile:
    // <root> -> 1 -> 2 -> 3
    //                  -> 4
    //             -> 3
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_3")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_4")); 

    FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
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

    ASSERT(profile->blocks.size == 5);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 1);
    ASSERT(profile->blocks.xs[0]->block.time == 131);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 131);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 1);
    ASSERT(profile->blocks.xs[1]->block.time == 131);
    ASSERT(profile->blocks.xs[1]->callees.size == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->id == 3);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->time == 31);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 1);
    ASSERT(profile->blocks.xs[2]->block.time == 90);
    ASSERT(profile->blocks.xs[2]->callees.size == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 30);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 4);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 40);

    ASSERT(profile->blocks.xs[3]->block.id == 3);
    ASSERT(profile->blocks.xs[3]->block.count == 2);
    ASSERT(profile->blocks.xs[3]->block.time == 61);
    ASSERT(profile->blocks.xs[3]->callees.size == 0);

    ASSERT(profile->blocks.xs[4]->block.id == 4);
    ASSERT(profile->blocks.xs[4]->block.count == 1);
    ASSERT(profile->blocks.xs[4]->block.time == 40);
    ASSERT(profile->blocks.xs[4]->callees.size == 0);

    FbleProfileReport(stdout, profile);
    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  {
    // Test a profile with tail calls
    // <root> -> 1 -> 2 => 3 -> 4
    //                       => 5
    //             -> 6
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_3")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_4")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_5")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_6")); 

    FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
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

    ASSERT(profile->blocks.size == 7);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 1);
    ASSERT(profile->blocks.xs[0]->block.time == 210);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 210);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 1);
    ASSERT(profile->blocks.xs[1]->block.time == 210);
    ASSERT(profile->blocks.xs[1]->callees.size == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 140);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->id == 6);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->time == 60);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 1);
    ASSERT(profile->blocks.xs[2]->block.time == 140);
    ASSERT(profile->blocks.xs[2]->callees.size == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 120);

    ASSERT(profile->blocks.xs[3]->block.id == 3);
    ASSERT(profile->blocks.xs[3]->block.count == 1);
    ASSERT(profile->blocks.xs[3]->block.time == 120);
    ASSERT(profile->blocks.xs[3]->callees.size == 2);
    ASSERT(profile->blocks.xs[3]->callees.xs[0]->id == 4);
    ASSERT(profile->blocks.xs[3]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[3]->callees.xs[0]->time == 40);
    ASSERT(profile->blocks.xs[3]->callees.xs[1]->id == 5);
    ASSERT(profile->blocks.xs[3]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[3]->callees.xs[1]->time == 50);

    ASSERT(profile->blocks.xs[4]->block.id == 4);
    ASSERT(profile->blocks.xs[4]->block.count == 1);
    ASSERT(profile->blocks.xs[4]->block.time == 40);
    ASSERT(profile->blocks.xs[4]->callees.size == 0);

    ASSERT(profile->blocks.xs[5]->block.id == 5);
    ASSERT(profile->blocks.xs[5]->block.count == 1);
    ASSERT(profile->blocks.xs[5]->block.time == 50);
    ASSERT(profile->blocks.xs[5]->callees.size == 0);

    ASSERT(profile->blocks.xs[6]->block.id == 6);
    ASSERT(profile->blocks.xs[6]->block.count == 1);
    ASSERT(profile->blocks.xs[6]->block.time == 60);
    ASSERT(profile->blocks.xs[6]->callees.size == 0);

    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  {
    // Test a profile with self recursion
    // <root> -> 1 -> 2 -> 2 -> 2 -> 3
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_3")); 

    FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
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

    ASSERT(profile->blocks.size == 4);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 1);
    ASSERT(profile->blocks.xs[0]->block.time == 100);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 100);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 1);
    ASSERT(profile->blocks.xs[1]->block.time == 100);
    ASSERT(profile->blocks.xs[1]->callees.size == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 3);
    ASSERT(profile->blocks.xs[2]->block.time == 90);
    ASSERT(profile->blocks.xs[2]->callees.size == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 70);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 3);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 30);

    ASSERT(profile->blocks.xs[3]->block.id == 3);
    ASSERT(profile->blocks.xs[3]->block.count == 1);
    ASSERT(profile->blocks.xs[3]->block.time == 30);
    ASSERT(profile->blocks.xs[3]->callees.size == 0);

    FbleProfileReport(stdout, profile);
    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  {
    // Test a profile with self recursion and tail calls
    // <root> -> 1 => 2 => 2 => 2 => 3
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_3")); 

    FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
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

    ASSERT(profile->blocks.size == 4);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 1);
    ASSERT(profile->blocks.xs[0]->block.time == 100);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 100);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 1);
    ASSERT(profile->blocks.xs[1]->block.time == 100);
    ASSERT(profile->blocks.xs[1]->callees.size == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 3);
    ASSERT(profile->blocks.xs[2]->block.time == 90);
    ASSERT(profile->blocks.xs[2]->callees.size == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 70);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 3);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 30);

    ASSERT(profile->blocks.xs[3]->block.id == 3);
    ASSERT(profile->blocks.xs[3]->block.count == 1);
    ASSERT(profile->blocks.xs[3]->block.time == 30);
    ASSERT(profile->blocks.xs[3]->callees.size == 0);

    FbleProfileReport(stdout, profile);
    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  {
    // Test a profile with mutual recursion
    // <root> -> 1 -> 2 -> 3 -> 2 -> 3 -> 4
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_3")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_4")); 

    FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
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

    ASSERT(profile->blocks.size == 5);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 1);
    ASSERT(profile->blocks.xs[0]->block.time == 150);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 150);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 1);
    ASSERT(profile->blocks.xs[1]->block.time == 150);
    ASSERT(profile->blocks.xs[1]->callees.size == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 140);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 2);
    ASSERT(profile->blocks.xs[2]->block.time == 140);
    ASSERT(profile->blocks.xs[2]->callees.size == 1);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 2);
    ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 120);

    ASSERT(profile->blocks.xs[3]->block.id == 3);
    ASSERT(profile->blocks.xs[3]->block.count == 2);
    ASSERT(profile->blocks.xs[3]->block.time == 120);
    ASSERT(profile->blocks.xs[3]->callees.size == 2);
    ASSERT(profile->blocks.xs[3]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[3]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[3]->callees.xs[0]->time == 90);
    ASSERT(profile->blocks.xs[3]->callees.xs[1]->id == 4);
    ASSERT(profile->blocks.xs[3]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[3]->callees.xs[1]->time == 40);

    ASSERT(profile->blocks.xs[4]->block.id == 4);
    ASSERT(profile->blocks.xs[4]->block.count == 1);
    ASSERT(profile->blocks.xs[4]->block.time == 40);
    ASSERT(profile->blocks.xs[4]->callees.size == 0);

    FbleProfileReport(stdout, profile);
    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  {
    // Test that tail calls have O(1) memory.
    size_t mem_100 = AutoExitMaxMem(100);
    size_t mem_200 = AutoExitMaxMem(200);
    ASSERT(mem_100 == mem_200);
  }

  {
    // Test multithreaded profiling.
    // a: <root> -> 1 -> 2
    // b: <root> -> 1 -> 2
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 

    FbleProfileThread* a = FbleNewProfileThread(arena, profile);
    FbleProfileThread* b = FbleNewProfileThread(arena, profile);

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

    ASSERT(profile->blocks.size == 3);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 2);
    ASSERT(profile->blocks.xs[0]->block.time == 33);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 2);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 33);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 2);
    ASSERT(profile->blocks.xs[1]->block.time == 33);
    ASSERT(profile->blocks.xs[1]->callees.size == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 22);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 2);
    ASSERT(profile->blocks.xs[2]->block.time == 22);
    ASSERT(profile->blocks.xs[2]->callees.size == 0);

    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  {
    // Test forking of threads.
    // parent: <root> -> 1 -> 2
    // child:            \--> 3
    FbleArena* arena = FbleNewArena();
    FbleProfile* profile = FbleNewProfile(arena);
    FbleProfileAddBlock(arena, profile, Name(arena, "_1")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_2")); 
    FbleProfileAddBlock(arena, profile, Name(arena, "_3")); 

    FbleProfileThread* parent = FbleNewProfileThread(arena, profile);
    FbleProfileEnterBlock(arena, parent, 1);
    FbleProfileSample(arena, parent, 1);

    FbleProfileThread* child = FbleForkProfileThread(arena, parent);

    FbleProfileEnterBlock(arena, parent, 2);
    FbleProfileSample(arena, parent, 2);

    FbleProfileEnterBlock(arena, child, 3);
    FbleProfileSample(arena, child, 30);

    FbleProfileExitBlock(arena, parent); // 2
    FbleProfileExitBlock(arena, parent); // 1
    FbleFreeProfileThread(arena, parent);

    FbleProfileExitBlock(arena, child); // 3
    FbleFreeProfileThread(arena, child);

    ASSERT(profile->blocks.size == 4);
    ASSERT(profile->blocks.xs[0]->block.id == 0);
    ASSERT(profile->blocks.xs[0]->block.count == 1);
    ASSERT(profile->blocks.xs[0]->block.time == 33);
    ASSERT(profile->blocks.xs[0]->callees.size == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 33);

    ASSERT(profile->blocks.xs[1]->block.id == 1);
    ASSERT(profile->blocks.xs[1]->block.count == 1);
    ASSERT(profile->blocks.xs[1]->block.time == 33);
    ASSERT(profile->blocks.xs[1]->callees.size == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 2);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->id == 3);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->count == 1);
    ASSERT(profile->blocks.xs[1]->callees.xs[1]->time == 30);

    ASSERT(profile->blocks.xs[2]->block.id == 2);
    ASSERT(profile->blocks.xs[2]->block.count == 1);
    ASSERT(profile->blocks.xs[2]->block.time == 2);
    ASSERT(profile->blocks.xs[2]->callees.size == 0);

    ASSERT(profile->blocks.xs[3]->block.id == 3);
    ASSERT(profile->blocks.xs[3]->block.count == 1);
    ASSERT(profile->blocks.xs[3]->block.time == 30);
    ASSERT(profile->blocks.xs[3]->callees.size == 0);

    FbleFreeProfile(arena, profile);
    FbleFreeArena(arena);
  }

  return sTestsFailed ? 1 : 0;
}
