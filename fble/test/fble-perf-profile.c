// fble-perf-profile.c --
//   A program that displays a linux perf tool profile in fble profile format.

#define _GNU_SOURCE     // for getline
#include <assert.h>     // for assert
#include <stdio.h>      // for getline
#include <stdlib.h>     // for free
#include <string.h>     // for strcmp, strchr

#include "fble-profile.h"

static FbleBlockId GetBlockId(FbleProfile* profile, char* name);
static void Sample(FbleProfile* profile, FbleProfileThread* thread, int count, char* s);
static void PrintUsage(FILE* stream);
int main(int argc, char* argv[]);


// GetBlockId --
//   Get or create a block id for the given name.
//
// Inputs:
//   profile - the profile.
//   name - the name of the block.
//
// Returns:
//   The block id to use for the block.
//
// Side effects:
//   Allocates a new block id if there isn't already a block with the given
//   name.
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
  return FbleProfileAddBlock(profile, n);
}

// Sample --
//   Process a profiling sample.
//
// Inputs:
//   profile - the profile
//   thread - the profiling thread
//   count - the count corresponding to this sample
//   s - the sample path, of the form "foo;bar;sludge\n"
//
// Side effects:
// * May modify the contents of s by replacing ';' with nuls.
// * Adds the sample to the profile.
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

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Result:
//   None.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream)
{
  fprintf(stream,
      "Usage: fble-perf-profile\n"
      "  Produce an fble profile report for a linux perf profile passed on stdin.\n"
      "\n"
      "To collect perf data: $ perf record -F 997 -d -g <cmd>\n"
      "To report perf data:  $ perf report -q -g folded,count,0 | fble-perf-profile\n"
      "\n"
      "The output 'count' field is mostly meaningless. It is the number of\n"
      "different sample traces the function appeared in.\n"
  );
}

// main --
//   The main entry point for the fble-perf-profile program.
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
  argc--;
  argv++;
  if (argc > 0 && strcmp("--help", *argv) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  char* line = NULL;
  size_t len = 0;
  ssize_t nread;

  // Skip the first line of the form:
  //   100.00%     0.00%  <cmd>  <exe>        [.] _start
  getline(&line, &len, stdin);

  FbleProfile* profile = FbleNewProfile();
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
      return 1;
    }

    Sample(profile, thread, count, space+1);
	}
	free(line);

  FbleFreeProfileThread(thread);
  FbleProfileReport(stdout, profile);
  FbleFreeProfile(profile);
  return 0;
}
