/**
 * @file fble-profile-test.c
 *  A program that runs unit tests for the FbleProfile APIs.
 */

#include <string.h>   // for strcpy
#include <stdlib.h>   // for rand

#include <fble/fble-alloc.h>     // for FbleResetMaxTotalBytesAllocated, etc.
#include <fble/fble-profile.h>


static bool sTestsFailed = false;

/**
 * @func[ASSERT] Test assertion function.
 *  @arg[bool][p] Property to assert to be true.
 *
 *  @sideeffects
 *   Reports a test failure if @a[p] is not true.
 */
#define ASSERT(p) { \
  if (!(p)) { \
    Fail(__FILE__, __LINE__, #p); \
  } \
}

/**
 * @func[Fail] Reports a test failure.
 *  @arg[const char*][file] The source code file.
 *  @arg[int][line] The line number of the failure.
 *  @arg[const char*][msg] The failure message.
 *  @sideeffects
 *   Reports and records the test failure.
 */
static void Fail(const char* file, int line, const char* msg)
{
  fprintf(stderr, "%s:%i: assert failure: %s\n", file, line, msg);
  sTestsFailed = true;
}

/**
 * @func[Name] Creates a name to use in FbleAddBlockToProfile.
 *  @arg[const char*][name]
 *   The name to use for the block. Typically a string literal.
 *
 *  @returns[FbleName]
 *   An FbleName that can be used in FbleAddBlockToProfile.
 *
 *  @sideeffects
 *   Allocates memory for the name that we expect to be freed by
 *   FbleFreeProfile.
 */
static FbleName Name(const char* name)
{
  FbleName nm = {
    .name = FbleNewString(name),
    .loc = { .source = FbleNewString(__FILE__), .line = rand(), .col = rand() }
  };
  return nm;
}

/**
 * @func[ReplaceN] Performs an N deep replace self recursive call.
 *  For the purposes of testing that tail calls can be done using O(1) memory.
 *
 *  @arg[size_t][n] The depth of recursion
 *
 *  @sideeffects
 *   Allocates memory that impacts the result of FbleMaxTotalBytesAllocated.
 */
static void ReplaceN(size_t n)
{
  // <root> -> 1 -> 1 -> ... -> 1
  FbleProfile* profile = FbleNewProfile(true);
  FbleAddBlockToProfile(profile, Name("_1")); 

  FbleProfileThread* thread = FbleNewProfileThread(profile);
  FbleProfileEnterBlock(thread, 1);
  FbleProfileSample(thread, 10);

  for (size_t i = 0; i < n; ++i) {
    FbleProfileReplaceBlock(thread, 1);
    FbleProfileSample(thread, 10);
  }
  FbleProfileExitBlock(thread);
  FbleFreeProfileThread(thread);

  //ASSERT(profile->blocks.size == 2);
  //ASSERT(profile->blocks.xs[0]->self == 0);
  //ASSERT(profile->blocks.xs[0]->block.id == 0);
  //ASSERT(profile->blocks.xs[0]->block.count == 1);
  //ASSERT(profile->blocks.xs[0]->block.time == 10 * (n + 1));
  //ASSERT(profile->blocks.xs[0]->callees.size == 1);
  //ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
  //ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
  //ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 10 * (n + 1));

  //ASSERT(profile->blocks.xs[1]->self == 10 * (n + 1));
  //ASSERT(profile->blocks.xs[1]->block.id == 1);
  //ASSERT(profile->blocks.xs[1]->block.count == n + 1);
  //ASSERT(profile->blocks.xs[1]->block.time == 10 * (n + 1));
  //ASSERT(profile->blocks.xs[1]->callees.size == 1);
  //ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 1);
  //ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == n);
  //ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 10 * n);

  FbleFreeProfile(profile);
}

/**
 * @func[main] The main entry point for the fble-profile-test program.
 *  @arg[int][argc] Unused.
 *  @arg[char**][argv] Unused.
 *
 *  @returns[int]
 *   0 on success, non-zero on error.
 *
 *  @sideeffects
 *   Prints an error to stderr and exits the program in the case of error.
 */
int main(int argc, char* argv[])
{
  (void)argc;
  (void)argv;

  {
    // Test a simple call profile:
    // <root> -> 1 -> 2 -> 3
    //                  -> 4
    //             -> 3
    FbleProfile* profile = FbleNewProfile(true);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleAddBlockToProfile(profile, Name("_3")); 
    FbleAddBlockToProfile(profile, Name("_4")); 

    FbleProfileThread* thread = FbleNewProfileThread(profile);
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 30);
    FbleProfileExitBlock(thread); // 3
    FbleProfileEnterBlock(thread, 4);
    FbleProfileSample(thread, 40);
    FbleProfileExitBlock(thread); // 4
    FbleProfileExitBlock(thread); // 2
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 31);
    FbleProfileExitBlock(thread); // 3
    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 5);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 1);
    // ASSERT(profile->blocks.xs[0]->block.time == 131);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 131);

    // ASSERT(profile->blocks.xs[1]->self == 10);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 1);
    // ASSERT(profile->blocks.xs[1]->block.time == 131);
    // ASSERT(profile->blocks.xs[1]->callees.size == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->id == 3);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->time == 31);

    // ASSERT(profile->blocks.xs[2]->self == 20);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 1);
    // ASSERT(profile->blocks.xs[2]->block.time == 90);
    // ASSERT(profile->blocks.xs[2]->callees.size == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 30);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 4);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 40);

    // ASSERT(profile->blocks.xs[3]->self == 61);
    // ASSERT(profile->blocks.xs[3]->block.id == 3);
    // ASSERT(profile->blocks.xs[3]->block.count == 2);
    // ASSERT(profile->blocks.xs[3]->block.time == 61);
    // ASSERT(profile->blocks.xs[3]->callees.size == 0);

    // ASSERT(profile->blocks.xs[4]->self == 40);
    // ASSERT(profile->blocks.xs[4]->block.id == 4);
    // ASSERT(profile->blocks.xs[4]->block.count == 1);
    // ASSERT(profile->blocks.xs[4]->block.time == 40);
    // ASSERT(profile->blocks.xs[4]->callees.size == 0);

    FbleGenerateProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  {
    // Test a profile with tail calls
    // <root> -> 1 -> 2 => 3 -> 4
    //                       => 5
    //             -> 6
    FbleProfile* profile = FbleNewProfile(true);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleAddBlockToProfile(profile, Name("_3")); 
    FbleAddBlockToProfile(profile, Name("_4")); 
    FbleAddBlockToProfile(profile, Name("_5")); 
    FbleAddBlockToProfile(profile, Name("_6")); 

    FbleProfileThread* thread = FbleNewProfileThread(profile);
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileReplaceBlock(thread, 3); // 2
    FbleProfileSample(thread, 30);
    FbleProfileEnterBlock(thread, 4);
    FbleProfileSample(thread, 40);
    FbleProfileExitBlock(thread); // 4
    FbleProfileReplaceBlock(thread, 5); // 3
    FbleProfileSample(thread, 50);
    FbleProfileExitBlock(thread); // 5
    FbleProfileEnterBlock(thread, 6);
    FbleProfileSample(thread, 60);
    FbleProfileExitBlock(thread); // 6
    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 7);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 1);
    // ASSERT(profile->blocks.xs[0]->block.time == 210);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 210);

    // ASSERT(profile->blocks.xs[1]->self == 10);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 1);
    // ASSERT(profile->blocks.xs[1]->block.time == 210);
    // ASSERT(profile->blocks.xs[1]->callees.size == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 140);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->id == 6);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->time == 60);

    // ASSERT(profile->blocks.xs[2]->self == 20);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 1);
    // ASSERT(profile->blocks.xs[2]->block.time == 140);
    // ASSERT(profile->blocks.xs[2]->callees.size == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 120);

    // ASSERT(profile->blocks.xs[3]->self == 30);
    // ASSERT(profile->blocks.xs[3]->block.id == 3);
    // ASSERT(profile->blocks.xs[3]->block.count == 1);
    // ASSERT(profile->blocks.xs[3]->block.time == 120);
    // ASSERT(profile->blocks.xs[3]->callees.size == 2);
    // ASSERT(profile->blocks.xs[3]->callees.xs[0]->id == 4);
    // ASSERT(profile->blocks.xs[3]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[3]->callees.xs[0]->time == 40);
    // ASSERT(profile->blocks.xs[3]->callees.xs[1]->id == 5);
    // ASSERT(profile->blocks.xs[3]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[3]->callees.xs[1]->time == 50);

    // ASSERT(profile->blocks.xs[4]->self == 40);
    // ASSERT(profile->blocks.xs[4]->block.id == 4);
    // ASSERT(profile->blocks.xs[4]->block.count == 1);
    // ASSERT(profile->blocks.xs[4]->block.time == 40);
    // ASSERT(profile->blocks.xs[4]->callees.size == 0);

    // ASSERT(profile->blocks.xs[5]->self == 50);
    // ASSERT(profile->blocks.xs[5]->block.id == 5);
    // ASSERT(profile->blocks.xs[5]->block.count == 1);
    // ASSERT(profile->blocks.xs[5]->block.time == 50);
    // ASSERT(profile->blocks.xs[5]->callees.size == 0);

    // ASSERT(profile->blocks.xs[6]->self == 60);
    // ASSERT(profile->blocks.xs[6]->block.id == 6);
    // ASSERT(profile->blocks.xs[6]->block.count == 1);
    // ASSERT(profile->blocks.xs[6]->block.time == 60);
    // ASSERT(profile->blocks.xs[6]->callees.size == 0);

    FbleFreeProfile(profile);
  }

  {
    // Test a profile with self recursion
    // <root> -> 1 -> 2 -> 2 -> 2 -> 3
    FbleProfile* profile = FbleNewProfile(true);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleAddBlockToProfile(profile, Name("_3")); 

    FbleProfileThread* thread = FbleNewProfileThread(profile);
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 30);
    FbleProfileExitBlock(thread); // 3
    FbleProfileExitBlock(thread); // 2
    FbleProfileExitBlock(thread); // 2
    FbleProfileExitBlock(thread); // 2
    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 4);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 1);
    // ASSERT(profile->blocks.xs[0]->block.time == 100);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 100);

    // ASSERT(profile->blocks.xs[1]->self == 10);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 1);
    // ASSERT(profile->blocks.xs[1]->block.time == 100);
    // ASSERT(profile->blocks.xs[1]->callees.size == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);

    // ASSERT(profile->blocks.xs[2]->self == 60);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 3);
    // ASSERT(profile->blocks.xs[2]->block.time == 90);
    // ASSERT(profile->blocks.xs[2]->callees.size == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 70);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 3);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 30);

    // ASSERT(profile->blocks.xs[3]->self == 30);
    // ASSERT(profile->blocks.xs[3]->block.id == 3);
    // ASSERT(profile->blocks.xs[3]->block.count == 1);
    // ASSERT(profile->blocks.xs[3]->block.time == 30);
    // ASSERT(profile->blocks.xs[3]->callees.size == 0);

    FbleGenerateProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  {
    // Test a profile with self recursion and tail calls
    // <root> -> 1 => 2 => 2 => 2 => 3
    FbleProfile* profile = FbleNewProfile(true);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleAddBlockToProfile(profile, Name("_3")); 

    FbleProfileThread* thread = FbleNewProfileThread(profile);
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);
    FbleProfileReplaceBlock(thread, 2);  // 1
    FbleProfileSample(thread, 20);
    FbleProfileReplaceBlock(thread, 2); // 2
    FbleProfileSample(thread, 20);
    FbleProfileReplaceBlock(thread, 2); // 2
    FbleProfileSample(thread, 20);
    FbleProfileReplaceBlock(thread, 3); // 2
    FbleProfileSample(thread, 30);
    FbleProfileExitBlock(thread); // 3
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 4);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 1);
    // ASSERT(profile->blocks.xs[0]->block.time == 100);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 100);

    // ASSERT(profile->blocks.xs[1]->self == 10);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 1);
    // ASSERT(profile->blocks.xs[1]->block.time == 100);
    // ASSERT(profile->blocks.xs[1]->callees.size == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);

    // ASSERT(profile->blocks.xs[2]->self == 60);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 3);
    // ASSERT(profile->blocks.xs[2]->block.time == 90);
    // ASSERT(profile->blocks.xs[2]->callees.size == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 70);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 3);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 30);

    // ASSERT(profile->blocks.xs[3]->self == 30);
    // ASSERT(profile->blocks.xs[3]->block.id == 3);
    // ASSERT(profile->blocks.xs[3]->block.count == 1);
    // ASSERT(profile->blocks.xs[3]->block.time == 30);
    // ASSERT(profile->blocks.xs[3]->callees.size == 0);

    FbleGenerateProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  {
    // Test a profile with mutual recursion
    // <root> -> 1 -> 2 -> 3 -> 2 -> 3 -> 4
    FbleProfile* profile = FbleNewProfile(true);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleAddBlockToProfile(profile, Name("_3")); 
    FbleAddBlockToProfile(profile, Name("_4")); 

    FbleProfileThread* thread = FbleNewProfileThread(profile);
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 30);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 30);
    FbleProfileEnterBlock(thread, 4);
    FbleProfileSample(thread, 40);
    FbleProfileExitBlock(thread); // 4
    FbleProfileExitBlock(thread); // 3
    FbleProfileExitBlock(thread); // 2
    FbleProfileExitBlock(thread); // 3
    FbleProfileExitBlock(thread); // 2
    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 5);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 1);
    // ASSERT(profile->blocks.xs[0]->block.time == 150);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 150);

    // ASSERT(profile->blocks.xs[1]->self == 10);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 1);
    // ASSERT(profile->blocks.xs[1]->block.time == 150);
    // ASSERT(profile->blocks.xs[1]->callees.size == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 140);

    // ASSERT(profile->blocks.xs[2]->self == 40);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 2);
    // ASSERT(profile->blocks.xs[2]->block.time == 140);
    // ASSERT(profile->blocks.xs[2]->callees.size == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 120);

    // ASSERT(profile->blocks.xs[3]->self == 60);
    // ASSERT(profile->blocks.xs[3]->block.id == 3);
    // ASSERT(profile->blocks.xs[3]->block.count == 2);
    // ASSERT(profile->blocks.xs[3]->block.time == 120);
    // ASSERT(profile->blocks.xs[3]->callees.size == 2);
    // ASSERT(profile->blocks.xs[3]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[3]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[3]->callees.xs[0]->time == 90);
    // ASSERT(profile->blocks.xs[3]->callees.xs[1]->id == 4);
    // ASSERT(profile->blocks.xs[3]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[3]->callees.xs[1]->time == 40);

    // ASSERT(profile->blocks.xs[4]->self == 40);
    // ASSERT(profile->blocks.xs[4]->block.id == 4);
    // ASSERT(profile->blocks.xs[4]->block.count == 1);
    // ASSERT(profile->blocks.xs[4]->block.time == 40);
    // ASSERT(profile->blocks.xs[4]->callees.size == 0);

    FbleGenerateProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  {
    // Test that tail calls have O(1) memory.
    FbleResetMaxTotalBytesAllocated();
    ReplaceN(1024);
    size_t mem_small = FbleMaxTotalBytesAllocated();

    FbleResetMaxTotalBytesAllocated();
    ReplaceN(4096);
    size_t mem_large = FbleMaxTotalBytesAllocated();
    ASSERT(mem_small <= mem_large + 4);
  }

  {
    // Test multithreaded profiling.
    // a: <root> -> 1 -> 2
    // b: <root> -> 1 -> 2
    FbleProfile* profile = FbleNewProfile(true);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 

    FbleProfileThread* a = FbleNewProfileThread(profile);
    FbleProfileThread* b = FbleNewProfileThread(profile);

    FbleProfileEnterBlock(a, 1);
    FbleProfileSample(a, 1);
    FbleProfileEnterBlock(a, 2);
    FbleProfileSample(a, 2);

    // We had a bug in the past where this sample wouldn't count everything
    // because it thought it was nested under the sample from thread a.
    FbleProfileEnterBlock(b, 1);
    FbleProfileSample(b, 10);
    FbleProfileEnterBlock(b, 2);
    FbleProfileSample(b, 20);

    FbleProfileExitBlock(a); // 2
    FbleProfileExitBlock(a); // 1
    FbleFreeProfileThread(a);

    FbleProfileExitBlock(b); // 2
    FbleProfileExitBlock(b); // 1
    FbleFreeProfileThread(b);

    // ASSERT(profile->blocks.size == 3);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 2);
    // ASSERT(profile->blocks.xs[0]->block.time == 33);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 2);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 33);

    // ASSERT(profile->blocks.xs[1]->self == 11);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 2);
    // ASSERT(profile->blocks.xs[1]->block.time == 33);
    // ASSERT(profile->blocks.xs[1]->callees.size == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 22);

    // ASSERT(profile->blocks.xs[2]->self == 22);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 2);
    // ASSERT(profile->blocks.xs[2]->block.time == 22);
    // ASSERT(profile->blocks.xs[2]->callees.size == 0);

    FbleFreeProfile(profile);
  }

  {
    // Test adding blocks while running.
    // <root> -> 1 -> 2 -> 3
    //                  -> 4
    //             -> 3
    FbleProfile* profile = FbleNewProfile(true);
    FbleProfileThread* thread = FbleNewProfileThread(profile);

    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);

    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);

    FbleAddBlockToProfile(profile, Name("_3")); 
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 30);
    FbleProfileExitBlock(thread); // 3

    FbleAddBlockToProfile(profile, Name("_4")); 
    FbleProfileEnterBlock(thread, 4);
    FbleProfileSample(thread, 40);
    FbleProfileExitBlock(thread); // 4
    FbleProfileExitBlock(thread); // 2
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 31);
    FbleProfileExitBlock(thread); // 3
    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 5);
    // ASSERT(profile->blocks.xs[0]->self == 0);
    // ASSERT(profile->blocks.xs[0]->block.id == 0);
    // ASSERT(profile->blocks.xs[0]->block.count == 1);
    // ASSERT(profile->blocks.xs[0]->block.time == 131);
    // ASSERT(profile->blocks.xs[0]->callees.size == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->id == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[0]->callees.xs[0]->time == 131);

    // ASSERT(profile->blocks.xs[1]->self == 10);
    // ASSERT(profile->blocks.xs[1]->block.id == 1);
    // ASSERT(profile->blocks.xs[1]->block.count == 1);
    // ASSERT(profile->blocks.xs[1]->block.time == 131);
    // ASSERT(profile->blocks.xs[1]->callees.size == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->id == 2);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[0]->time == 90);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->id == 3);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[1]->callees.xs[1]->time == 31);

    // ASSERT(profile->blocks.xs[2]->self == 20);
    // ASSERT(profile->blocks.xs[2]->block.id == 2);
    // ASSERT(profile->blocks.xs[2]->block.count == 1);
    // ASSERT(profile->blocks.xs[2]->block.time == 90);
    // ASSERT(profile->blocks.xs[2]->callees.size == 2);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->id == 3);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[0]->time == 30);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->id == 4);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->count == 1);
    // ASSERT(profile->blocks.xs[2]->callees.xs[1]->time == 40);

    // ASSERT(profile->blocks.xs[3]->self == 61);
    // ASSERT(profile->blocks.xs[3]->block.id == 3);
    // ASSERT(profile->blocks.xs[3]->block.count == 2);
    // ASSERT(profile->blocks.xs[3]->block.time == 61);
    // ASSERT(profile->blocks.xs[3]->callees.size == 0);

    // ASSERT(profile->blocks.xs[4]->self == 40);
    // ASSERT(profile->blocks.xs[4]->block.id == 4);
    // ASSERT(profile->blocks.xs[4]->block.count == 1);
    // ASSERT(profile->blocks.xs[4]->block.time == 40);
    // ASSERT(profile->blocks.xs[4]->callees.size == 0);

    FbleGenerateProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  {
    // Test disabling of profiling.
    // <root> -> 1 -> 2 -> 3
    //                  -> 4
    //             -> 3
    FbleProfile* profile = FbleNewProfile(false);
    FbleAddBlockToProfile(profile, Name("_1")); 
    FbleAddBlockToProfile(profile, Name("_2")); 
    FbleAddBlockToProfile(profile, Name("_3")); 
    FbleAddBlockToProfile(profile, Name("_4")); 

    FbleProfileThread* thread = FbleNewProfileThread(profile);
    FbleProfileEnterBlock(thread, 1);
    FbleProfileSample(thread, 10);
    FbleProfileEnterBlock(thread, 2);
    FbleProfileSample(thread, 20);
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 30);
    FbleProfileExitBlock(thread); // 3
    FbleProfileEnterBlock(thread, 4);
    FbleProfileSample(thread, 40);
    FbleProfileExitBlock(thread); // 4
    FbleProfileExitBlock(thread); // 2
    FbleProfileEnterBlock(thread, 3);
    FbleProfileSample(thread, 31);
    FbleProfileExitBlock(thread); // 3
    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    // ASSERT(profile->blocks.size == 5);
    // for (size_t i = 0; i < profile->blocks.size; ++i) {
      // ASSERT(profile->blocks.xs[i]->self == 0);
      // ASSERT(profile->blocks.xs[i]->block.id == i);
      // ASSERT(profile->blocks.xs[i]->block.count == 0);
      // ASSERT(profile->blocks.xs[i]->block.time == 0);
      // ASSERT(profile->blocks.xs[i]->callees.size == 0);
    // }

    FbleGenerateProfileReport(stdout, profile);
    FbleFreeProfile(profile);
  }

  return sTestsFailed ? 1 : 0;
}
