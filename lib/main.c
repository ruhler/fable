/**
 * @file main.c
 *  Implementation of fble main helper functions.
 */

#include <fble/fble-main.h>

#include <string.h>                 // for strcmp

#include <fble/fble-arg-parse.h>    // for FbleNewMainArg, etc.
#include <fble/fble-link.h>         // for FbleLink
#include <fble/fble-version.h>      // for FblePrintVersion
#include <fble/fble-vector.h>       // for FbleInitVector, etc.

// See documentation in fble-main.h
FbleMainStatus FbleMain(
    FbleArgParser arg_parser,
    void* data,
    const char* tool,
    const unsigned char* usage,
    int argc,
    const char** argv,
    FbleNativeModule* preloaded,
    FbleValueHeap* heap,
    FbleProfile* profile,
    FILE** profile_output_file,
    FbleValue** result)
{
  const char* arg0 = argv[0];

  FbleModuleArg module_arg = FbleNewModuleArg();
  const char* profile_file = NULL;
  const char* deps_file = NULL;
  const char* deps_target = NULL;
  bool end_of_options = false;
  bool help = false;
  bool error = false;
  bool version = false;

  // If the module is precompiled and '--' isn't present, skip to end of
  // options right away. That way preloaded programs can go straight to
  // application args if they want.
  if (preloaded != NULL) {
    end_of_options = true;
    for (int i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "--") == 0) {
        end_of_options = false;
        break;
      }
    }
  }

  argc--;
  argv++;
  while (!(help || error || version) && !end_of_options && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (!preloaded && FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (arg_parser && arg_parser(data, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--deps-file", &deps_file, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--deps-target", &deps_target, &argc, &argv, &error)) continue;

    end_of_options = true;
    if (strcmp(argv[0], "--") == 0) {
      argc--;
      argv++;
    }
  }

  if (version) {
    FblePrintCompiledHeaderLine(stdout, tool, arg0, preloaded);
    FblePrintVersion(stdout, tool);
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_SUCCESS;
  }

  if (help) {
    FblePrintCompiledHeaderLine(stdout, tool, arg0, preloaded);
    fprintf(stdout, "%s", usage);
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_USAGE_ERROR;
  }

  if (!preloaded && module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_USAGE_ERROR;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFreeModuleArg(module_arg);
      return FBLE_MAIN_OTHER_ERROR;
    }
  }

  if (deps_file != NULL && deps_target == NULL) {
    fprintf(stderr, "--deps-file requires --deps-target.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_USAGE_ERROR;
  }

  if (deps_target != NULL && deps_file == NULL) {
    fprintf(stderr, "--deps-target requires --deps-file.\n");
    fprintf(stderr, "Try --help for usage\n");
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_USAGE_ERROR;
  }

  FbleNativeModuleV native_search_path = { .xs = NULL, .size = 0 };
  if (preloaded != NULL) {
    native_search_path.xs = &preloaded;
    native_search_path.size = 1;
  }

  if (module_arg.module_path == NULL) {
    module_arg.module_path = FbleCopyModulePath(preloaded->path);
  }

  if (fprofile != NULL) {
    profile->enabled = true;
  }

  FbleStringV deps;
  FbleInitVector(deps);

  FbleValue* linked = FbleLink(heap, profile, native_search_path, module_arg.search_path, module_arg.module_path, &deps);
  FbleFreeModuleArg(module_arg);

  if (linked == NULL) {
    for (size_t i = 0; i < deps.size; ++i) {
      FbleFreeString(deps.xs[i]);
    }
    FbleFreeVector(deps);
    return FBLE_MAIN_COMPILE_ERROR;
  }

  if (deps_file != NULL) {
    FILE* depsfile = fopen(deps_file, "w");
    if (depsfile == NULL) {
      fprintf(stderr, "unable to open %s for writing\n", deps_file);
      for (size_t i = 0; i < deps.size; ++i) {
        FbleFreeString(deps.xs[i]);
      }
      FbleFreeVector(deps);
      return FBLE_MAIN_OTHER_ERROR;
    }
    FbleSaveBuildDeps(depsfile, deps_target, deps);
    fclose(depsfile);
  }

  for (size_t i = 0; i < deps.size; ++i) {
    FbleFreeString(deps.xs[i]);
  }
  FbleFreeVector(deps);

  FbleValue* func = FbleEval(heap, linked, profile);
  if (func == NULL) {
    return FBLE_MAIN_RUNTIME_ERROR;
  }

  *profile_output_file = fprofile;
  *result = func;
  return FBLE_MAIN_SUCCESS;
}

// See documentation in fble-main.h
void FblePrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FbleNativeModule* module)
{
  if (module != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    fprintf(stream, "%s: %s -m ", binary_name, tool);
    FblePrintModulePath(stream, module->path);
    fprintf(stream, " (compiled)\n");
  }
}
