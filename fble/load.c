// load.c --
//   This file implements routines for loading an fble program.

#include <assert.h>   // for assert
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include "internal.h"

// Stack --
//   A stack of modules in the process of being loaded.
//
// Fields:
//   module_refs - relative references from the module to other modules.
//   path - the resolved path to the module.
//   value - the value of the module.
//   tail - the rest of the stack of modules.
typedef struct Stack {
  FbleModuleRefV module_refs;
  FbleNameV path;
  FbleExpr* value;
  struct Stack* tail;
} Stack;

static bool PathsEqual(FbleNameV a, FbleNameV b);
static void PathToName(FbleArena* arena, FbleNameV path, FbleName* name);
static bool ResolvePath(FbleArena* arena, const char* root, FbleNameV base, FbleModuleRef* ref, FbleNameV* resolved);
static FbleExpr* Parse(FbleArena* arena, FbleNameV path, const char* root, FbleModuleRefV* module_refs);

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

// PathToName --
//   Convert an absolute module path to the corresponding canonical module
//   name.
//
// Inputs:
//   arena - arena to use for allocations.
//   path - an absolute path to a module.
//   name - output parameter set to the canonical name for the module.
//
// Results:
//   None.
//
// Side effects:
//   Sets name to the canonical name for the module.
static void PathToName(FbleArena* arena, FbleNameV path, FbleName* name)
{
  FbleLoc loc = {
    .source = "???",
    .line = 0,
    .col = 0
  };

  if (path.size > 0) {
    loc = path.xs[path.size - 1].loc;
  }

  size_t len = 1;
  for (size_t i = 0; i < path.size; ++i) {
    len += 1 + strlen(path.xs[i].name);
  }

  char* name_name = FbleArrayAlloc(arena, char, len);
  name->name = name_name;
  name->loc = loc;
  name->space = FBLE_MODULE_NAME_SPACE;

  name_name[0] = '\0';
  for (size_t i = 0; i < path.size; ++i) {
    strcat(name_name, "/");
    strcat(name_name, path.xs[i].name);
  }
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
//   Sets 'resolved' to the resolved absolute path. The caller is responsible
//   for freeing the resolved->xs when no longer required.
//   Prints a message to stderr in case of failure to resolve the reference.
static bool ResolvePath(FbleArena* arena, const char* root, FbleNameV base, FbleModuleRef* ref, FbleNameV* resolved)
{
  // Check for all possible paths from the base.
  for (size_t j = ref->is_absolute ? base.size : 0; j <= base.size; ++j) {
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

  FbleReportError("module ", &ref->path.xs[ref->path.size-1].loc);
  const char* slash = "";
  for (size_t i = 0; i < ref->path.size; ++i) {
    fprintf(stderr, "%s%s", slash, ref->path.xs[i].name);
    slash = "/";
  }
  fprintf(stderr, "%% not found\n");
  return false;
}

// Parse  -- 
//  Parse an expression from given path.
//  
// Inputs:
//   arena - arena to use for allocations
//   path - the resolved module path to parse
//   root - file path to the root of the module search path
//   module_refs - Output param: A list of the module references in the parsed
//                 expression.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
//   Prints an error message to stderr if the program cannot be parsed.
//   Appends module references in the parsed expression to module_refs, which
//   is assumed to be a pre-initialized vector.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned program if there is no error.
static FbleExpr* Parse(FbleArena* arena, FbleNameV path, const char* root, FbleModuleRefV* module_refs)
{
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

  return FbleParse(arena, filename, module_refs);
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

  stack->value = FbleParse(arena, filename, &stack->module_refs);
  if (stack->value == NULL) {
    return NULL;
  }

  while (stack != NULL) {
    if (stack->module_refs.size == 0) {
      // We have loaded all the dependencies for this module.
      FbleModule* module = FbleVectorExtend(arena, program->modules);
      PathToName(arena, stack->path, &module->name);
      module->value = stack->value;
      Stack* tail = stack->tail;
      FbleFree(arena, stack->path.xs);  // BUG HERE: Freeing what we don't mean to
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
    FbleName resolved_name;
    PathToName(arena, resolved, &resolved_name);
    bool found = false;
    for (size_t i = 0; i < program->modules.size; ++i) {
      if (FbleNamesEqual(&resolved_name, &program->modules.xs[i].name)) {
        ref->resolved = program->modules.xs[i].name;
        stack->module_refs.size--;
        found = true;
        break;
      }
    }
    FbleFree(arena, (void*)resolved_name.name);
    if (found) {
      FbleFree(arena, resolved.xs);
      continue;
    }

    for (Stack* s = stack; s != NULL; s = s->tail) {
      if (PathsEqual(resolved, s->path)) {
        FbleName* module = resolved.xs + resolved.size - 1;
        FbleReportError("recursive module dependency\n", &module->loc);
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
