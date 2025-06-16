/**
 * @file main.c
 *  Implementation of fble main helper functions.
 */

#include <fble/fble-main.h>

#include <string.h>                 // for strcmp

#include <fble/fble-arg-parse.h>    // for FbleNewMainArg, etc.
#include <fble/fble-compile.h>      // for FbleCompileProgram.
#include <fble/fble-link.h>         // for FbleLink
#include <fble/fble-load.h>         // for FbleLoadForExecution
#include <fble/fble-version.h>      // for FblePrintVersion
#include <fble/fble-vector.h>       // for FbleInitVector, etc.

static void PrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FblePreloadedModule* preloaded);
static FbleValue* Link(FbleValueHeap* heap, FbleProfile* profile, FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps);

/**
 * @func[PrintCompiledHeaderLine] Prints an information line about a preloaded module.
 *  This is a convenience function for providing more information to users as
 *  part of a fble compiled main function. It prints a header line if the
 *  preloaded module is not NULL, of the form something like:
 *
 *  @code[txt] @
 *   fble-debug-test: fble-test -m /DebugTest% (compiled)
 *
 *  @arg[FILE*] stream
 *   The output stream to print to.
 *  @arg[const char*] tool
 *   Name of the underlying tool, e.g. "fble-test".
 *  @arg[const char*] arg0
 *   argv[0] from the main function.
 *  @arg[FblePreloadedModule*] preloaded
 *   Optional preloaded module to get the module name from.
 *
 *  @sideeffects
 *   Prints a header line to the given stream.
 */
static void PrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FblePreloadedModule* preloaded)
{
  if (preloaded != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    fprintf(stream, "%s: %s -m ", binary_name, tool);
    FblePrintModulePath(stream, preloaded->path);
    fprintf(stream, " (compiled)\n");
  }
}

/**
 * @func[Link] Load and link an optionally preloaded program.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *  @arg[FblePreloadedModuleV] builtins
 *   The list of builtin modules.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for locating .fble files.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load.
 *  @arg[FbleStringV*] build_deps
 *   Output to store list of files the load depended on. This should be a
 *   preinitialized vector, or NULL.
 *
 *  @returns FbleValue*
 *   A zero-argument fble function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Allocates a value on the heap.
 *   @item
 *    The user should free strings added to build_deps when no longer
 *    needed, including in the case when program loading fails.
 */
static FbleValue* Link(FbleValueHeap* heap, FbleProfile* profile, FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  FbleProgram* program = FbleLoadForExecution(builtins, search_path, module_path, build_deps);
  if (program == NULL) {
    return NULL;
  }

  if (!FbleCompileProgram(program)) {
    FbleFreeProgram(program);
    return NULL;
  }

  FbleValue* linked = FbleLink(heap, profile, program);
  FbleFreeProgram(program);
  return linked;
}

// See documentation in fble-main.h
FbleMainStatus FbleMain(
    FbleArgParser arg_parser,
    void* data,
    const char* tool,
    const unsigned char* usage,
    int* argc,
    const char*** argv,
    FblePreloadedModule* preloaded,
    FblePreloadedModuleV builtins,
    FbleValueHeap* heap,
    FbleProfile* profile,
    const char** profile_output_file,
    FbleValue** result)
{
  const char* arg0 = (*argv)[0];

  FbleModuleArg module_arg = FbleNewModuleArg();
  const char* deps_file = NULL;
  const char* deps_target = NULL;
  bool help = false;
  bool error = false;
  bool version = false;

  (*argc)--;
  (*argv)++;
  while (!(help || error || version) && *argc > 0) {
    if (FbleParseBoolArg("-h", &help, argc, argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, argc, argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, argc, argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, argc, argv, &error)) continue;
    if (!preloaded && FbleParseModuleArg(&module_arg, argc, argv, &error)) continue;
    if (arg_parser && arg_parser(data, argc, argv, &error)) continue;
    if (FbleParseStringArg("--profile", profile_output_file, argc, argv, &error)) continue;
    if (FbleParseStringArg("--deps-file", &deps_file, argc, argv, &error)) continue;
    if (FbleParseStringArg("--deps-target", &deps_target, argc, argv, &error)) continue;
    if (strcmp((*argv)[0], "--") == 0) {
      (*argc)--;
      (*argv)++;
      break;
    }
    if (FbleParseInvalidArg(argc, argv, &error)) continue;
  }

  if (version) {
    PrintCompiledHeaderLine(stdout, tool, arg0, preloaded);
    FblePrintVersion(stdout, tool);
    FbleFreeModuleArg(module_arg);
    return FBLE_MAIN_SUCCESS;
  }

  if (help) {
    PrintCompiledHeaderLine(stdout, tool, arg0, preloaded);
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

  if (module_arg.module_path == NULL) {
    module_arg.module_path = FbleCopyModulePath(preloaded->path);
  }

  profile->enabled = (*profile_output_file != NULL);

  FbleStringV deps;
  FbleInitVector(deps);

  FblePreloadedModuleV preloaded_and_builtins;
  FbleInitVector(preloaded_and_builtins);
  if (preloaded != NULL) {
    FbleAppendToVector(preloaded_and_builtins, preloaded);
  }
  for (size_t i = 0; i < builtins.size; ++i) {
    FbleAppendToVector(preloaded_and_builtins, builtins.xs[i]);
  }

  FbleValue* linked = Link(heap, profile, preloaded_and_builtins, module_arg.search_path, module_arg.module_path, &deps);
  FbleFreeVector(preloaded_and_builtins);
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

  *result = func;
  return FBLE_MAIN_SUCCESS;
}
