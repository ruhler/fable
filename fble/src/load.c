// load.c --
//   This file implements routines for loading an fble program.

#include <assert.h>   // for assert
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include "fble.h"
#include "instr.h"
#include "syntax.h"

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
//   deps - other modules that this module immediately depends on.
//   deps_loaded - the number of deps we have attempted to load so far.
//   module - the value of the module.
//   tail - the rest of the stack of modules.
typedef struct Stack {
  FbleModule module;
  size_t deps_loaded;
  struct Stack* tail;
} Stack;

static bool AccessAllowed(Tree* tree, FbleModulePath* source, FbleModulePath* target);
static void FreeTree(FbleArena* arena, Tree* tree);
static FbleString* Find(FbleArena* arena, const char* root, Tree* tree, FbleModulePath* path);

// AccessAllowed --
//   Check if the module at the given source path is allowed to access the
//   module at the given target path, according to the access modifiers in the
//   tree.
//
// Inputs:
//   tree - tree with access modifier information
//   source - path to the module making a reference to another module.
//   target - path to the module being referred to.
//
// Results:
//   true if source is allowed to access target, false otherwise.
//
// Side effects:
//   none.
static bool AccessAllowed(Tree* tree, FbleModulePath* source, FbleModulePath* target)
{
  // Count how many nodes in the path to the target from the root are visible
  // to the source thanks to it being under a sibling node.
  size_t visible = 0;
  while (source != NULL && visible < source->path.size && visible < target->path.size
      && FbleNamesEqual(source->path.xs[visible], target->path.xs[visible])) {
    visible++;
  }

  // Ensure all nodes that aren't visible due to sibling state are public.
  for (size_t i = 0; i < target->path.size; ++i) {
    Tree* child = NULL;
    for (size_t j = 0; j < tree->children.size; ++j) {
      if (FbleNamesEqual(tree->children.xs[j]->name, target->path.xs[i])) {
        child = tree->children.xs[j];
        break;
      }
    }
    assert(child != NULL);
    tree = child;

    if (tree->private && i > visible) {
      return false;
    }
  }
  return true;
}

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
  FbleFree(arena, tree);
}

// Find  -- 
//  Locate the file associated with the given module path, enforcing
//  visibility rules in the process.
//  
// Inputs:
//   arena - arena to use for allocations.
//   root - file path to the root of the module search path. May be NULL.
//   tree - the module hierarchy known so far.
//   path - the module path to find the source file for.
//
// Results:
//   The file path to the source code of the module, or NULL in case of error.
//
// Side effects:
// * Prints an error message to stderr in case of error.
// * Updates the tree with new module hierarchy information.
// * The user should call FbleFreeString when the returned string is no
//   longer needed.
static FbleString* Find(FbleArena* arena, const char* root, Tree* tree, FbleModulePath* path)
{
  if (root == NULL) {
    FbleReportError("module ", path->loc);
    FblePrintModulePath(stderr, path);
    fprintf(stderr, " not found\n");
    return NULL;
  }

  // Compute an upper bound on the length of the filename for the module.
  size_t len = strlen(root) + strlen(".fble") + 1;
  for (size_t i = 0; i < path->path.size; ++i) {
    // "/name*"
    len += 1 + strlen(path->path.xs[i].name->str) + 1;

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

  // Find the path to the module on disk, which depends on the access
  // modifiers for modules in the path.
  char filename[len];
  filename[0] = '\0';
  strcat(filename, root);
  for (size_t i = 0; i < path->path.size; ++i) {
    strcat(filename, "/");
    strcat(filename, path->path.xs[i].name->str);

    bool treed = false;
    for (size_t c = 0; c < tree->children.size; ++c) {
      if (FbleNamesEqual(path->path.xs[i], tree->children.xs[c]->name)) {
        treed = true;
        tree = tree->children.xs[c];
        break;
      }
    }

    if (!treed) {
      Tree* child = FbleAlloc(arena, Tree);
      child->name = path->path.xs[i];
      FbleVectorInit(arena, child->children);
      FbleVectorAppend(arena, tree->children, child);
      tree = child;

      char* tail = filename + strlen(filename);
      assert(*tail == '\0');

      if (i + 1 == path->path.size) {
        strcat(filename, ".fble");
      }
      bool public = (access(filename, F_OK) == 0);
      *tail = '\0';

      strcat(filename, "*");
      if (i + 1 == path->path.size) {
        strcat(filename, ".fble");
      }
      bool private = (access(filename, F_OK) == 0);
      *tail = '\0';

      if (public && private) {
        FbleReportError("module ", path->loc);
        size_t size = path->path.size;
        path->path.size = i + 1;
        FblePrintModulePath(stderr, path);
        fprintf(stderr, " is marked as both public and private\n");
        path->path.size = size;
        return NULL;
      } else if (public) {
        child->private = false;
      } else if (private) {
        child->private = true;
      } else {
        FbleReportError("module ", path->loc);
        FblePrintModulePath(stderr, path);
        fprintf(stderr, " not found\n");
        return NULL;
      }
    }

    if (tree->private) {
      strcat(filename, "*");
    }
  }
  strcat(filename, ".fble");
  return FbleNewString(arena, filename);
}

// FbleLoad -- see documentation in fble.h
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root)
{
  FbleProgram* program = FbleAlloc(arena, FbleProgram);
  FbleVectorInit(arena, program->modules);

  bool error = false;
  FbleString* filename_str = FbleNewString(arena, filename);
  Stack* stack = FbleAlloc(arena, Stack);
  stack->deps_loaded = 0;
  stack->module.path = NULL;
  FbleVectorInit(arena, stack->module.deps);
  stack->tail = NULL;
  stack->module.value = FbleParse(arena, filename_str, &stack->module.deps);
  if (stack->module.value == NULL) {
    error = true;
    stack->deps_loaded = stack->module.deps.size;
  }
  FbleFreeString(arena, filename_str);

  Tree* tree = FbleAlloc(arena, Tree);
  tree->name.name = FbleNewString(arena, "");
  tree->name.loc.source = FbleNewString(arena, "???");
  tree->name.loc.line = 0;
  tree->name.loc.col = 0;
  tree->private = false;
  FbleVectorInit(arena, tree->children);

  while (stack != NULL) {
    if (stack->deps_loaded == stack->module.deps.size) {
      // We have loaded all the dependencies for this module.
      FbleVectorAppend(arena, program->modules, stack->module);
      Stack* tail = stack->tail;
      FbleFree(arena, stack);
      stack = tail;
      continue;
    }
    
    FbleModulePath* ref = stack->module.deps.xs[stack->deps_loaded];

    // Check to see if we have already loaded this path.
    bool found = false;
    for (size_t i = 0; i < program->modules.size; ++i) {
      if (FbleModulePathsEqual(ref, program->modules.xs[i].path)) {
        // We may have failed to load a module previously. Check to see if the
        // module has been loaded before doing the access allowed check.
        // There's no point checking access against a module that failed to
        // load, and if the module failed to load, we may not have updated the
        // Tree in the way AccessAllowed expects.
        if (program->modules.xs[i].value != NULL
            && !AccessAllowed(tree, stack->module.path, ref)) {
          FbleReportError("module ", ref->loc);
          FblePrintModulePath(stderr, ref);
          fprintf(stderr, " is private\n");
          error = true;
        }

        stack->deps_loaded++;
        found = true;
        break;
      }
    }
    if (found) {
      continue;
    }

    bool recursive = false;
    for (Stack* s = stack; s->tail != NULL; s = s->tail) {
      assert(s->module.path != NULL);
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
    stack = FbleAlloc(arena, Stack);
    stack->deps_loaded = 0;
    stack->module.path = FbleCopyModulePath(ref);
    FbleVectorInit(arena, stack->module.deps);
    stack->module.value = NULL;
    stack->tail = tail;

    FbleString* filename_str = Find(arena, root, tree, stack->module.path);
    if (filename_str != NULL) {
      stack->module.value = FbleParse(arena, filename_str, &stack->module.deps);
      FbleFreeString(arena, filename_str);
    }

    if (stack->module.value == NULL) {
      error = true;
      stack->deps_loaded = stack->module.deps.size;
    }
  }
  FbleFreeString(arena, tree->name.name);
  FbleFreeString(arena, tree->name.loc.source);
  FreeTree(arena, tree);

  if (error) {
    FbleFreeProgram(arena, program);
    return NULL;
  }
  return program;
}
