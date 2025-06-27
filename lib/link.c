/**
 * @file link.c
 *  Routines for linking fble modules together into FbleValue*.
 */

#include <fble/fble-link.h>

#include <assert.h>     // for assert
#include <string.h>     // for strrchr

#include <fble/fble-alloc.h>   // for FbleAlloc, etc.
#include <fble/fble-vector.h>  // for FbleInitVector, etc.

#include "code.h"       // for FbleCode
#include "interpret.h"  // for FbleNewInterpretedFuncValue

// See documentation in fble-link.h
FbleValue* FbleLink(FbleValueHeap* heap, FbleProfile* profile, FbleProgram* program)
{
  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleName main_block = {
    .name = FbleNewString("<main>"),
    .loc = FbleNewLoc(__FILE__, __LINE__-2, 12)
  };
  FbleBlockId main_id = FbleAddBlockToProfile(profile, main_block);

  size_t modulec = program->modules.size;
  FbleCode* code = FbleNewCode(0, modulec, modulec, main_id);
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    FbleModule* module = program->modules.xs[i];

    size_t profile_block_id = FbleAddBlocksToProfile(profile, module->profile_blocks);

    assert(module->code != NULL || module->exe != NULL);
    if (module->code != NULL) {
      assert(module->exe == NULL);
      funcs[i] = FbleNewInterpretedFuncValue(heap, module->code, profile_block_id, NULL);
    }
    
    if (module->exe != NULL) {
      assert(module->code == NULL);
      funcs[i] = FbleNewFuncValue(heap, module->exe, profile_block_id, NULL);
    }

    FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
    call->loc.source = FbleNewString(__FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->func.tag = FBLE_STATIC_VAR;
    call->func.index = i;
    FbleInitVector(call->args);
    for (size_t d = 0; d < module->link_deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->link_deps.xs[d], program->modules.xs[v]->path)) {
          FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
          FbleAppendToVector(call->args, var);
          break;
        }
      }
    }
    assert(call->args.size == module->link_deps.size);

    call->dest = i;
    FbleAppendToVector(code->instrs, &call->_base);
  }

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = modulec - 1;
  FbleAppendToVector(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewInterpretedFuncValue(heap, code, 0, funcs);
  FbleFreeCode(code);
  return linked;
}
