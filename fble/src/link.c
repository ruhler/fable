
#include "fble-link.h"

#include <assert.h>     // for assert

#include "fble-alloc.h"     // for FbleAlloc, etc.
#include "fble-interpret.h" // for FbleInterpret
#include "fble-vector.h"    // for FbleVectorInit, etc.

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
    FbleExecutable* module = program->modules.xs[i]->executable;

    assert(module->statics == 0);

    size_t profile_base_id = 0;
    if (profile != NULL) {
      profile_base_id = FbleProfileAddBlocks(profile, module->profile_blocks);
    }

    FbleFuncValue* func = FbleNewFuncValue(heap, module, profile_base_id);
    funcs[i] = &func->_base;
  }

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleCode* code = FbleNewCode(0, modulec, modulec, main_id);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleExecutableModule* module = program->modules.xs[i];

    FbleCallInstr* call = FbleAlloc(FbleCallInstr);
    call->_base.tag = FBLE_CALL_INSTR;
    call->_base.profile_ops = NULL;
    call->loc.source = FbleNewString(__FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->exit = false;
    call->func.section = FBLE_STATICS_FRAME_SECTION;
    call->func.index = i;
    FbleVectorInit(call->args);
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[v]->path)) {
          FbleFrameIndex index = {
            .section = FBLE_LOCALS_FRAME_SECTION,
            .index = v
          };
          FbleVectorAppend(call->args, index);
          break;
        }
      }
    }
    assert(call->args.size == module->deps.size);

    call->dest = i;
    FbleVectorAppend(code->instrs, &call->_base);
  }

  for (size_t i = 0; i + 1 < program->modules.size; ++i) {
    FbleReleaseInstr* release_instr = FbleAlloc(FbleReleaseInstr);
    release_instr->_base.tag = FBLE_RELEASE_INSTR;
    release_instr->_base.profile_ops = NULL;
    release_instr->target = i;
    FbleVectorAppend(code->instrs, &release_instr->_base);
  }

  FbleReturnInstr* return_instr = FbleAlloc(FbleReturnInstr);
  return_instr->_base.tag = FBLE_RETURN_INSTR;
  return_instr->_base.profile_ops = NULL;
  return_instr->result.section = FBLE_LOCALS_FRAME_SECTION;
  return_instr->result.index = modulec - 1;
  FbleVectorAppend(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleFuncValue* linked = FbleNewFuncValue(heap, &code->_base, 0);
  for (size_t i = 0; i < modulec; ++i) {
    linked->statics[i] = funcs[i];
    FbleValueAddRef(heap, &linked->_base, funcs[i]);
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(code);

  return &linked->_base;
}

// FbleLinkFromSource -- see documentation in fble-link.h
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, FbleSearchPath search_path, FbleModulePath* module_path, FbleProfile* profile)
{
  FbleLoadedProgram* program = FbleLoad(search_path, module_path);
  if (program == NULL) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleCompile(program);
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
