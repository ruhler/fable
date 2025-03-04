/**
 * @file fble-deps.c
 *  The implementation of the fble-deps program, which generates gcc -MD
 *  compatible dependencies for an .fble file.
 */

#include <assert.h>   // for assert
#include <string.h>   // for strcmp, strlen
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>     // for FbleFree.
#include <fble/fble-arg-parse.h> // for FbleParseBoolArg, FbleParseStringArg
#include <fble/fble-load.h>      // for FbleLoadForExecution.
#include <fble/fble-vector.h>    // for FbleInitVector.
#include <fble/fble-version.h>   // for FBLE_VERSION

#include "fble-deps.usage.h"     // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

int main(int argc, const char* argv[]);

/**
 * @func[main] The main entry point for the fble-deps program.
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
  FbleModuleArg module_arg = FbleNewModuleArg();
  const char* target = NULL;
  bool version = false;
  bool help = false;
  bool error = false;
  bool compile = false;

  argc--;
  argv++;
  while (!(help || version || error) && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-t", &target, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--target", &target, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-c", &compile, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--compile", &compile, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    FblePrintVersion(stdout, "fble-deps");
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    fprintf(stdout, "%s", fbldUsageHelpText);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (target == NULL) {
    fprintf(stderr, "missing required --target option.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  FblePreloadedModuleV builtins = { .size = 0, .xs = NULL };
  FbleStringV deps;
  FbleInitVector(deps);
  FbleProgram* prgm = NULL;
  if (compile) {
    prgm = FbleLoadForModuleCompilation(module_arg.search_path, module_arg.module_path, &deps);
  } else {
    prgm = FbleLoadForExecution(builtins, module_arg.search_path, module_arg.module_path, &deps);
  }
  FbleFreeModuleArg(module_arg);
  FbleFreeProgram(prgm);

  FbleSaveBuildDeps(stdout, target, deps);

  for (size_t i = 0; i < deps.size; ++i) {
    FbleFreeString(deps.xs[i]);
  }
  FbleFreeVector(deps);
  return EX_SUCCESS;
}
