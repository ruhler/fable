/**
 * @file load.c
 *  Routines for finding and loading fble modules.
 */

#include <fble/fble-load.h>

#include <assert.h>   // for assert
#include <stdlib.h>   // for getenv
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include <fble/fble-alloc.h>
#include <fble/fble-name.h>
#include <fble/fble-vector.h>

#include "config.h"   // for FBLE_CONFIG_DATADIR
#include "expr.h"

/**
 * The default package path.
 */
const char* FbleDefaultPackagePath = FBLE_CONFIG_DATADIR "/fble";

/**
 * @struct[FbleSearchPath] An fble search path.
 *  @field[size_t][size] The number of elements.
 *  @field[FbleString**][xs] The elements.
 */
struct FbleSearchPath {
  size_t size;
  FbleString** xs;
};

/**
 * @struct[Pending] List of modules being loaded.
 *  For the purposes of detecting recursive module dependencies.
 *
 *  @field[FbleModulePath*][path] The path of a module being loaded.
 *  @field[Pending*][tail] The rest of the list.
 */
typedef struct Pending {
  FbleModulePath* path;
  struct Pending* tail;
} Pending;

static FbleString* FindPackageAt(const char* package, const char* package_dir);
static FbleString* FindAt(const char* root, const char* suffix, FbleModulePath* path, FbleStringV* build_deps);
static FbleString* Find(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleStringV* build_deps);
static bool Read(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleExpr** parsed, FbleModulePathV* deps, FbleStringV* build_deps);
static bool ReadOptional(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleExpr** parsed, FbleModulePathV* deps, FbleStringV* build_deps);
static bool ReadRequired(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleExpr** parsed, FbleModulePathV* deps, FbleStringV* build_deps);

static FbleModule* Preload(FbleModuleV* loaded, FblePreloadedModule* preloaded);
static FbleModule* LookupModule(FbleModuleV* loaded, FbleModulePath* path);
static FbleModule* Load(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModuleV* loaded, Pending* pending, FbleModulePath* module_path, bool for_execution, bool for_main, FbleStringV* build_deps);
static FbleProgram* LoadMain(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, bool for_execution, FbleStringV* build_deps);

// See documentation in fble-load.h.
FbleSearchPath* FbleNewSearchPath()
{
  FbleSearchPath* path = FbleAlloc(FbleSearchPath);
  FbleInitVector(*path);
  return path;
}

// See documentation in fble-load.h.
void FbleFreeSearchPath(FbleSearchPath* path)
{
  for (size_t i = 0; i < path->size; ++i) {
    FbleFreeString(path->xs[i]);
  }
  FbleFreeVector(*path);
  FbleFree(path);
}

// See documentation in fble-load.h.
void FbleAppendToSearchPath(FbleSearchPath* path, const char* root_dir)
{
  FbleAppendToVector(*path, FbleNewString(root_dir));
}

// See documentation in fble-load.h.
void FbleAppendStringToSearchPath(FbleSearchPath* path, FbleString* root_dir)
{
  FbleAppendToVector(*path, FbleCopyString(root_dir));
}

/**
 * @func[FindPackageAt] Finds a package in the given package directory.
 *  Searches for the package in the given directory.
 *
 *  @arg[const char*][package] The name of the package.
 *  @arg[const char*][package_dir] The package directory to search.
 *
 *  @returns[FbleString*]
 *   The module root directory for the package, or NULL in case no such
 *   package is found.
 *
 *  @sideeffects
 *   Allocates an FbleString that should be freed with FbleFreeString when no
 *   longer needed.
 */
static FbleString* FindPackageAt(const char* package, const char* package_dir)
{
  // package plus extra space for '/' and '\0' characters.
  size_t len = 2 + strlen(package);
  size_t n = strlen(package_dir);
  char dir[n + len];
  strcpy(dir, package_dir);
  strcat(dir, "/");
  strcat(dir, package);

  if (access(dir, F_OK) == 0) {
    return FbleNewString(dir);
  }
  return NULL;
}

// See documentation in fble-load.h.
FbleString* FbleFindPackage(const char* package)
{
  // Search FBLE_PACKAGE_PATH.
  char* package_path = getenv("FBLE_PACKAGE_PATH");
  if (package_path != NULL) {
    char* end = package_path + strlen(package_path);
    char* s = package_path;
    do {
      char* e = strchr(s, ':');
      if (e == NULL) {
        e = end;
      }

      size_t n = e - s;
      char dir[n+1];
      strncpy(dir, s, n);
      dir[n] = '\0';
      FbleString* found = FindPackageAt(package, dir);
      if (found) {
        return found;
      }

      s = e+1;
    } while (s < end);
  }

  return FindPackageAt(package, FbleDefaultPackagePath);
}

/**
 * @func[FindAt] Gets the file path for an fble module given the root directory.
 *  Returns the path on disk of the .fble file for the given root directory and
 *  module path.
 *  
 *  @arg[const char*][root] File path to the root of the module search path.
 *  @arg[const char*][suffix]
 *   The suffix of the file to look for, e.g. @l{.fble}.
 *  @arg[FbleModulePath*][path] The module path to find the source file for.
 *  @arg[FbleStringV*][build_deps]
 *   Preinitialized output vector to store list of files searched in. May be
 *   NULL.
 *
 *  @returns[FbleString*]
 *   The file path to the source code of the module, or NULL in case no such
 *   file can be found.
 *
 *  @sideeffects
 *   @item
 *    The user should call FbleFreeString when the returned string is no
 *    longer needed.
 *   @item
 *    Adds strings to build_deps that should be freed with FbleFreeString when
 *    no longer needed.
 */
static FbleString* FindAt(const char* root, const char* suffix, FbleModulePath* path, FbleStringV* build_deps)
{
  assert(root != NULL);

  // Cacluate the length of the filename for the module.
  size_t len = strlen(root) + strlen(suffix) + 1;
  for (size_t i = 0; i < path->path.size; ++i) {
    // "/name"
    len += 1 + strlen(path->path.xs[i].name->str);

    if (strchr(path->path.xs[i].name->str, '/') != NULL) {
      // There's nothing in the fble language spec that says you can't have a
      // forward slash in a module name, but there's no way on a posix system
      // to put the slash in the filename where we would look for the module,
      // so don't even try looking for it.
      return NULL;
    }
  }

  // Construct the path to the module on disk.
  char filename[len];
  filename[0] = '\0';
  strcat(filename, root);
  for (size_t i = 0; i < path->path.size; ++i) {
    strcat(filename, "/");
    strcat(filename, path->path.xs[i].name->str);
  }
  strcat(filename, suffix);

  if (access(filename, F_OK) != 0) {
    // TODO: We should add as much of the directory of 'filename' that exists
    // to build_deps, so build systems will detect the case when filename is added
    // as a need to rebuild.
    return NULL;
  }

  FbleString* found = FbleNewString(filename);
  if (build_deps) {
    FbleAppendToVector(*build_deps, FbleCopyString(found));
  }

  return found;
}

/**
 * @func[Find] Finds the source code for a module.
 *  Returns the path on disk of the .fble file for the given search path and
 *  module path.
 *  
 *  @arg[FbleSearchPath*][search_path] The module search path.
 *  @arg[const char*][suffix]
 *   The suffix of the file to look for, e.g. @l{.fble}.
 *  @arg[FbleModulePath*][path] The module path to find the source file for.
 *  @arg[FbleStringV*][build_deps]
 *   Preinitialized output vector to store list of files searched in. May be
 *   NULL.
 *
 *  @returns[FbleString*]
 *   The file path to the source code of the module, or NULL in case no such
 *   file can be found.
 *
 *  @sideeffects
 *   The user should call FbleFreeString when the returned string is no longer
 *   needed.
 */
static FbleString* Find(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleStringV* build_deps)
{
  FbleString* found = NULL;
  for (size_t i = 0; !found && i < search_path->size; ++i) {
    found = FindAt(search_path->xs[i]->str, suffix, path, build_deps);
  }

  return found;
}

/**
 * @func[Read] Search for and parse the expression for a module.
 *  @arg[FbleSearchPath*][search_path] The module search path.
 *  @arg[const char*][suffix]
 *   The suffix of the file to look for, @l{.fble} or @l{.fble.@}.
 *  @arg[FbleModulePath*][path] The module path to find the source file for.
 *  @arg[FbleExpr**][parsed]
 *   Output set to the parsed value. Set to NULL on failure to parse.
 *  @arg[FbleModulePathV*][deps]
 *   A list of the modules that the parsed expression references. Modules will
 *   appear at most once in the list.
 *  @arg[FbleStringV*][build_deps]
 *   Preinitialized output vector to store list of files searched in. May be
 *   NULL.
 *  @returns[bool] True if the module was found, false otherwise.
 *  @sideeffects
 *   @i Sets parsed to the parsed value if found.
 *   @i Sets parsed to NULL if found but failed to parse.
 *   @i Appends to deps and build_deps as appropriate.
 *   @i Reports an error to stderr on failure to parse.
 *   @i Does not report an error on failure to find.
 *   @item
 *    The user is responsible for freeing the parsed result when no longer
 *    needed
 */
static bool Read(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleExpr** parsed, FbleModulePathV* deps, FbleStringV* build_deps)
{
  FbleString* filename = Find(search_path, suffix, path, build_deps);
  if (filename == NULL) {
    return false;
  }

  *parsed = FbleParse(filename, deps);
  FbleFreeString(filename);
  return true;
}

/**
 * @func[ReadOptional] Try reading an optional file.
 *  @arg[FbleSearchPath*][search_path] The module search path.
 *  @arg[const char*][suffix]
 *   The suffix of the file to look for, @l{.fble} or @l{.fble.@}.
 *  @arg[FbleModulePath*][path] The module path to find the source file for.
 *  @arg[FbleExpr**][parsed]
 *   Output set to the parsed value. Set to NULL on failure to parse.
 *  @arg[FbleModulePathV*][deps]
 *   A list of the modules that the parsed expression references. Modules will
 *   appear at most once in the list.
 *  @arg[FbleStringV*][build_deps]
 *   Preinitialized output vector to store list of files searched in. May be
 *   NULL.
 *  @returns[bool]
 *   False if the module was found and failed to parse. True otherwise.
 *  @sideeffects
 *   @i Sets parsed to the parsed value if found.
 *   @i Sets parsed to NULL if found but failed to parse.
 *   @i Appends to deps and build_deps as appropriate.
 *   @i Reports an error to stderr on failure to parse.
 *   @i Does not report an error on failure to find.
 *   @item
 *    The user is responsible for freeing the parsed result when no longer
 *    needed
 */
static bool ReadOptional(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleExpr** parsed, FbleModulePathV* deps, FbleStringV* build_deps)
{
  return !Read(search_path, suffix, path, parsed, deps, build_deps) || *parsed;
}

/**
 * @func[ReadRequired] Reading a required file.
 *  @arg[FbleSearchPath*][search_path] The module search path.
 *  @arg[const char*][suffix]
 *   The suffix of the file to look for, @l{.fble} or @l{.fble.@}.
 *  @arg[FbleModulePath*][path] The module path to find the source file for.
 *  @arg[FbleExpr**][parsed]
 *   Output set to the parsed value. Set to NULL on failure to parse.
 *  @arg[FbleModulePathV*][deps]
 *   A list of the modules that the parsed expression references. Modules will
 *   appear at most once in the list.
 *  @arg[FbleStringV*][build_deps]
 *   Preinitialized output vector to store list of files searched in. May be
 *   NULL.
 *  @returns[bool]
 *   True if the module was found and parsed successfully. False otherwise.
 *  @sideeffects
 *   @i Sets parsed to the parsed value if found.
 *   @i Sets parsed to NULL if found but failed to parse.
 *   @i Appends to deps and build_deps as appropriate.
 *   @i Reports an error to stderr on failure to find.
 *   @i Reports an error to stderr on failure to parse.
 *   @item
 *    The user is responsible for freeing the parsed result when no longer
 *    needed
 */
static bool ReadRequired(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleExpr** parsed, FbleModulePathV* deps, FbleStringV* build_deps)
{
  if (!Read(search_path, suffix, path, parsed, deps, build_deps)) {
    FbleReportError("module ", path->loc);
    FblePrintModulePath(stderr, path);
    fprintf(stderr, " not found\n");
    return false;
  }
  return *parsed != NULL;
}

/**
 * @func[Preload] Add a preloaded module to a program.
 *  @arg[FbleModuleV*][loaded] List of loaded modules to add the module to.
 *  @arg[FblePreloadedModule*][preloaded] The module to add.
 *  @returns[FbleModule*]
 *   The module that was added to the program.
 *  @sideeffects
 *   @i Recursively adds the module and all its dependencies to the program.
 *   @item
 *    The caller should call FbleFreeModule on the returned module when no
 *    longer needed.
 *
 */
static FbleModule* Preload(FbleModuleV* loaded, FblePreloadedModule* preloaded)
{
  // Check if we've already loaded the module.
  for (size_t i = 0; i < loaded->size; ++i) {
    if (FbleModulePathsEqual(loaded->xs[i]->path, preloaded->path)) {
      return FbleCopyModule(loaded->xs[i]);
    }
  }

  FbleModule* module = FbleAlloc(FbleModule);
  module->magic = FBLE_MODULE_MAGIC;
  module->refcount = 1;
  module->path = FbleCopyModulePath(preloaded->path);
  FbleInitVector(module->type_deps);
  FbleInitVector(module->link_deps);
  for (size_t i = 0; i < preloaded->deps.size; ++i) {
    FbleModule* dep = Preload(loaded, preloaded->deps.xs[i]);
    FbleAppendToVector(module->link_deps, dep);
  }
  module->type = NULL;
  module->value = NULL;
  module->code = NULL;
  module->exe = preloaded->executable;
  FbleInitVector(module->profile_blocks);
  for (size_t i = 0; i < preloaded->profile_blocks.size; ++i) {
    FbleAppendToVector(module->profile_blocks, FbleCopyName(preloaded->profile_blocks.xs[i]));
  }
  FbleAppendToVector(*loaded, module);
  return FbleCopyModule(module);
}

/**
 * @func[LookupModule] Look up to see if we've loaded a module.
 *  @arg[FbleModuleV*][loaded] The list of loaded modules.
 *  @arg[FbleModulePath*][path] The path of the module to find.
 *  @returns[FbleModule] The module, or NULL if not found.
 *  @sideeffects None
 */
static FbleModule* LookupModule(FbleModuleV* loaded, FbleModulePath* path)
{
  for (size_t i = 0; i < loaded->size; ++i) {
    if (FbleModulePathsEqual(path, loaded->xs[i]->path)) {
      return loaded->xs[i];
    }
  }
  return NULL;
}

/**
 * @func[Load] Loads an fble program.
 *  @arg[FblePreloadedModuleV] builtins
 *   List of builtin modules to search.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for location .fble files. Borrowed.
 *  @arg[FbleModuleV*] loaded
 *   The list of already loaded modules.
 *  @arg[Pending*] pending
 *   Modules in the process of being loaded, for detecting module recursion.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load. Borrowed.
 *  @arg[bool] for_execution
 *   If true, load the program for execution. Otherwise load it module
 *   compilation.
 *  @arg[bool] for_main
 *   If true, this is the main module of the program to load.
 *  @arg[FbleStringV*] build_deps
 *   Output to store list of files the load depended on. This should be a
 *   preinitialized vector, or NULL.
 *
 *  @returns FbleModule*
 *   The loaded module, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error message to stderr if the program cannot be parsed.
 *   @item
 *    The user should call FbleFreeProgram to free resources
 *    associated with the given program when it is no longer needed.
 *   @item
 *    The user should free strings added to build_deps when no longer
 *    needed, including in the case when program loading fails.
 */
static FbleModule* Load(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModuleV* loaded, Pending* pending, FbleModulePath* module_path, bool for_execution, bool for_main, FbleStringV* build_deps)
{
  // See if we've already loaded this module.
  FbleModule* module = LookupModule(loaded, module_path);
  if (module != NULL) {
    return FbleCopyModule(module);
  }

  // Make sure we aren't trying to load a module recursively.
  for (Pending* p = pending; p != NULL; p = p->tail) {
    if (FbleModulePathsEqual(p->path, module_path)) {
      FbleReportError("module ", module_path->loc);
      FblePrintModulePath(stderr, module_path);
      fprintf(stderr, " recursively depends on itself\n");
      return NULL;
    }
  }

  // Prepare to load the module.
  module = FbleAlloc(FbleModule);
  module->refcount = 1;
  module->magic = FBLE_MODULE_MAGIC;
  module->path = FbleCopyModulePath(module_path);
  FbleInitVector(module->type_deps);
  FbleInitVector(module->link_deps);
  module->type = NULL;
  module->value = NULL;
  module->code = NULL;
  module->exe = NULL;
  module->profile_blocks.size = 0;
  module->profile_blocks.xs = NULL;

  bool error = false;

  // Try to get the type of the module from its .fble.@ file.
  FbleModulePathV type_deps;
  FbleInitVector(type_deps);
  if (!ReadOptional(search_path, ".fble.@", module->path,
        &module->type, &type_deps, build_deps)) {
    error = true;
  }

  // Try loading the implementation from builtins if we have the type info.
  if (!error && for_execution && (module->type != NULL || for_main)) {
    for (size_t i = 0; i < builtins.size; ++i) {
      if (FbleModulePathsEqual(module_path, builtins.xs[i]->path)) {
        FblePreloadedModule* preloaded = builtins.xs[i];
        for (size_t j = 0; j < preloaded->deps.size; ++j) {
          FbleModule* dep = Preload(loaded, preloaded->deps.xs[j]);
          FbleAppendToVector(module->link_deps, dep);
        }
        module->exe = preloaded->executable;
        FbleInitVector(module->profile_blocks);
        for (size_t j = 0; j < preloaded->profile_blocks.size; ++j) {
          FbleAppendToVector(module->profile_blocks, FbleCopyName(preloaded->profile_blocks.xs[j]));
        }
        break;
      }
    }
  }

  // Fall back to .fble file in the following cases:
  // 1. We need it for execution because we don't have a builtin.
  // 2. We need it for type because we don't have a type.
  //    Except for the case of main module with builtin for execution.
  // 3. We need it for compilation of the main module.
  FbleModulePathV link_deps;
  FbleInitVector(link_deps);
  if (!error
      && ((for_execution && module->exe == NULL)
        || (module->type == NULL && (!for_execution || module->exe == NULL))
        || (!for_execution && for_main))) {
    if (!ReadRequired(search_path, ".fble", module->path,
          (for_main || for_execution) ? &module->value : &module->type,
          (for_main || for_execution) ? &link_deps : &type_deps,
          build_deps)) {
      error = true;
    }
  }

  // The module path location is currently pointing at whoever happened to
  // be depending on the module first. Fix that up if we can to point to
  // the .fble file the module was actually implemented in.
  if (module->value != NULL || module->type != NULL) {
    FbleExpr* body = module->value != NULL ? module->value : module->type;
    FbleLoc loc = { .source = FbleCopyString(body->loc.source), .line = 1, .col = 0 };
    FbleFreeLoc(module->path->loc);
    module->path->loc = loc;
  }

  // Load type dependencies.
  Pending npending = { .path = module_path, .tail = pending };
  for (size_t i = 0; !error && i < type_deps.size; ++i) {
    FbleProgram* dep = Load(builtins, search_path, loaded, &npending, type_deps.xs[i], for_execution, false, build_deps);
    if (dep == NULL) {
      error = true;
      break;
    }

    FbleAppendToVector(module->type_deps, dep);
  }

  // Load link dependencies.
  for (size_t i = 0; !error && i < link_deps.size; ++i) {
    FbleProgram* dep = Load(builtins, search_path, loaded, &npending, link_deps.xs[i], for_execution, false, build_deps);
    if (dep == NULL) {
      error = true;
      break;
    }
    FbleAppendToVector(module->link_deps, dep);
  }

  // Clean up type dependency paths.
  for (size_t i = 0; i < type_deps.size; ++i) {
    FbleFreeModulePath(type_deps.xs[i]);
  }
  FbleFreeVector(type_deps);

  // Clean up link dependency paths.
  for (size_t i = 0; i < link_deps.size; ++i) {
    FbleFreeModulePath(link_deps.xs[i]);
  }
  FbleFreeVector(link_deps);

  if (error) {
    FbleFreeModule(module);
    return NULL;
  }

  FbleAppendToVector(*loaded, module);
  return FbleCopyModule(module);
}

/**
 * @func[LoadMain] Loads an fble main program.
 *  @arg[FblePreloadedModuleV] builtins
 *   List of builtin modules to search.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for location .fble files. Borrowed.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load. Borrowed.
 *  @arg[bool] for_execution
 *   If true, load the program for execution. Otherwise load it module
 *   compilation.
 *  @arg[FbleStringV*] build_deps
 *   Output to store list of files the load depended on. This should be a
 *   preinitialized vector, or NULL.
 *
 *  @returns FbleProgram*
 *   The loaded program, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error message to stderr if the program cannot be parsed.
 *   @item
 *    The user should call FbleFreeProgram to free resources
 *    associated with the given program when it is no longer needed.
 *   @item
 *    The user should free strings added to build_deps when no longer
 *    needed, including in the case when program loading fails.
 */
static FbleProgram* LoadMain(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, bool for_execution, FbleStringV* build_deps)
{
  if (module_path == NULL) {
    fprintf(stderr, "no module path specified\n");
    return NULL;
  }

  FbleModuleV loaded;
  FbleInitVector(loaded);
  FbleModule* main = Load(builtins, search_path, &loaded, NULL, module_path, for_execution, true, build_deps);
  for (size_t i = 0; i < loaded.size; ++i) {
    FbleFreeModule(loaded.xs[i]);
  }
  FbleFreeVector(loaded);
  return main;
}

// See documentation in fble-load.h.
FbleProgram* FbleLoadForExecution(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  return LoadMain(builtins, search_path, module_path, true, build_deps);
}

// See documentation in fble-load.h.
FbleProgram* FbleLoadForModuleCompilation(FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  FblePreloadedModuleV builtins = { .size = 0, .xs = NULL };
  return LoadMain(builtins, search_path, module_path, false, build_deps);
}

// See documentation in fble-load.h.
void FbleSaveBuildDeps(FILE* fout, const char* target, FbleStringV build_deps)
{
  int cols = 1 + strlen(target);
  fprintf(fout, "%s:", target);
  for (size_t i = 0; i < build_deps.size; ++i) {
    const char* dep = build_deps.xs[i]->str;
    size_t len = 1 + strlen(dep);
    if (cols + len > 80) {
      fprintf(fout, " \\\n");
      cols = 1;
    }

    cols += len;
    fprintf(fout, " %s", dep);
  }
  fprintf(fout, "\n");
}
