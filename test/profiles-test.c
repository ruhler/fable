// profiles-test.c --
//   Implementation of FbleProfilesTestMain function.

#include "profiles-test.h"

#include <assert.h>   // for assert
#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>       // for FbleFree
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-link.h>        // for FbleCompiledModuleFunction.
#include <fble/fble-name.h>        // for FbleName.
#include <fble/fble-profile.h>     // for FbleNewProfile, etc.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleVectorInit.
#include <fble/fble-version.h>     // for FBLE_VERSION, FbleBuildStamp.

#include "profiles-test.usage.h"   // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

static void PrintHeader(FILE* stream, const char* arg0, FbleCompiledModuleFunction* module);
static void PrintVersion(FILE* stream);
static void PrintHelp(FILE* stream);
static FbleBlockProfile* Block(FbleProfile* profile, const char* name);
static size_t Count(FbleProfile* profile, const char* name);
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee);

// PrintHeader 
//   Prints a header line to distinguish among different compiled fble-test
//   binaries when printing version and help text.
//
// @param stream  The output stream to write the header to.
// @param arg0  argv[0] of the main function, used to determine the binary name.
// @param module  The compiled module, or NULL
static void PrintHeader(FILE* stream, const char* arg0, FbleCompiledModuleFunction* module)
{
  if (module != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    // Load the module to figure out the path to it.
    FbleExecutableProgram* program = FbleAlloc(FbleExecutableProgram);
    FbleVectorInit(program->modules);
    module(program);
    FbleExecutableModule* mod = program->modules.xs[program->modules.size-1];

    fprintf(stream, "%s: fble-mem-test -m ", binary_name);
    FblePrintModulePath(stream, mod->path);
    fprintf(stream, " (compiled)\n");

    FbleFreeExecutableProgram(program);
  }
}

// PrintVersion --
//   Prints version info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the version information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs version information to the given stream.
static void PrintVersion(FILE* stream)
{
  fprintf(stream, "fble-profiles-test %s (%s)\n", FBLE_VERSION, FbleBuildStamp);
}

// PrintHelp --
//   Prints help info for FbleTestMain the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintHelp(FILE* stream)
{
  fprintf(stream, "%s", fbldUsageHelpText);
}

// Block --
//   Look up information for the given named block.
//
// Inputs:
//   profile - the profile to get the block info for.
//   name - the name of the block to get.
//
// Results:
//   The block.
//
// Side effects:
//   Asserts with failure if there are no blocks with the given name or there
//   is more than one block with the given name.
static FbleBlockProfile* Block(FbleProfile* profile, const char* name)
{
  FbleBlockProfile* result = NULL;
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    FbleBlockProfile* block = profile->blocks.xs[i];
    if (strcmp(block->name.name->str, name) == 0) {
      assert(result == NULL && "duplicate entries with name");
      result = block;
    }
  }
  assert(result != NULL && "no block found with given name");
  return result;
}

// Count --
//   Return the total number of times the profiling block with the given name
//   was called in the profile.
//
// Inputs:
//   profile - the profile to get the count from.
//   name - the name of the block to get the count for.
//
// Results:
//   The number of times the block was called.
//
// Side effects:
//   Asserts with failure if there are no blocks with the given name or there
//   is more than one block with the given name.
static size_t Count(FbleProfile* profile, const char* name)
{
  return Block(profile,name)->block.count;
}

// Calls --
//   Return the total number of times the callee block called the caller
//   block.
//
// Inputs:
//   profile - the profile to get the count from.
//   caller - the name of the caller block.
//   callee - the name of the callee block.
//
// Results:
//   The number of times the callee directly called the caller.
//
// Side effects:
//   Asserts with failure if there are no blocks matching the names of callee
//   and caller or if there are multiple blocks matching the names.
static size_t Calls(FbleProfile* profile, const char* caller, const char* callee)
{
  FbleBlockProfile* caller_block = Block(profile, caller);
  FbleBlockProfile* callee_block = Block(profile, callee);

  FbleBlockId callee_id = callee_block->block.id;
  for (size_t i = 0; i < caller_block->callees.size; ++i) {
    FbleCallData* call = caller_block->callees.xs[i];
    if (call->id == callee_id) {
      return call->count;
    }
  }
  return 0;
}

// FbleProfilesTestMain -- see documentation in profiles-test.h
int FbleProfilesTestMain(int argc, const char** argv, FbleCompiledModuleFunction* module)
{
  const char* arg0 = argv[0];

  FbleModuleArg module_arg = FbleNewModuleArg();
  bool help = false;
  bool error = false;
  bool version = false;

  argc--;
  argv++;
  while (!(help || error || version) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (!module && FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    PrintHeader(stdout, arg0, module);
    PrintVersion(stdout);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    PrintHeader(stdout, arg0, module);
    PrintHelp(stdout);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    PrintHeader(stderr, arg0, module);
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (!module && module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = FbleLinkFromCompiledOrSource(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  FbleValue* result = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);

  if (result == NULL) {
    FbleFreeProfile(profile);
    return EX_FAIL;
  }

  // Dump the profile to make it easier to develop and debug the tests that
  // follow.
  FbleProfileReport(stdout, profile);

  // Each of these top level let bindings were executed once when the main
  // program ran.
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.Not"));
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.t"));
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.f"));
  assert(1 == Calls(profile, "/ProfilesTest%", "/ProfilesTest%.f2"));

  // The Not function was executed three times, once from each of t, f, and
  // f2.
  assert(1 == Calls(profile, "/ProfilesTest%.t", "/ProfilesTest%.Not!"));
  assert(1 == Calls(profile, "/ProfilesTest%.f", "/ProfilesTest%.Not!"));
  assert(1 == Calls(profile, "/ProfilesTest%.f2", "/ProfilesTest%.Not!"));

  // In total, we created Not once and executed three times. 
  assert(1 == Count(profile, "/ProfilesTest%.Not"));
  assert(3 == Count(profile, "/ProfilesTest%.Not!"));

  // The true branch of Not was executed twice, the false branch once.
  assert(2 == Calls(profile, "/ProfilesTest%.Not!", "/ProfilesTest%.Not!.true"));
  assert(1 == Calls(profile, "/ProfilesTest%.Not!", "/ProfilesTest%.Not!.false"));

  // Regression test for a bug where the location for the top level profile
  // block was a module path instead of a file path.
  {
    FbleBlockProfile* block = Block(profile, "/ProfilesTest%");
    assert(strstr(block->name.loc.source->str, "test/ProfilesTest.fble"));
  }

  FbleFreeProfile(profile);
  return EX_SUCCESS;
}

