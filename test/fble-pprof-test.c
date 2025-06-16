/**
 * @file fble-pprof-test.c
 *  A program that generates a known profile for testing pprof output
 *  generation.
 */

#include <fble/fble-arg-parse.h>     // for FbleParseBoolArg, etc.
#include <fble/fble-profile.h>       // for FbleProfile, etc.
#include <fble/fble-version.h>       // for FblePrintVersion

/**
 * @func[main] The main entry point for the fble-pprof-test program.
 *  @arg[int][argc] Unused.
 *  @arg[char**][argv] Unused.
 *
 *  @returns[int]
 *   0 on success, non-zero on error.
 *
 *  @sideeffects
 *   Outputs a profile to stdout.
 */
int main(int argc, const char* argv[])
{
  const char* profile_output_file = NULL;
  bool help = false;
  bool error = false;
  bool version = false;

  argc--;
  argv++;
  while (!(help || error || version)  && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-o", &profile_output_file, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_output_file, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    FblePrintVersion(stdout, "fble-pprof-test");
    return 0;
  }

  if (help) {
    fprintf(stdout, "usage: fble-pprof-test --profile FILE.\n");
    return 0;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage.\n");
    return 1;
  }

  if (profile_output_file == NULL) {
    fprintf(stderr, "Missing profile output path. Try --help for usage.\n");
    return 1;
  }

  FbleProfile* profile = FbleNewProfile();

  FbleName a_name = {
    .name = FbleNewString("foo"),
    .loc = { .source = FbleNewString("Foo.fble"), .line = 10, .col = 14 }
  };
  FbleName b_name = {
    .name = FbleNewString("bar"),
    .loc = { .source = FbleNewString("Bar.fble"), .line = 140, .col = 2 }
  };
  FbleName c_name = {
    .name = FbleNewString("sludge"),
    .loc = { .source = FbleNewString("Sludge.fble"), .line = 1400, .col = 3 }
  };

  FbleBlockId a = FbleAddBlockToProfile(profile, a_name);
  FbleBlockId b = FbleAddBlockToProfile(profile, b_name);
  FbleBlockId c = FbleAddBlockToProfile(profile, c_name);

  FbleProfileThread* thread = FbleNewProfileThread(profile);
  FbleProfileEnterBlock(thread, a);
  FbleProfileEnterBlock(thread, b);
  FbleProfileEnterBlock(thread, c);
  FbleProfileSample(thread, 10);
  FbleProfileExitBlock(thread);
  FbleProfileExitBlock(thread);
  FbleProfileEnterBlock(thread, c);
  FbleProfileSample(thread, 20);
  FbleFreeProfileThread(thread);

  FbleOutputProfile(profile_output_file, profile);
  FbleFreeProfile(profile);

  return 0;
}
