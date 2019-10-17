// load.c --
//   This file implements routines for loading an fble program.

#include <assert.h>   // for assert
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include "internal.h"

// Stack --
//   A stack of modules in the process of being loaded.
typedef struct Stack {
  FbleModuleRefV module_refs;
  FbleNameV path;
  FbleExpr* value;
  struct Stack* tail;
} Stack;

static bool PathsEqual(FbleNameV a, FbleNameV b);
static bool ResolvePath(FbleArena* arena, const char* root, FbleNameV base, FbleModuleRef* ref, FbleNameV* resolved);
static FbleExpr* Parse(FbleArena* arena, FbleNameV path, const char* include_path, FbleModuleRefV* module_refs);

// PathsEqual --
//   Checks if two absolute moduel paths are equal.
//
// Inputs:
//   a - the first module path
//   b - the second module path
//
// Results:
//   true if the given module paths are equal, false otherwise.
//
// Side effects:
//   None.
static bool PathsEqual(FbleNameV a, FbleNameV b)
{
  if (a.size != b.size) {
    return false;
  }

  for (size_t i = 0; i < a.size; ++i) {
    if (!FbleNamesEqual(a.xs + i, b.xs + i)) {
      return false;
    }
  }
  return true;
}

// ResolvePath --
//   Resolve a possibly relative path to its full absolute path.
//
// Inputs:
//   arena - arena to use for allocations.
//   root - The path to the root of the module hierarchy on disk.
//   base - The path of the moduel from which the reference is made.
//   ref - the reference to resolve.
//   resolved - output parameter set to the absolute path to the module being
//              referred to.
//
// Results:
//   true if the module was successfully resolved, false otherwise.
//
// Side effects:
//   Sets 'resolved' to the resolved absolute path. Prints a message to stderr
//   in case of failure to resolve the reference.
static bool ResolvePath(FbleArena* arena, const char* root, FbleNameV base, FbleModuleRef* ref, FbleNameV* resolved)
{
  if (ref->is_absolute) {
    *resolved = ref->path;
    return true;
  }

  // Check for all possible paths from the base.
  for (size_t j = 0; j <= base.size; ++j) {
    size_t k = base.size - j;

    FbleNameV path;
    FbleVectorInit(arena, path);
    for (size_t i = 0; i < k; ++i) {
      FbleVectorAppend(arena, path, base.xs[i]);
    }

    for (size_t i = 0; i < ref->path.size; ++i) {
      FbleVectorAppend(arena, path, ref->path.xs[i]);
    }

    size_t len = strlen(root) + strlen(".fble") + 1;
    for (size_t i = 0; i < path.size; ++i) {
      len += 1 + strlen(path.xs[i].name);
    }

    char filename[len];
    filename[0] = '\0';
    strcat(filename, root);
    for (size_t i = 0; i < path.size; ++i) {
      len += 1 + strlen(path.xs[i].name);
      strcat(filename, "/");
      strcat(filename, path.xs[i].name);
    }
    strcat(filename, ".fble");

    if (access(filename, F_OK) == 0) {
      *resolved = path;
      return true;
    }

    FbleFree(arena, path.xs);
  }

  // TODO: Give a better error message here.
  FbleReportError("Module not found\n", &ref->path.xs[0].loc);
  return false;
}

// Parse  -- 
//  Parse an expression from given path.
//  See documentation of FbleParse in fble-syntax.h for details.
static FbleExpr* Parse(FbleArena* arena, FbleNameV path, const char* include_path, FbleModuleRefV* module_refs)
{
  size_t len = strlen(include_path) + strlen(".fble") + 1;
  for (size_t i = 0; i < path.size; ++i) {
    len += 1 + strlen(path.xs[i].name);
  }

  char filename[len];
  filename[0] = '\0';
  strcat(filename, include_path);
  for (size_t i = 0; i < path.size; ++i) {
    len += 1 + strlen(path.xs[i].name);
    strcat(filename, "/");
    strcat(filename, path.xs[i].name);
  }
  strcat(filename, ".fble");

  return FbleParse(arena, filename, include_path, module_refs);
}

// FbleLoad -- see documentation in fble-syntax.h
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root)
{
  FbleProgram* program = FbleAlloc(arena, FbleProgram);
  FbleVectorInit(arena, program->modules);

  Stack* stack = FbleAlloc(arena, Stack);
  FbleVectorInit(arena, stack->module_refs);
  FbleVectorInit(arena, stack->path);
  stack->tail = NULL;

  stack->value = FbleParse(arena, filename, root, &stack->module_refs);
  if (stack->value == NULL) {
    return NULL;
  }

  while (stack != NULL) {
    if (stack->module_refs.size == 0) {
      // We have loaded all the dependencies for this module.
      FbleModule* module = FbleVectorExtend(arena, program->modules);
      module->path = stack->path;
      module->value = stack->value;
      Stack* tail = stack->tail;
      FbleFree(arena, stack->module_refs.xs);
      FbleFree(arena, stack);
      stack = tail;
      continue;
    }
    
    FbleModuleRef* ref = stack->module_refs.xs[stack->module_refs.size-1];
    FbleNameV resolved;
    FbleVectorInit(arena, resolved);
    // TODO: Avoid resolving the same reference over and over again.
    if (!ResolvePath(arena, root, stack->path, ref, &resolved)) {
      return NULL;
    }

    // Check to see if we have already loaded this path.
    for (size_t i = 0; i < program->modules.size; ++i) {
      if (PathsEqual(resolved, program->modules.xs[i].path)) {
        ref->resolved = i;
        stack->module_refs.size--;
        break;
      }
    }
    if (ref->resolved != FBLE_UNRESOLVED_MODULE_ID) {
      FbleFree(arena, resolved.xs);
      continue;
    }

    for (Stack* s = stack; s != NULL; s = s->tail) {
      if (PathsEqual(resolved, s->path)) {
        // TODO: Improve the error message.
        FbleReportError("recursive module dependency detected.\n",
            &s->path.xs[s->path.size-1].loc);
        return NULL;
      }
    }

    // Parse the new module, placing it on the stack for processing.
    Stack* tail = stack;
    stack = FbleAlloc(arena, Stack);
    FbleVectorInit(arena, stack->module_refs);
    stack->path = resolved;
    stack->tail = tail;
    stack->value = Parse(arena, stack->path, root, &stack->module_refs);
    if (stack->value == NULL) {
      return NULL;
    }
  }

  // The last module loaded should be the main entry point.
  program->modules.size--;
  program->main = program->modules.xs[program->modules.size].value;
  return program;
}
