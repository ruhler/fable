/**
 * @file fble-perf-profile.c
 *  A program that displays a linux perf tool profile in fble profile format.
 */

#define _GNU_SOURCE     // for getline
#include <assert.h>     // for assert
#include <stdio.h>      // for getline
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp, strchr

#include <fble/fble-arg-parse.h> // for FbleParseBoolArg
#include <fble/fble-profile.h>   // for FbleProfile, etc.
#include <fble/fble-usage.h>     // for FblePrintUsageDoc
#include <fble/fble-version.h>   // for FBLE_VERSION, FbleBuildStamp

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static FbleBlockId GetBlockId(FbleProfile* profile, char* name);
static void Sample(FbleProfile* profile, FbleProfileThread* thread, int count, char* s);
int main(int argc, const char* argv[]);

/**
 * @func[GetBlockId] Get or create a block id for the given name.
 *  @arg[FbleProfile*][profile] The profile.
 *  @arg[char*][name] The name of the block.
 *
 *  @returns[FbleBlockId] The block id to use for the block.
 *
 *  @sideeffects
 *   Allocates a new block id if there isn't already a block with the given
 *   name.
 */
static FbleBlockId GetBlockId(FbleProfile* profile, char* name)
{
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    if (strcmp(name, profile->blocks.xs[i]->name.name->str) == 0) {
      return i;
    }
  }

  FbleName n = {
    .name = FbleNewString(name),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = FbleNewLoc("???", 0, 0),
  };
  return FbleAddBlockToProfile(profile, n);
}

/**
 * @func[Sample] Process a profiling sample.
 *  @arg[FbleProfile*][profile] The profile.
 *  @arg[FbleProfileThread*][thread] The profiling thread.
 *  @arg[int][count] The count corresponding to this sample.
 *  @arg[char*][s] The sample path, of the form "foo;bar;sludge\n".
 *
 *  @sideeffects
 *   @i May modify the contents of s by replacing ';' with nuls.
 *   @i Adds the sample to the profile.
 */
static void Sample(FbleProfile* profile, FbleProfileThread* thread, int count, char* s)
{
  if (*s == '\0') {
    // We've reached the end of the sample.
    FbleProfileSample(thread, count);
    return;
  }

  char* end = strchr(s, ';');
  if (end == NULL) {
    end = strchr(s, '\n');
    assert(end != NULL);
  }
  *end = '\0';

  FbleBlockId block = GetBlockId(profile, s);
  FbleProfileEnterBlock(thread, block);
  Sample(profile, thread, count, end + 1);
  FbleProfileExitBlock(thread);
}

/**
 * @func[main] The main entry point for the fble-perf-profile program.
 *  @arg[int][argc] The number of command line arguments.
 *  @arg[const char**][argv] The command line arguments.
 *
 *  @returns[int] 0 on success, non-zero on error.
 *
 *  @sideeffects
 *   Prints an error to stderr and exits the program in the case of error.
 */
int main(int argc, const char* argv[])
{
  bool version = false;
  bool help = false;
  bool error = false;

  const char* arg0 = argv[0];

  argc--;
  argv++;
  while (!(help || version || error) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
  }

  if (version) {
    FblePrintVersion(stdout, "fble-perf-profile");
    return EX_SUCCESS;
  }

  if (help) {
    FblePrintUsageDoc(arg0, "fble-perf-profile.usage.txt");
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage\n");
    return EX_USAGE;
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t nread;

  // Skip the first line of the form:
  //   100.00%     0.00%  <cmd>  <exe>        [.] _start
  nread = getline(&line, &len, stdin);
  assert(nread != -1 && "Unexpected error reading first line");

  FbleProfile* profile = FbleNewProfile(true);
  FbleProfileThread* thread = FbleNewProfileThread(profile);
	while ((nread = getline(&line, &len, stdin)) != -1) {
    // We're done once we reach the next line of the form:
    //    100.00%     0.00%  <cmd>  <ex>      [.] __libc_start_main
    char* space = strchr(line, ' '); 
    if (space == line || space == NULL) {
      break;
    }

    // We expect a line of the form:
    // 3694 foo;bar;sludge\n
    int count;
    if (sscanf(line, "%i", &count) != 1) {
      fprintf(stderr, "Unable to parse profile report.\n");
      return EX_FAIL;
    }

    Sample(profile, thread, count, space+1);
	}
	free(line);

  FbleFreeProfileThread(thread);
  FbleGenerateProfileReport(stdout, profile);
  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
