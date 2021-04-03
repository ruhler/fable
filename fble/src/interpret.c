
#include "fble-interpret.h"

#include "code.h"
#include "execute.h"
#include "value.h"

// FbleInterpretCode -- see documentation in fble-interpret.h
FbleExecutable* FbleInterpretCode(FbleArena* arena, FbleCode* code)
{
  FbleExecutable* executable = FbleAlloc(arena, FbleExecutable);
  executable->code = code;
  executable->code->refcount++;
  executable->run = &FbleStandardRunFunction;
  return executable;
}

// FbleInterpret -- see documentation in fble-interpret.h
FbleExecutableProgram* FbleInterpret(FbleArena* arena, FbleCompiledProgram* program)
{
  FbleExecutableProgram* executable = FbleAlloc(arena, FbleExecutableProgram);
  FbleVectorInit(arena, executable->modules);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleCompiledModule* module = program->modules.xs + i;

    FbleExecutableModule* executable_module = FbleVectorExtend(arena, executable->modules);
    executable_module->path = FbleCopyModulePath(module->path);
    FbleVectorInit(arena, executable_module->deps);
    for (size_t d = 0; d < module->deps.size; ++d) {
      FbleVectorAppend(arena, executable_module->deps, FbleCopyModulePath(module->deps.xs[d]));
    }

    executable_module->executable = FbleInterpretCode(arena, module->code);
  }

  return executable;
}
