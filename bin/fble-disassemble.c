/**
 * @file fble-disassemble.c
 *  This file implements the main entry point for the fble-disassemble program.
 */

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>     // for FbleFree.
#include <fble/fble-arg-parse.h> // for FbleParseBoolArg, etc.
#include <fble/fble-compile.h>   // for FbleCompile, FbleDisassemble.
#include <fble/fble-load.h>      // for FbleLoadForModuleCompilation.
#include <fble/fble-profile.h>   // for FbleNewProfile, etc.
#include <fble/fble-version.h>   // for FBLE_VERSION

#include "fble-disassemble.usage.h"   // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

/**
 * @func[main] The main entry point for the fble-disassemble program.
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
    if (FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    FblePrintVersion(stdout, "fble-disassemble");
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

  if (module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  FbleProgram* prgm = FbleLoadForModuleCompilation(module_arg.search_path, module_arg.module_path, NULL);
  FbleFreeModuleArg(module_arg);
  if (prgm == NULL) {
    return EX_FAIL;
  }

  if (!FbleCompileModule(prgm)) {
    FbleFreeProgram(prgm);
    return EX_FAIL;
  }

  FbleModule* compiled = prgm->modules.xs[prgm->modules.size - 1];
  FbleDisassemble(stdout, compiled);

  FbleFreeProgram(prgm);
  return EX_SUCCESS;
}
