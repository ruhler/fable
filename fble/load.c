// load.c --
//   This file implements routines for loading an fble program.

#include <assert.h>   // for assert
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include "internal.h"

typedef struct Tree Tree;

// TreeV --
//   A vector of Tree.
typedef struct {
  size_t size;
  Tree** xs;
} TreeV;

// Tree --
//   A module hierarchy tree tracking access modes for modules.
struct Tree {
  FbleName name;
  bool private;
  TreeV children;
};

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

static void FreeTree(FbleArena* arena, Tree* tree);
static bool PathsEqual(FbleNameV a, FbleNameV b);
static void PathToName(FbleArena* arena, FbleNameV path, FbleName* name);
static FbleExpr* Parse(FbleArena* arena, const char* root, Tree* tree, FbleNameV path, FbleModuleRefV* module_refs);

// FreeTree --
//   Free memory associated with the given tree.
//
// Inputs:
//   arena - arena to use for allocations
//   tree - the tree to free
//
// Results:
//   none.
//
// Side effects:
//   Release memory resources used for the tree. Do not access the tree after
//   calling this function.
void FreeTree(FbleArena* arena, Tree* tree)
{
  for (size_t i = 0; i < tree->children.size; ++i) {
    FreeTree(arena, tree->children.xs[i]);
  }
  FbleFree(arena, tree->children.xs);
}

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

// Parse  -- 
//  Parse an expression from given path.
//  
// Inputs:
//   arena - arena to use for allocations
//   root - file path to the root of the module search path
//   tree - the module hierarchy known so far
//   path - the resolved module path to parse
//   module_refs - Output param: A list of the module references in the parsed
//                 expression.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
//   Prints an error message to stderr if the program cannot be parsed.
//   Updates the tree with new module hierarchy information.
//   Appends module references in the parsed expression to module_refs, which
//   is assumed to be a pre-initialized vector.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned program if there is no error.
static FbleExpr* Parse(FbleArena* arena, const char* root, Tree* tree, FbleNameV path, FbleModuleRefV* module_refs)
{
  // Compute an upper bound on the length of the filename for the module.
  size_t len = strlen(root) + strlen(".fble") + 1;
  for (size_t i = 0; i < path.size; ++i) {
    // "/name*"
    len += 1 + strlen(path.xs[i].name) + 1;
  }

  // Find the path to the module on disk, which depends on the access
  // modifiers for modules in the path.
  char filename[len];
  filename[0] = '\0';
  strcat(filename, root);
  for (size_t i = 0; i < path.size; ++i) {
    strcat(filename, "/");
    strcat(filename, path.xs[i].name);

    bool treed = false;
    for (size_t c = 0; c < tree->children.size; ++c) {
      if (FbleNamesEqual(&path.xs[i], &tree->children.xs[c]->name)) {
        treed = true;
        tree = tree->children.xs[c];
        if (tree->private) {
          strcat(filename, "*");
        }
        break;
      }
    }

    if (!treed) {
      Tree* child = FbleAlloc(arena, Tree);
      child->name = path.xs[i];
      FbleVectorInit(arena, child->children);
      FbleVectorAppend(arena, tree->children, child);
      tree = child;

      char* tail = filename + strlen(filename);
      if (i + 1 == path.size) {
        strcat(filename, ".fble");
      }
      bool public = (access(filename, F_OK) == 0);
      // perror(filename);

      *tail = '\0';
      strcat(filename, "*");
      if (i + 1 == path.size) {
        strcat(filename, ".fble");
      }
      bool private = (access(filename, F_OK) == 0);
      // perror(filename);
      *tail = '\0';

      if (public && private) {
        FbleReportError("module ", &path.xs[0].loc);
        for (size_t j = 0; j < i; ++j) {
          fprintf(stderr, "/%s", path.xs[j].name);
        }
        fprintf(stderr, "%% marked as both public and private\n");
        return NULL;
      } else if (public) {
        child->private = false;
      } else if (private) {
        child->private = true;
      } else {
        FbleReportError("module ", &path.xs[0].loc);
        for (size_t j = 0; j < i; ++j) {
          fprintf(stderr, "/%s", path.xs[j].name);
        }
        fprintf(stderr, "%% not found\n");
        return NULL;
      }
    }
  }
  strcat(filename, ".fble");

  FbleExpr* expr = FbleParse(arena, filename, module_refs);
  if (expr == NULL) {
    FbleReportError("module ", &path.xs[0].loc);
    for (size_t i = 0; i < path.size; ++i) {
      fprintf(stderr, "/%s", path.xs[i].name);
    }
    fprintf(stderr, "%%: failed to parse\n");
  }
  return expr;
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

  Tree* tree = FbleAlloc(arena, Tree);
  tree->name.name = "";
  tree->name.loc.source = "???";
  tree->name.loc.line = 0;
  tree->name.loc.col = 0;
  tree->private = false;
  FbleVectorInit(arena, tree->children);

  while (stack != NULL) {
    if (stack->module_refs.size == 0) {
      // We have loaded all the dependencies for this module.
      FbleModule* module = FbleVectorExtend(arena, program->modules);
      PathToName(arena, stack->path, &module->name);
      module->value = stack->value;
      Stack* tail = stack->tail;
      FbleFree(arena, stack->module_refs.xs);
      FbleFree(arena, stack);
      stack = tail;
      continue;
    }
    
    FbleModuleRef* ref = stack->module_refs.xs[stack->module_refs.size-1];

    // Check to see if we have already loaded this path.
    FbleName resolved_name;
    PathToName(arena, ref->path, &resolved_name);
    bool found = false;
    for (size_t i = 0; i < program->modules.size; ++i) {
      if (FbleNamesEqual(&resolved_name, &program->modules.xs[i].name)) {
        // TODO: Check that the path is visible to this module.
        ref->resolved = program->modules.xs[i].name;
        stack->module_refs.size--;
        found = true;
        break;
      }
    }
    FbleFree(arena, (void*)resolved_name.name);
    if (found) {
      continue;
    }

    for (Stack* s = stack; s != NULL; s = s->tail) {
      if (PathsEqual(ref->path, s->path)) {
        FbleName* module = ref->path.xs + ref->path.size - 1;
        FbleReportError("recursive module dependency\n", &module->loc);
        FreeTree(arena, tree);
        return NULL;
      }
    }

    // Parse the new module, placing it on the stack for processing.
    // TODO: Check that the path is visible to this module.
    Stack* tail = stack;
    stack = FbleAlloc(arena, Stack);
    FbleVectorInit(arena, stack->module_refs);
    stack->path = ref->path;
    stack->tail = tail;
    stack->value = Parse(arena, root, tree, stack->path, &stack->module_refs);
    if (stack->value == NULL) {
      FreeTree(arena, tree);
      return NULL;
    }
  }
  FreeTree(arena, tree);

  // The last module loaded should be the main entry point.
  program->modules.size--;
  program->main = program->modules.xs[program->modules.size].value;
  return program;
}
