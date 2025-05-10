/**
 * @file fble-perf-profile.c
 *  A program that displays a linux perf tool profile in fble profile format.
 */

#include <assert.h>     // for assert
#include <ctype.h>      // for isspace

#include <fble/fble-alloc.h>     // for FbleArrayAlloc, etc.
#include <fble/fble-arg-parse.h> // for FbleParseBoolArg
#include <fble/fble-profile.h>   // for FbleProfile, etc.
#include <fble/fble-version.h>   // for FBLE_VERSION, FbleBuildStamp

#include "fble-perf-profile.usage.h"   // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static FbleBlockId GetBlockId(FbleProfile* profile, char* name);
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
  FbleBlockId id = FbleLookupProfileBlockId(profile, name);
  if (id == 0) {
    FbleName n = {
      .name = FbleNewString(name),
      .space = FBLE_NORMAL_NAME_SPACE,
      .loc = FbleNewLoc("???", 0, 0),
    };
    id = FbleAddBlockToProfile(profile, n);
  }
  return id;
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
    fprintf(stdout, "%s", fbldUsageHelpText);
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage\n");
    return EX_USAGE;
  }

  // Blocks stores the current sample stack, from inner most call to outer
  // most call.
  size_t blocks_size = 0;
  size_t blocks_capacity = 8;
  FbleBlockId* blocks = FbleAllocArray(FbleBlockId, blocks_capacity);

  size_t name_size = 0;
  size_t name_capacity = 8;
  char* name = FbleAllocArray(char, name_capacity);

  FbleProfile* profile = FbleNewProfile(true);
  FbleProfileThread* thread = FbleNewProfileThread(profile);

  FILE* fin = stdin;

  int c;
  while ((c = fgetc(fin)) != EOF) {
    switch (c) {
      case '\n': {
        // Record the current sample.
        for (size_t i = 0; i < blocks_size; ++i) {
          FbleBlockId block = blocks[blocks_size - i - 1];
          FbleProfileEnterBlock(thread, block);
        }
        FbleProfileSample(thread, 1);
        for (size_t i = 0; i < blocks_size; ++i) {
          FbleProfileExitBlock(thread);
        }
        blocks_size = 0;
        break;
      }

      case '\t': {
        // Push the next block into the stack.
        // The line is of the form:
        // 	     be9c _dl_relocate_object+0x6cc (/usr/lib/aarch64-linux-gnu/ld-2.31.so)
        // But may also be something like:
        //       ffffffea1bb64d10 [unknown] ([unknown])

        // Skip past initial space, the addess, and subsequent space.
        while (c != EOF && isspace(c)) c = fgetc(fin);
        while (c != EOF && !isspace(c)) c = fgetc(fin);
        while (c != EOF && isspace(c)) c = fgetc(fin);

        // Parse the name.
        name_size = 0;
        while (c != EOF && !isspace(c) && c != '+') {
          if (name_size + 1 > name_capacity) {
            name_capacity *= 2;
            name = FbleReAllocArray(char, name, name_capacity);
          }
          name[name_size++] = c;
          c = fgetc(fin);
        }
        name[name_size] = '\0';

        // Save the block on the stack.
        if (blocks_size == blocks_capacity) {
          blocks_capacity *= 2;
          blocks = FbleReAllocArray(FbleBlockId, blocks, blocks_capacity);
        }
        blocks[blocks_size++] = GetBlockId(profile, name);

        // Skip past the end of the line.
        while (c != EOF && c != '\n') c = fgetc(fin);
      }

      default: {
        // Skip past the end of the line.
        while (c != EOF && c != '\n') c = fgetc(fin);
      }
    }
  }

  FbleFreeProfileThread(thread);
  FbleFree(name);
  FbleFree(blocks);

  FbleGenerateProfileReport(stdout, profile);
  FbleFreeProfile(profile);
  return EX_SUCCESS;
}
