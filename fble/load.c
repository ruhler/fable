// load.c --
//   This file implements routines for loading an fble program.

#include <assert.h>   // for assert
#include <string.h>   // for strcat
#include <unistd.h>   // for access, F_OK

#include "fble.h"
#include "instr.h"

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

static void PrintModuleName(FILE* fout, FbleNameV path);
static bool AccessAllowed(Tree* tree, FbleNameV source, FbleNameV target);
static void FreeTree(FbleArena* arena, Tree* tree);
static bool PathsEqual(FbleNameV a, FbleNameV b);
static void PathToName(FbleArena* arena, FbleNameV path, FbleName* name);
static FbleExpr* Parse(FbleArena* load_arena, FbleArena* program_arena, const char* root, Tree* tree, FbleNameV path, FbleModuleRefV* module_refs);

// PrintModuleName --
//   Print the name of a module to the given stream.
//
// Inputs: 
//   fout - the stream to print to.
//   path - the path to the module.
//
// Results:
//   None.
//
// Side effects:
//   Prints the module name to the given stream.
static void PrintModuleName(FILE* fout, FbleNameV path)
{
  if (path.size == 0) {
    fprintf(fout, "/");
  }
  for (size_t i = 0; i < path.size; ++i) {
    fprintf(fout, "/%s", path.xs[i].name);
  }
  fprintf(fout, "%%");
}

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
static bool AccessAllowed(Tree* tree, FbleNameV source, FbleNameV target)
{
  // Count how many nodes in the path to the target from the root are visible
  // to the source thanks to it being under a sibling node.
  size_t visible = 0;
  while (visible < source.size && visible < target.size
      && FbleNamesEqual(source.xs + visible, target.xs + visible)) {
    visible++;
  }

  // Ensure all nodes that aren't visible due to sibling state are public.
  for (size_t i = 0; i < target.size; ++i) {
    Tree* child = NULL;
    for (size_t j = 0; j < tree->children.size; ++j) {
      if (FbleNamesEqual(&tree->children.xs[j]->name, target.xs + i)) {
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
//   load_arena - arena to use for allocations local to loading.
//   program_arena - arena to use for allocations returned from loading.
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
static FbleExpr* Parse(FbleArena* load_arena, FbleArena* program_arena, const char* root, Tree* tree, FbleNameV path, FbleModuleRefV* module_refs)
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
        break;
      }
    }

    if (!treed) {
      Tree* child = FbleAlloc(load_arena, Tree);
      child->name = path.xs[i];
      FbleVectorInit(load_arena, child->children);
      FbleVectorAppend(load_arena, tree->children, child);
      tree = child;

      char* tail = filename + strlen(filename);
      assert(*tail == '\0');

      if (i + 1 == path.size) {
        strcat(filename, ".fble");
      }
      bool public = (access(filename, F_OK) == 0);
      *tail = '\0';

      strcat(filename, "*");
      if (i + 1 == path.size) {
        strcat(filename, ".fble");
      }
      bool private = (access(filename, F_OK) == 0);
      *tail = '\0';

      if (public && private) {
        FbleReportError("module ", &path.xs[0].loc);
        FbleNameV prefix = { .xs = path.xs, .size = i + 1 };
        PrintModuleName(stderr, prefix);
        fprintf(stderr, " is marked as both public and private\n");
        return NULL;
      } else if (public) {
        child->private = false;
      } else if (private) {
        child->private = true;
      } else {
        FbleReportError("module ", &path.xs[0].loc);
        PrintModuleName(stderr, path);
        fprintf(stderr, " not found\n");
        return NULL;
      }
    }

    if (tree->private) {
      strcat(filename, "*");
    }
  }
  strcat(filename, ".fble");

  FbleExpr* expr = FbleParse(program_arena, filename, module_refs);
  if (expr == NULL) {
    FbleReportError("Failed to parse module ", &path.xs[0].loc);
    PrintModuleName(stderr, path);
    fprintf(stderr, "\n");
  }
  return expr;
}

// FbleLoad -- see documentation in fble-syntax.h
FbleProgram* FbleLoad(FbleArena* arena, const char* filename, const char* root)
{
  // The load_arena is used for allocations local to FbleLoad, so it's easier
  // to keep track of whether we are leaking any memory here.
  //
  // Note: stack->module_refs is not allocated on load_arena because FbleParse
  // needs to be able to reallocate it.
  FbleArena* load_arena = FbleNewArena();

  FbleProgram* program = FbleAlloc(arena, FbleProgram);
  FbleVectorInit(arena, program->modules);

  Stack* stack = FbleAlloc(load_arena, Stack);
  FbleVectorInit(arena, stack->module_refs);
  stack->path.size = 0;
  stack->tail = NULL;

  stack->value = FbleParse(arena, filename, &stack->module_refs);
  bool error = (stack->value == NULL);
  if (stack->value == NULL) {
    stack->module_refs.size = 0;
  }

  Tree* tree = FbleAlloc(load_arena, Tree);
  tree->name.name = "";
  tree->name.loc.source = "???";
  tree->name.loc.line = 0;
  tree->name.loc.col = 0;
  tree->private = false;
  FbleVectorInit(load_arena, tree->children);

  while (stack != NULL) {
    if (stack->module_refs.size == 0) {
      // We have loaded all the dependencies for this module.
      FbleModule* module = FbleVectorExtend(arena, program->modules);
      PathToName(arena, stack->path, &module->name);
      module->value = stack->value;
      Stack* tail = stack->tail;
      FbleFree(arena, stack->module_refs.xs);
      FbleFree(load_arena, stack);
      stack = tail;
      continue;
    }
    
    FbleModuleRef* ref = stack->module_refs.xs[stack->module_refs.size-1];

    // Check to see if we have already loaded this path.
    FbleName resolved_name;
    PathToName(load_arena, ref->path, &resolved_name);
    bool found = false;
    for (size_t i = 0; i < program->modules.size; ++i) {
      if (FbleNamesEqual(&resolved_name, &program->modules.xs[i].name)) {
        // We may have failed to load a module previously. Check to see if the
        // module has been loaded before doing the access allowed check.
        // There's no point checking access against a module that failed to
        // load, and if the module failed to load, we may not have updated the
        // Tree in the way AccessAllowed expects.
        if (program->modules.xs[i].value != NULL
            && !AccessAllowed(tree, stack->path, ref->path)) {
          FbleReportError("module ", &ref->path.xs[0].loc);
          PrintModuleName(stderr, stack->path);
          fprintf(stderr, " is not allowed to reference private module ");
          PrintModuleName(stderr, ref->path);
          fprintf(stderr, "\n");
          error = true;
        }

        ref->resolved = program->modules.xs[i].name;
        stack->module_refs.size--;
        found = true;
        break;
      }
    }
    FbleFree(load_arena, (void*)resolved_name.name);
    if (found) {
      continue;
    }

    bool recursive = false;
    for (Stack* s = stack; s != NULL; s = s->tail) {
      if (PathsEqual(ref->path, s->path)) {
        FbleName* module = ref->path.xs + ref->path.size - 1;
        FbleReportError("module ", &module->loc);
        PrintModuleName(stderr, ref->path);
        fprintf(stderr, " recursively depends on itself\n");
        error = true;
        recursive = true;
        stack->module_refs.size = 0;
        break;
      }
    }

    if (recursive) {
      continue;
    }

    // Parse the new module, placing it on the stack for processing.
    Stack* tail = stack;
    stack = FbleAlloc(load_arena, Stack);
    FbleVectorInit(arena, stack->module_refs);
    stack->path = ref->path;
    stack->tail = tail;
    stack->value = Parse(load_arena, arena, root, tree, stack->path, &stack->module_refs);
    if (stack->value == NULL) {
      error = true;
      stack->module_refs.size = 0;
    }
  }
  FreeTree(load_arena, tree);

  // The last module loaded should be the main entry point.
  program->modules.size--;
  program->main = program->modules.xs[program->modules.size].value;
  FbleAssertEmptyArena(load_arena);
  return error ? NULL : program;
}
