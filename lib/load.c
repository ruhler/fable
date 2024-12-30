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
 * @struct[Stack] A stack of modules in the process of being loaded.
 *  @field[FbleModule][module] The value of the module.
 *  @field[size_t][deps_loaded]
 *   The number of deps we have attempted to load so far.
 *  @field[Stack*][tail] The rest of the stack of modules.
 */
typedef struct Stack {
  FbleModule module;
  size_t deps_loaded;
  struct Stack* tail;
} Stack;

static FbleString* FindPackageAt(const char* package, const char* package_dir);
static FbleString* FindAt(const char* root, const char* suffix, FbleModulePath* path, FbleStringV* build_deps);
static FbleString* Find(FbleSearchPath* search_path, const char* suffix, FbleModulePath* path, FbleStringV* build_deps);
static void Preload(FbleProgram* program, FblePreloadedModule* preloaded);
static FbleProgram* Load(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, bool for_execution, FbleStringV* build_deps);


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
 * @func[Preload] Add a preloaded module to a program.
 *  @arg[FbleProgram*][program] The program to add the module to.
 *  @arg[FblePreloadedModule*][preloaded] The module to add.
 *  @sideeffects
 *   Recursively adds the module and all its dependencies to the program.
 */
static void Preload(FbleProgram* program, FblePreloadedModule* preloaded)
{
  // Check if we've already loaded the module.
  for (size_t i = 0; i < program->modules.size; ++i) {
    if (FbleModulePathsEqual(program->modules.xs[i].path, preloaded->path)) {
      return;
    }
  }

  // Load the dependencies.
  for (size_t i = 0; i < preloaded->deps.size; ++i) {
    Preload(program, preloaded->deps.xs[i]);
  }

  FbleModule* module = FbleExtendVector(program->modules);
  module->path = FbleCopyModulePath(preloaded->path);
  FbleInitVector(module->deps);
  for (size_t i = 0; i < preloaded->deps.size; ++i) {
    FbleAppendToVector(module->deps, FbleCopyModulePath(preloaded->deps.xs[i]->path));
  }
  module->type = NULL;
  module->value = NULL;
  module->code = NULL;
  module->exe = preloaded->executable;
  FbleInitVector(module->profile_blocks);
  for (size_t i = 0; i < preloaded->profile_blocks.size; ++i) {
    FbleAppendToVector(module->profile_blocks, FbleCopyName(preloaded->profile_blocks.xs[i]));
  }
}

/**
 * @func[Load] Loads an fble program.
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
 *   The parsed program, or NULL in case of error.
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
static FbleProgram* Load(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, bool for_execution, FbleStringV* build_deps)
{
  if (module_path == NULL) {
    fprintf(stderr, "no module path specified\n");
    return NULL;
  }

  FbleProgram* program = FbleAlloc(FbleProgram);
  FbleInitVector(program->modules);

  for (size_t i = 0; i < builtins.size; ++i) {
    if (FbleModulePathsEqual(module_path, builtins.xs[i]->path)) {
      Preload(program, builtins.xs[i]);
      return program;
    }
  }

  FbleString* filename = Find(search_path, ".fble", module_path, build_deps);
  if (filename == NULL) {
    FbleReportError("module ", module_path->loc);
    FblePrintModulePath(stderr, module_path);
    fprintf(stderr, " not found\n");
    FbleFreeProgram(program);
    return NULL;
  }

  bool error = false;
  Stack* stack = FbleAlloc(Stack);
  stack->deps_loaded = 0;

  FbleLoc loc = { .source = FbleCopyString(filename), .line = 1, .col = 0 };
  stack->module.path = FbleCopyModulePath(module_path);
  FbleFreeLoc(stack->module.path->loc);
  stack->module.path->loc = loc;

  FbleInitVector(stack->module.deps);
  stack->tail = NULL;
  stack->module.type = NULL;
  stack->module.code = NULL;
  stack->module.exe = NULL;
  stack->module.profile_blocks.size = 0;
  stack->module.profile_blocks.xs = NULL;
  stack->module.value = FbleParse(filename, &stack->module.deps);

  FbleString* header = Find(search_path, ".fble.@", module_path, build_deps);
  if (header != NULL) {
    stack->module.type = FbleParse(header, &stack->module.deps);
    FbleFreeString(header);
  }

  if (stack->module.value == NULL
      || (header != NULL && stack->module.type == NULL)) {
    error = true;
    stack->deps_loaded = stack->module.deps.size;
  }
  FbleFreeString(filename);

  while (stack != NULL) {
    if (stack->deps_loaded == stack->module.deps.size) {
      // We have loaded all the dependencies for this module.
      FbleAppendToVector(program->modules, stack->module);
      Stack* tail = stack->tail;
      FbleFree(stack);
      stack = tail;
      continue;
    }
    
    FbleModulePath* ref = stack->module.deps.xs[stack->deps_loaded];

    // Check to see if we have already loaded this path.
    bool found = false;
    for (size_t i = 0; i < program->modules.size; ++i) {
      if (FbleModulePathsEqual(ref, program->modules.xs[i].path)) {
        stack->deps_loaded++;
        found = true;
        break;
      }
    }
    if (found) {
      continue;
    }

    // See if the module is available as a builtin.
    for (size_t i = 0; i < builtins.size; ++i) {
      if (FbleModulePathsEqual(ref, builtins.xs[i]->path)) {
        Preload(program, builtins.xs[i]);
        found = true;
        break;
      }
    }
    if (found) {
      continue;
    }

    // Make sure we aren't trying to load a module recursively.
    bool recursive = false;
    for (Stack* s = stack; s != NULL; s = s->tail) {
      if (FbleModulePathsEqual(ref, s->module.path)) {
        FbleReportError("module ", ref->loc);
        FblePrintModulePath(stderr, ref);
        fprintf(stderr, " recursively depends on itself\n");
        error = true;
        recursive = true;
        stack->deps_loaded = stack->module.deps.size;
        break;
      }
    }

    if (recursive) {
      continue;
    }

    // Parse the new module, placing it on the stack for processing.
    Stack* tail = stack;
    stack = FbleAlloc(Stack);
    stack->deps_loaded = 0;
    stack->module.path = FbleCopyModulePath(ref);
    FbleInitVector(stack->module.deps);
    stack->module.type = NULL;
    stack->module.value = NULL;
    stack->module.code = NULL;
    stack->module.exe = NULL;
    stack->module.profile_blocks.size = 0;
    stack->module.profile_blocks.xs = NULL;
    stack->tail = tail;

    FbleString* header_str = Find(search_path, ".fble.@", stack->module.path, build_deps);
    if (header_str != NULL) {
      stack->module.type = FbleParse(header_str, &stack->module.deps);
      FbleFreeString(header_str);
      if (stack->module.type == NULL) {
        error = true;
        stack->deps_loaded = stack->module.deps.size;
      }
    }

    if (for_execution || header_str == NULL) {
      FbleString* filename_str = Find(search_path, ".fble", stack->module.path, build_deps);
      if (filename_str == NULL) {
        FbleReportError("module ", stack->module.path->loc);
        FblePrintModulePath(stderr, stack->module.path);
        fprintf(stderr, " not found\n");
        error = true;
        stack->deps_loaded = stack->module.deps.size;
      } else {
        FbleExpr* parsed = FbleParse(filename_str, &stack->module.deps);
        if (parsed == NULL) {
          error = true;
          stack->deps_loaded = stack->module.deps.size;
        }

        if (for_execution) {
          stack->module.value = parsed;
        } else {
          assert(stack->module.type == NULL);
          stack->module.type = parsed;
        }
        FbleFreeString(filename_str);
      }
    }
  }

  if (error) {
    FbleFreeProgram(program);
    return NULL;
  }
  return program;
}

// See documentation in fble-load.h.
FbleProgram* FbleLoadForExecution(FblePreloadedModuleV builtins, FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  return Load(builtins, search_path, module_path, true, build_deps);
}

// See documentation in fble-load.h.
FbleProgram* FbleLoadForModuleCompilation(FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  FblePreloadedModuleV builtins = { .size = 0, .xs = NULL };
  return Load(builtins, search_path, module_path, false, build_deps);
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
