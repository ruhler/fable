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

// See documentation in fble-program.h.
void FbleFreeCompiledModule(FbleCompiledModule* module)
{
  FbleFreeModulePath(module->path);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleFreeModulePath(module->deps.xs[i]);
  }
  FbleFreeVector(module->deps);
  FbleFreeCode(module->code);
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleFreeName(module->profile_blocks.xs[i]);
  }
  FbleFreeVector(module->profile_blocks);

  FbleFree(module);
}

// See documentation in fble-program.h.
void FbleFreeCompiledProgram(FbleCompiledProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeCompiledModule(program->modules.xs[i]);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
}
