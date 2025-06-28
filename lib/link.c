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

static void LinkModule(FbleValueHeap* heap, FbleProfile* profile,
    FbleModuleV* linked, FbleValueV* funcs, FbleCode* code, FbleModule* module);

/**
 * @func[LinkModule] Links a module into the given code.
 *  @arg[FbleValueHeap*][heap] Heap to use to allocate value.s.
 *  @arg[FbleProfile*][profile] Profile to add block info to.
 *  @arg[FbleModuleV*][linked] List of linked modules.
 *  @arg[FbleValueV*][funcs] Values of linked modules.
 *  @arg[FbleCode*][code] The linking code.
 *  @arg[FbleModule*][module] The module to link into the given code.
 *  @sideeffects
 *   @i Adds the module to @a[linked].
 *   @i Adds the value of the module to @a[funcs].
 *   @i Adds instructions to call the module function to @a[code].
 */
static void LinkModule(FbleValueHeap* heap, FbleProfile* profile,
    FbleModuleV* linked, FbleValueV* funcs, FbleCode* code, FbleModule* module)
{
  size_t profile_block_id = FbleAddBlocksToProfile(profile, module->profile_blocks);

  FbleValue* func = NULL;
  if (module->code != NULL) {
    assert(module->exe == NULL);
    func = FbleNewInterpretedFuncValue(heap, module->code, profile_block_id, NULL);
  } else if (module->exe != NULL) {
    assert(module->code == NULL);
    func = FbleNewFuncValue(heap, module->exe, profile_block_id, NULL);
  } else {
    assert(false && "Attempt to link uncompiled module");
  }

  FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
  call->loc.source = FbleNewString(__FILE__);
  call->loc.line = __LINE__ - 1;
  call->loc.col = 5;
  call->func.tag = FBLE_STATIC_VAR;
  call->func.index = funcs->size;
  FbleInitVector(call->args);
  for (size_t d = 0; d < module->link_deps.size; ++d) {
    for (size_t v = 0; v < linked->size; ++v) {
      if (module->link_deps.xs[d] == linked->xs[v]) {
        FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
        FbleAppendToVector(call->args, var);
        break;
      }
    }
  }
  assert(call->args.size == module->link_deps.size);
  call->dest = funcs->size;

  FbleAppendToVector(*linked, module);
  FbleAppendToVector(*funcs, func);
  FbleAppendToVector(code->instrs, &call->_base);
}

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

  FbleCode* code = FbleNewCode(0, 0, 0, main_id);
  FbleValueV funcs;
  FbleInitVector(funcs);
  FbleModuleV linked_modules;
  FbleInitVector(linked_modules);
  for (size_t i = 0; i < program->modules.size; ++i) {
    LinkModule(heap, profile, &linked_modules, &funcs, code, program->modules.xs[i]);
  }
  code->executable.num_statics = linked_modules.size;
  code->num_locals = linked_modules.size;

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = linked_modules.size - 1;
  FbleAppendToVector(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewInterpretedFuncValue(heap, code, 0, funcs.xs);
  FbleFreeVector(funcs);
  FbleFreeVector(linked_modules);
  FbleFreeCode(code);
  return linked;
}
