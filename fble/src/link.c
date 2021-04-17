
#include "fble-link.h"

#include <assert.h>     // for assert

#include "code.h"
#include "fble-interpret.h"
#include "value.h"

// FbleFreeExecutableProgram -- see documentation in fble-link.h
void FbleFreeExecutableProgram(FbleExecutableProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleExecutableModule* module = program->modules.xs + i;
      FbleFreeModulePath(module->path);
      for (size_t j = 0; j < module->deps.size; ++j) {
        FbleFreeModulePath(module->deps.xs[j]);
      }
      FbleFree(module->deps.xs);
      FbleFreeExecutable(module->executable);
    }
    FbleFree(program->modules.xs);
    FbleFree(program);
  }
}

// FbleLink -- see documentation in fble-link.h
FbleValue* FbleLink(FbleValueHeap* heap, FbleExecutableProgram* program)
{
  size_t modulec = program->modules.size;

  // Make an FbleFuncValue for each module that computes the value of the
  // module given values of the modules it depends on.
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    assert(program->modules.xs[i].executable->statics == 0);
    FbleFuncValue* func = FbleNewFuncValue(heap, program->modules.xs[i].executable);
    funcs[i] = &func->_base;
  }

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleCode* code = FbleNewCode(0, modulec, modulec);
  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleExecutableModule* module = program->modules.xs + i;

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
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[v].path)) {
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

  FbleReturnInstr* return_instr = FbleAlloc(FbleReturnInstr);
  return_instr->_base.tag = FBLE_RETURN_INSTR;
  return_instr->_base.profile_ops = NULL;
  return_instr->result.section = FBLE_LOCALS_FRAME_SECTION;
  return_instr->result.index = modulec - 1;
  FbleVectorAppend(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleFuncValue* linked = FbleNewFuncValue(heap, &code->_base);
  for (size_t i = 0; i < modulec; ++i) {
    linked->statics[i] = funcs[i];
    FbleValueAddRef(heap, &linked->_base, funcs[i]);
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(code);

  return &linked->_base;
}

// FbleLinkFromSource -- see documentation in fble-link.h
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, const char* filename, const char* root, FbleProfile* profile)
{
  FbleLoadedProgram* program = FbleLoad(filename, root);
  if (program == NULL) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleCompile(program, profile);
  FbleFreeLoadedProgram(program);
  if (compiled == NULL) {
   return NULL;
  }

  FbleExecutableProgram* executable = FbleInterpret(compiled);
  FbleFreeCompiledProgram(compiled);

  FbleValue* linked = FbleLink(heap, executable);
  FbleFreeExecutableProgram(executable);
  return linked;
}
