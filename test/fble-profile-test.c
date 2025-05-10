/**
 * @file fble-profile-test.c
 *  A program that runs unit tests for the FbleProfile APIs.
 */

#include <stdarg.h>   // for va_list, va_start, va_end
#include <string.h>   // for strcpy
#include <stdlib.h>   // for rand

#include <fble/fble-alloc.h>     // for FbleResetMaxTotalBytesAllocated, etc.
#include <fble/fble-profile.h>
#include <fble/fble-vector.h>    // for FbleInitVector, etc.


static bool sTestsFailed = false;

static void Fail(const char* file, int line, const char* msg);
static FbleName Name(const char* name);

static void CountQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time);
static void AssertCount(FbleProfile* profile, size_t count);

typedef struct {
  FbleBlockIdV seq;
  uint64_t count;
  uint64_t time;
  bool found;
} SeqData;

static void SeqQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time);
static void AssertSeq(FbleProfile* profile, uint64_t count, uint64_t time, ...);

static void ReplaceN(size_t n);

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
  fprintf(stdout, "%s:%i: assert failure: %s\n", file, line, msg);
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
 * @func[CountQuery] FbleProfileQuery for the CountSeqs function.
 *  See documentation of FbleProfileQuery in fble-profile.h.
 */
static void CountQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time)
{
  size_t* ptr = (size_t*)userdata;
  (*ptr)++;
}

/**
 * @func[AssertCount] Check the number of unique sequences in a profile.
 *  @arg[FbleProfile*][profile] The profile to check.
 *  @arg[size_t][count] The expected number of unique sequences.
 *  @sideeffects Fails with an assertion if the count doesn't match.
 */
static void AssertCount(FbleProfile* profile, size_t count)
{
  size_t actual = 0;
  FbleQueryProfile(profile, &CountQuery, &actual);
  ASSERT(count == actual);
}

/**
 * @func[SeqQuery] FbleProfileQuery for the AssertSeq function.
 *  See documentation of FbleProfileQuery in fble-profile.h.
 */
static void SeqQuery(FbleProfile* profile, void* userdata, FbleBlockIdV seq, uint64_t count, uint64_t time)
{
  SeqData* data = (SeqData*)userdata;
  if (data->seq.size != seq.size) {
    return;
  }

  for (size_t i = 0; i < seq.size; ++i) {
    if (data->seq.xs[i] != seq.xs[i]) {
      return;
    }
  }

  ASSERT(count == data->count);
  ASSERT(time == data->time);
  ASSERT(!data->found);
  data->found = true;
}

/**
 * @func[AssertSeq] Asserts the values of a sequence.
 *  @arg[FbleProfile*][profile] The profile to check.
 *  @arg[uint64_t][count] The expected count.
 *  @arg[uint64_t][time] The expected time.
 *  @arg[...][] The block ids of the sequence, terminated with -1.
 *  @sideeffects None.
 */
static void AssertSeq(FbleProfile* profile, uint64_t count, uint64_t time, ...)
{
  SeqData data;
  FbleInitVector(data.seq);
  data.count = count;
  data.time = time;
  data.found = false;

  va_list ap;
  va_start(ap, time);
  while (true) {
    int i = va_arg(ap, int);
    if (i < 0) {
      break;
    }
    FbleAppendToVector(data.seq, i);
  }

  FbleQueryProfile(profile, &SeqQuery, &data);
  ASSERT(data.found);
  FbleFreeVector(data.seq);
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

  fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
  FbleGenerateProfileReport(stdout, profile);

  AssertSeq(profile, 1, 0, 0, -1);
  AssertSeq(profile, n + 1, 10 * (n + 1), 0, 1, -1);
  AssertCount(profile, 2);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 1, 20, 0, 1, 2, -1);
    AssertSeq(profile, 1, 30, 0, 1, 2, 3, -1);
    AssertSeq(profile, 1, 40, 0, 1, 2, 4, -1);
    AssertSeq(profile, 1, 31, 0, 1, 3, -1);
    AssertCount(profile, 6);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 1, 20, 0, 1, 2, -1);
    AssertSeq(profile, 1, 30, 0, 1, 2, 3, -1);
    AssertSeq(profile, 1, 40, 0, 1, 2, 3, 4, -1);
    AssertSeq(profile, 1, 50, 0, 1, 2, 3, 5, -1);
    AssertSeq(profile, 1, 60, 0, 1, 6, -1);
    AssertCount(profile, 7);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 3, 60, 0, 1, 2, -1);
    AssertSeq(profile, 1, 30, 0, 1, 2, 3, -1);
    AssertCount(profile, 4);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 3, 60, 0, 1, 2, -1);
    AssertSeq(profile, 1, 30, 0, 1, 2, 3, -1);
    AssertCount(profile, 4);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 1, 20, 0, 1, 2, -1);
    AssertSeq(profile, 2, 60, 0, 1, 2, 3, -1);
    AssertSeq(profile, 1, 20, 0, 1, 2, 3, 2, -1);
    AssertSeq(profile, 1, 40, 0, 1, 2, 3, 4, -1);
    AssertCount(profile, 6);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 2, 0, 0, -1);
    AssertSeq(profile, 2, 11, 0, 1, -1);
    AssertSeq(profile, 2, 22, 0, 1, 2, -1);
    AssertCount(profile, 3);

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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 1, 10, 0, 1, -1);
    AssertSeq(profile, 1, 20, 0, 1, 2, -1);
    AssertSeq(profile, 1, 30, 0, 1, 2, 3, -1);
    AssertSeq(profile, 1, 40, 0, 1, 2, 4, -1);
    AssertSeq(profile, 1, 31, 0, 1, 3, -1);
    AssertCount(profile, 6);

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
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

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertCount(profile, 0);

    FbleFreeProfile(profile);
  }

  {
    // Test to exercise node sorted insertion.
    // <root> -> 1 -> 4, 3, 5, 2, 6, 1
    //             -> 1, 2, 3, 5, 6
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

    FbleBlockId blocks[] = {4, 3, 5, 2, 6, 1};
    for (size_t i = 0; i < 6; ++i) {
      FbleProfileEnterBlock(thread, blocks[i]);
      FbleProfileSample(thread, blocks[i] * 10);
      FbleProfileExitBlock(thread);
    }

    for (FbleBlockId i = 1; i <= 6; ++i) {
      FbleProfileEnterBlock(thread, i);
      FbleProfileSample(thread, i * 100);
      FbleProfileExitBlock(thread);
    }

    FbleProfileExitBlock(thread); // 1
    FbleFreeProfileThread(thread);

    fprintf(stdout, "%s:%i:\n", __FILE__, __LINE__);
    FbleGenerateProfileReport(stdout, profile);

    AssertSeq(profile, 1, 0, 0, -1);
    AssertSeq(profile, 3, 120, 0, 1, -1);
    AssertSeq(profile, 2, 220, 0, 1, 2, -1);
    AssertSeq(profile, 2, 330, 0, 1, 3, -1);
    AssertSeq(profile, 2, 440, 0, 1, 4, -1);
    AssertSeq(profile, 2, 550, 0, 1, 5, -1);
    AssertSeq(profile, 2, 660, 0, 1, 6, -1);
    AssertCount(profile, 7);

    FbleFreeProfile(profile);
  }

  return sTestsFailed ? 1 : 0;
}
