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

// See documentation in fble-program.h.
void FbleFreeModule(FbleModule* module)
{
  FbleFreeModulePath(module->path);
  for (size_t i = 0; i < module->type_deps.size; ++i) {
    FbleFreeModulePath(module->type_deps.xs[i]);
  }
  FbleFreeVector(module->type_deps);
  for (size_t i = 0; i < module->link_deps.size; ++i) {
    FbleFreeModulePath(module->link_deps.xs[i]);
  }
  FbleFreeVector(module->link_deps);
  FbleFreeExpr(module->type);
  FbleFreeExpr(module->value);
  FbleFreeCode(module->code);
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleFreeName(module->profile_blocks.xs[i]);
  }
  FbleFreeVector(module->profile_blocks);
  FbleFree(module);
}

// See documentation in fble-program.h.
void FbleFreeProgram(FbleProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeModule(program->modules.xs[i]);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
}
