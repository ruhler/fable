/**
 * @file load.c
 * Routines for finding and loading fble modules.
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

/** An fble search path. */
struct FbleSearchPath {
  size_t size;      /**< The number of elements. */
  FbleString** xs;  /**< The elements. */
};

/** A stack of modules in the process of being loaded. */
typedef struct Stack {
  FbleLoadedModule module;  /**< The value of the module. */
  size_t deps_loaded;  /**< The number of deps we have attempted to load so far. */
  struct Stack* tail;  /**< The rest of the stack of modules. */
} Stack;

static FbleString* FindPackageAt(const char* package, const char* package_dir);
static FbleString* FindAt(const char* root, FbleModulePath* path, FbleStringV* build_deps);
static FbleString* Find(FbleSearchPath* search_path, FbleModulePath* path, FbleStringV* build_deps);

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
 * Finds a package in the given package directory.
 *
 * Searches for the package in the given directory.
 *
 * @param package  The name of the package.
 * @param package_dir The package directory to search.
 *
 * @returns
 *   The module root directory for the package, or NULL in case no such
 *   package is found.
 *
 * @sideeffects
 * * Allocates an FbleString that should be freed with FbleFreeString when no
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
 * Gets the file path for an fble module given the root directory.
 * 
 * Returns the path on disk of the .fble file for the given root directory and
 * module path.
 *  
 * @param root  File path to the root of the module search path.
 * @param path  The module path to find the source file for.
 * @param build_deps  Preinitialized output vector to store list of files searched in.
 *    May be NULL.
 *
 * @returns
 *   The file path to the source code of the module, or NULL in case no such
 *   file can be found.
 *
 * @sideeffects
 * * The user should call FbleFreeString when the returned string is no
 *   longer needed.
 * * Adds strings to build_deps that should be freed with FbleFreeString when no
 *   longer needed.
 */
static FbleString* FindAt(const char* root, FbleModulePath* path, FbleStringV* build_deps)
{
  assert(root != NULL);

  // Cacluate the length of the filename for the module.
  size_t len = strlen(root) + strlen(".fble") + 1;
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
  strcat(filename, ".fble");

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
 * Finds the source code for a module.
 *
 * Returns the path on disk of the .fble file for the given search path and
 * module path.
 *  
 * @param search_path  The module search path.
 * @param path  The module path to find the source file for.
 * @param build_deps  Preinitialized output vector to store list of files
 *   searched in. May be NULL.
 *
 * @returns
 *   The file path to the source code of the module, or NULL in case no such
 *   file can be found.
 *
 * @sideeffects
 * * Prints an error message to stderr in case of error.
 * * The user should call FbleFreeString when the returned string is no
 *   longer needed.
 */
static FbleString* Find(FbleSearchPath* search_path, FbleModulePath* path, FbleStringV* build_deps)
{
  FbleString* found = NULL;
  for (size_t i = 0; !found && i < search_path->size; ++i) {
    found = FindAt(search_path->xs[i]->str, path, build_deps);
  }

  if (found == NULL) {
    FbleReportError("module ", path->loc);
    FblePrintModulePath(stderr, path);
    fprintf(stderr, " not found\n");
    return NULL;
  }
  return found;
}

// See documentation in fble-load.h.
FbleLoadedProgram* FbleLoad(FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  if (module_path == NULL) {
    fprintf(stderr, "no module path specified\n");
    return NULL;
  }

  FbleString* filename = Find(search_path, module_path, build_deps);
  if (filename == NULL) {
    return NULL;
  }

  FbleLoadedProgram* program = FbleAlloc(FbleLoadedProgram);
  FbleInitVector(program->modules);

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
  stack->module.value = FbleParse(filename, &stack->module.deps);
  if (stack->module.value == NULL) {
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
    stack->tail = tail;

    FbleString* filename_str = Find(search_path, stack->module.path, build_deps);
    if (filename_str != NULL) {
      stack->module.value = FbleParse(filename_str, &stack->module.deps);
      FbleFreeString(filename_str);
    }

    if (stack->module.value == NULL) {
      error = true;
      stack->deps_loaded = stack->module.deps.size;
    }
  }

  if (error) {
    FbleFreeLoadedProgram(program);
    return NULL;
  }
  return program;
}

// See documentation in fble-load.h.
void FbleFreeLoadedProgram(FbleLoadedProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleLoadedModule* module = program->modules.xs + i;
      FbleFreeModulePath(module->path);
      for (size_t j = 0; j < module->deps.size; ++j) {
        FbleFreeModulePath(module->deps.xs[j]);
      }
      FbleFreeVector(module->deps);
      FbleFreeExpr(module->type);
      FbleFreeExpr(module->value);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
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
