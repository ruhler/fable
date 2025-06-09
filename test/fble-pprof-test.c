/**
 * @file fble-pprof-test.c
 *  A program that generates a known profile for testing pprof output
 *  generation.
 */

#include <fble/fble-profile.h>

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
int main(int argc, char* argv[])
{
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

  FbleOutputProfile(stdout, profile);
  FbleFreeProfile(profile);

  return 0;
}
