/**
 * @file fble-compile.c
 *  This file implements the main entry point for the fble-compile program,
 *  which compiles *.fble code to *.c code.
 */

#include <string.h>   // for strcmp
#include <stdio.h>    // for FILE, fprintf, stderr

#include <fble/fble-alloc.h>         // for FbleFree.
#include <fble/fble-arg-parse.h>     // for FbleParseBoolArg, etc.
#include <fble/fble-generate.h>      // for FbleGenerate*
#include <fble/fble-compile.h>       // for FbleCompile, etc.
#include <fble/fble-module-path.h>   // for FbleParseModulePath.
#include <fble/fble-vector.h>        // for FbleInitVector.
#include <fble/fble-version.h>       // for FblePrintVersion.

#include "fble-compile.usage.h"      // for fbldUsageHelpText

#define EX_SUCCESS 0
#define EX_FAIL 1
#define EX_USAGE 2

typedef enum {
  TARGET_AARCH64,
  TARGET_C
} Target;

/**
 * @func[main] The main entry point for the fble-compile program.
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
  bool compile = false;
  const char* export = NULL;
  const char* main_ = NULL;
  const char* target_string = NULL;
  const char* deps_file = NULL;
  const char* deps_target = NULL;
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
    if (FbleParseStringArg("--deps-file", &deps_file, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--deps-target", &deps_target, &argc, &argv, &error)) continue;
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

  if (deps_file != NULL && !compile) {
    fprintf(stderr, "--deps-file requires --compile.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (deps_file != NULL && deps_target == NULL) {
    fprintf(stderr, "--deps-file requires --deps-target.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (deps_target != NULL && deps_file == NULL) {
    fprintf(stderr, "--deps-target requires --deps-file.\n");
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
    FbleStringV deps;
    FbleInitVector(deps);
    FbleProgram* prgm = FbleLoadForModuleCompilation(module_arg.search_path, module_arg.module_path, &deps);
    if (prgm == NULL) {
      for (size_t i = 0; i < deps.size; ++i) {
        FbleFreeString(deps.xs[i]);
      }
      FbleFreeVector(deps);
      FbleFreeModuleArg(module_arg);
      return EX_FAIL;
    }

    if (deps_file != NULL) {
      FILE* depsfile = fopen(deps_file, "w");
      if (depsfile == NULL) {
        fprintf(stderr, "unable to open %s for writing\n", deps_file);
        FbleFreeProgram(prgm);
        FbleFreeModuleArg(module_arg);
      }
      FbleSaveBuildDeps(depsfile, deps_target, deps);
      fclose(depsfile);
    }

    for (size_t i = 0; i < deps.size; ++i) {
      FbleFreeString(deps.xs[i]);
    }
    FbleFreeVector(deps);

    if (!FbleCompileModule(prgm)) {
      FbleFreeProgram(prgm);
      FbleFreeModuleArg(module_arg);
      return EX_FAIL;
    }

    FbleModule* module = prgm->modules.xs[prgm->modules.size - 1];

    FbleFreeModulePath(module->path);
    module->path = FbleCopyModulePath(module_arg.module_path);

    switch (target) {
      case TARGET_AARCH64: FbleGenerateAArch64(stdout, module); break;
      case TARGET_C: FbleGenerateC(stdout, module); break;
    }

    FbleFreeProgram(prgm);
  }

  FbleFreeModuleArg(module_arg);
  return EX_SUCCESS;
}
