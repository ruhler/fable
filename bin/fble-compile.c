// fble-native.c --
//   This file implements the main entry point for the fble-native program,
//   which compiles *.fble code to *.c code.

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>         // for FbleFree.
#include <fble/fble-arg-parse.h>     // for FbleParseBoolArg, etc.
#include <fble/fble-compile.h>       // for FbleCompile, etc.
#include <fble/fble-module-path.h>   // for FbleParseModulePath.
#include <fble/fble-vector.h>        // for FbleVectorInit.
#include <fble/fble-version.h>       // for FBLE_VERSION

#include "fble-compile.usage.h"      // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

typedef enum {
  TARGET_AARCH64,
  TARGET_C
} Target;

// main --
//   The main entry point for the fble-compile program.
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
int main(int argc, const char* argv[])
{
  FbleModuleArg module_arg = FbleNewModuleArg();
  bool compile = false;
  const char* export = NULL;
  const char* main_ = NULL;
  const char* target_string = NULL;
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
    if (FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-t", &target_string, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--target", &target_string, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-c", &compile, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--compile", &compile, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("-e", &export, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--export", &export, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--main", &main_, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (version) {
    FblePrintVersion(stdout, "fble-compile");
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    fprintf(stdout, "%s", fbldUsageHelpText);
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stdout, "Try --help for usage.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  Target target;
  if (target_string == NULL) {
    target = TARGET_AARCH64;
  } else if (strcmp(target_string, "aarch64") == 0) {
    target = TARGET_AARCH64;
  } else if (strcmp(target_string, "c") == 0) {
    target = TARGET_C;
  } else {
    fprintf(stderr, "unsupported target '%s'\n", target_string);
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (!compile && export == NULL && main_ == NULL) {
    fprintf(stderr, "one of --export NAME, --compile, or --main NAME must be specified.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }
  
  if (module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (export != NULL) {
    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64Export(stdout, export, module_arg.module_path); break;
      case TARGET_C: FbleGenerateCExport(stdout, export, module_arg.module_path); break;
    }
  }

  if (main_ != NULL) {
    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64Main(stdout, main_, module_arg.module_path); break;
      case TARGET_C: FbleGenerateCMain(stdout, main_, module_arg.module_path); break;
    }
  }

  if (compile) {
    FbleLoadedProgram* prgm = FbleLoad(module_arg.search_path, module_arg.module_path, NULL);
    if (prgm == NULL) {
      FbleFreeModuleArg(module_arg);
      return EX_FAIL;
    }

    FbleCompiledModule* module = FbleCompileModule(prgm);
    FbleFreeLoadedProgram(prgm);

    if (module == NULL) {
      FbleFreeModuleArg(module_arg);
      return EX_FAIL;
    }

    FbleFreeModulePath(module->path);
    module->path = FbleCopyModulePath(module_arg.module_path);

    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64(stdout, module); break;
      case TARGET_C: FbleGenerateC(stdout, module); break;
    }

    FbleFreeCompiledModule(module);
  }

  FbleFreeModuleArg(module_arg);
  return EX_SUCCESS;
}
