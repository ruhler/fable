/**
 * @file link.c
 * Routines for linking fble modules together into FbleValue*.
 */

#include <fble/fble-link.h>

#include <assert.h>     // for assert

#include <fble/fble-alloc.h>     // for FbleAlloc, etc.
#include <fble/fble-interpret.h> // for FbleInterpret
#include <fble/fble-vector.h>    // for FbleVectorInit, etc.

#include "code.h"
#include "value.h"

// FbleLink -- see documentation in fble-link.h
FbleValue* FbleLink(FbleValueHeap* heap, FbleExecutableProgram* program, FbleProfile* profile)
{
  size_t modulec = program->modules.size;

  FbleBlockId main_id = 0;
  if (profile != NULL) {
    FbleName main_block = {
      .name = FbleNewString("<main>"),
      .loc = FbleNewLoc(__FILE__, __LINE__-2, 12)
    };
    main_id = FbleProfileAddBlock(profile, main_block);
  }

  // Make an FbleFuncValue for each module that computes the value of the
  // module given values of the modules it depends on.
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    FbleExecutableModule* module = program->modules.xs[i];
    FbleExecutable* exe = module->executable;

    // TODO: Return error messages in these cases instead of assert failures?
    assert(exe->num_statics == 0 && "Module cannot have statics");
    assert(module->deps.size == exe->num_args && "Module args mismatch");

    size_t profile_block_offset = 0;
    if (profile != NULL) {
      profile_block_offset = FbleProfileAddBlocks(profile, module->profile_blocks);
    }

    funcs[i] = FbleNewFuncValue(heap, exe, profile_block_offset, NULL);
  }

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleCode* code = FbleNewCode(0, modulec, modulec, main_id);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleExecutableModule* module = program->modules.xs[i];

    FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
    call->loc.source = FbleNewString(__FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->exit = false;
    call->func.tag = FBLE_STATIC_VAR;
    call->func.index = i;
    FbleVectorInit(call->args);
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[v]->path)) {
          FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
          FbleVectorAppend(call->args, var);
          break;
        }
      }
    }
    assert(call->args.size == module->deps.size);

    call->dest = i;
    FbleVectorAppend(code->instrs, &call->_base);
  }

  FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
  FbleVectorInit(release_instr->targets);
  for (size_t i = 0; i + 1 < program->modules.size; ++i) {
    FbleVectorAppend(release_instr->targets, i);
  }
  FbleVectorAppend(code->instrs, &release_instr->_base);

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = modulec - 1;
  FbleVectorAppend(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewFuncValue(heap, &code->_base, 0, funcs);
  for (size_t i = 0; i < modulec; ++i) {
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(code);

  return linked;
}

// FbleLinkFromSource -- see documentation in fble-link.h
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, FbleSearchPath* search_path, FbleModulePath* module_path, FbleProfile* profile)
{
  FbleLoadedProgram* program = FbleLoad(search_path, module_path, NULL);
  if (program == NULL) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleCompileProgram(program);
  FbleFreeLoadedProgram(program);
  if (compiled == NULL) {
   return NULL;
  }

  FbleExecutableProgram* executable = FbleInterpret(compiled);
  FbleFreeCompiledProgram(compiled);

  FbleValue* linked = FbleLink(heap, executable, profile);
  FbleFreeExecutableProgram(executable);
  return linked;
}

// FbleLoadFromCompiled -- see documentation in fble-link.h
void FbleLoadFromCompiled(FbleExecutableProgram* program, FbleExecutableModule* module, size_t depc, FbleCompiledModuleFunction** deps)
{
  // Don't do anything if the module has already been loaded.
  for (size_t i = 0; i < program->modules.size; ++i) {
    if (FbleModulePathsEqual(module->path, program->modules.xs[i]->path)) {
      return;
    }
  }

  // Otherwise, load it's dependencies and add it to the list.
  for (size_t i = 0; i < depc; ++i) {
    deps[i](program);
  }

  module->refcount++;
  FbleVectorAppend(program->modules, module);
}

// FbleLinkFromCompiled -- see documentation in fble-link.h
FbleValue* FbleLinkFromCompiled(FbleCompiledModuleFunction* module, FbleValueHeap* heap, FbleProfile* profile)
{
  FbleExecutableProgram* program = FbleAlloc(FbleExecutableProgram);
  FbleVectorInit(program->modules);
  module(program);
  FbleValue* value = FbleLink(heap, program, profile);
  FbleFreeExecutableProgram(program);
  return value;
}

// FbleLinkFromCompiledOrSource -- see documentation in fble-link.h
FbleValue* FbleLinkFromCompiledOrSource(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* module, FbleSearchPath* search_path, const char* module_path)
{
  if (module != NULL) {
    return FbleLinkFromCompiled(module, heap, profile);
  }

  assert(module_path != NULL);
  FbleModulePath* mpath = FbleParseModulePath(module_path);
  if (mpath == NULL) {
    return NULL;
  }

  FbleValue* linked = FbleLinkFromSource(heap, search_path, mpath, profile);
  FbleFreeModulePath(mpath);
  return linked;
}
