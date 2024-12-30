/**
 * @file program.c
 *  Routines for dealing with programs.
 */

#include <fble/fble-program.h>

#include <fble/fble-alloc.h>
#include <fble/fble-name.h>
#include <fble/fble-vector.h>

#include "code.h"
#include "expr.h"

static void Preload(FbleProgram* program, FblePreloadedModule* preloaded);

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

// See documentation in fble-program.h.
void FbleFreeModule(FbleModule* module)
{
  FbleFreeModulePath(module->path);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleFreeModulePath(module->deps.xs[i]);
  }
  FbleFreeVector(module->deps);
  FbleFreeExpr(module->type);
  FbleFreeExpr(module->value);
  FbleFreeCode(module->code);
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleFreeName(module->profile_blocks.xs[i]);
  }
  FbleFreeVector(module->profile_blocks);
}

// See documentation in fble-program.h.
void FbleFreeProgram(FbleProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeModule(program->modules.xs + i);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
}

// See documentation in fble-program.h
FbleProgram* FbleNewPreloadedProgram(FblePreloadedModule* main)
{
  FbleProgram* program = FbleAlloc(FbleProgram);
  FbleInitVector(program->modules);
  Preload(program, main);
  return program;
}
