
#include "fble-link.h"

#include <assert.h>     // for assert

#include "code.h"
#include "fble-interpret.h"
#include "value.h"

// FbleFreeExecutableProgram -- see documentation in fble-link.h
void FbleFreeExecutableProgram(FbleArena* arena, FbleExecutableProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleExecutableModule* module = program->modules.xs + i;
      FbleFreeModulePath(arena, module->path);
      for (size_t j = 0; j < module->deps.size; ++j) {
        FbleFreeModulePath(arena, module->deps.xs[j]);
      }
      FbleFree(arena, module->deps.xs);
      FbleFreeCode(arena, module->executable->code);
      FbleFree(arena, module->executable);
    }
    FbleFree(arena, program->modules.xs);
    FbleFree(arena, program);
  }
}

// FbleLink -- see documentation in fble-link.h
FbleValue* FbleLink(FbleValueHeap* heap, FbleExecutableProgram* program)
{
  FbleArena* arena = heap->arena;
  size_t modulec = program->modules.size;

  // Make an FbleFuncValue for each module that computes the value of the
  // module given values of the modules it depends on.
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    FbleFuncValue* func = FbleNewValue(heap, FbleFuncValue);
    func->_base.tag = FBLE_FUNC_VALUE;
    func->executable = FbleAlloc(arena, FbleExecutable);
    func->executable->code = program->modules.xs[i].executable->code;
    func->executable->code->refcount++;
    func->executable->run = program->modules.xs[i].executable->run;
    func->argc = program->modules.xs[i].deps.size;
    func->localc = program->modules.xs[i].executable->code->locals;
    func->staticc = program->modules.xs[i].executable->code->statics;
    funcs[i] = &func->_base;
  }

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleCode* code = FbleAlloc(arena, FbleCode);
  code->refcount = 1;
  code->magic = FBLE_CODE_MAGIC;
  code->statics = modulec;
  code->locals = modulec;
  FbleVectorInit(arena, code->instrs);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleExecutableModule* module = program->modules.xs + i;

    FbleCallInstr* call = FbleAlloc(arena, FbleCallInstr);
    call->_base.tag = FBLE_CALL_INSTR;
    call->_base.profile_ops = NULL;
    call->loc.source = FbleNewString(arena, __FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->exit = false;
    call->func.section = FBLE_STATICS_FRAME_SECTION;
    call->func.index = i;
    FbleVectorInit(arena, call->args);
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[v].path)) {
          FbleFrameIndex index = {
            .section = FBLE_LOCALS_FRAME_SECTION,
            .index = v
          };
          FbleVectorAppend(arena, call->args, index);
          break;
        }
      }
    }
    assert(call->args.size == module->deps.size);

    call->dest = i;
    FbleVectorAppend(arena, code->instrs, &call->_base);
  }

  FbleReturnInstr* return_instr = FbleAlloc(arena, FbleReturnInstr);
  return_instr->_base.tag = FBLE_RETURN_INSTR;
  return_instr->_base.profile_ops = NULL;
  return_instr->result.section = FBLE_LOCALS_FRAME_SECTION;
  return_instr->result.index = modulec - 1;
  FbleVectorAppend(arena, code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleFuncValue* linked = FbleNewInterpretedFuncValue(heap, 0, code);
  for (size_t i = 0; i < modulec; ++i) {
    linked->statics[i] = funcs[i];
    FbleValueAddRef(heap, &linked->_base, funcs[i]);
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(arena, code);

  return &linked->_base;
}

// FbleLinkFromSource -- see documentation in fble-link.h
FbleValue* FbleLinkFromSource(FbleValueHeap* heap, const char* filename, const char* root, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;

  FbleLoadedProgram* program = FbleLoad(arena, filename, root);
  if (program == NULL) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleCompile(arena, program, profile);
  FbleFreeLoadedProgram(arena, program);
  if (compiled == NULL) {
   return NULL;
  }

  FbleExecutableProgram* executable = FbleInterpret(arena, compiled);
  FbleFreeCompiledProgram(arena, compiled);

  FbleValue* linked = FbleLink(heap, executable);
  FbleFreeExecutableProgram(arena, executable);
  return linked;
}
