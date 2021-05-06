// load.c --
//   This file implements routines for loading an fble program.

#include "fble-load.h"

#include <assert.h>   // for assert
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include "expr.h"
#include "fble-alloc.h"
#include "fble-name.h"
#include "fble-vector.h"

// Stack --
//   A stack of modules in the process of being loaded.
//
// Fields:
//   deps - other modules that this module immediately depends on.
//   deps_loaded - the number of deps we have attempted to load so far.
//   module - the value of the module.
//   tail - the rest of the stack of modules.
typedef struct Stack {
  FbleLoadedModule module;
  size_t deps_loaded;
  struct Stack* tail;
} Stack;

static FbleString* Find(const char* root, FbleModulePath* path);

// Find  -- 
//  Returns the path on disk of the .fble file for the given root directory
//  and module path.
//  
// Inputs:
//   root - file path to the root of the module search path. May be NULL.
//   path - the module path to find the source file for.
//
// Results:
//   The file path to the source code of the module, or NULL in case no such
//   file can be found.
//
// Side effects:
// * Prints an error message to stderr in case of error.
// * The user should call FbleFreeString when the returned string is no
//   longer needed.
static FbleString* Find(const char* root, FbleModulePath* path)
{
  if (root == NULL) {
    FbleReportError("module ", path->loc);
    FblePrintModulePath(stderr, path);
    fprintf(stderr, " not found\n");
    return NULL;
  }

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
      FbleReportError("module ", path->loc);
      FblePrintModulePath(stderr, path);
      fprintf(stderr, " not found\n");
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
    FbleReportError("module ", path->loc);
    FblePrintModulePath(stderr, path);
    fprintf(stderr, " not found\n");
    return NULL;
  }

  return FbleNewString(filename);
}

// FbleLoad -- see documentation in fble-load.h
FbleLoadedProgram* FbleLoad(const char* filename, const char* root)
{
  if (filename == NULL) {
    fprintf(stderr, "no input file\n");
    return NULL;
  }

  FbleLoadedProgram* program = FbleAlloc(FbleLoadedProgram);
  FbleVectorInit(program->modules);

  bool error = false;
  FbleString* filename_str = FbleNewString(filename);
  Stack* stack = FbleAlloc(Stack);
  stack->deps_loaded = 0;
  FbleLoc loc = { .source = FbleNewString(filename), .line = 1, .col = 0 };
  stack->module.path = FbleNewModulePath(loc);
  FbleFreeLoc(loc);
  FbleVectorInit(stack->module.deps);
  stack->tail = NULL;
  stack->module.value = FbleParse(filename_str, &stack->module.deps);
  if (stack->module.value == NULL) {
    error = true;
    stack->deps_loaded = stack->module.deps.size;
  }
  FbleFreeString(filename_str);

  while (stack != NULL) {
    if (stack->deps_loaded == stack->module.deps.size) {
      // We have loaded all the dependencies for this module.
      FbleVectorAppend(program->modules, stack->module);
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
    FbleVectorInit(stack->module.deps);
    stack->module.value = NULL;
    stack->tail = tail;

    FbleString* filename_str = Find(root, stack->module.path);
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

// FbleFreeLoadedProgram -- see documentation in fble-load.h
void FbleFreeLoadedProgram(FbleLoadedProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleLoadedModule* module = program->modules.xs + i;
      FbleFreeModulePath(module->path);
      for (size_t j = 0; j < module->deps.size; ++j) {
        FbleFreeModulePath(module->deps.xs[j]);
      }
      FbleFree(module->deps.xs);
      FbleFreeExpr(module->value);
    }
    FbleFree(program->modules.xs);
    FbleFree(program);
  }
}
