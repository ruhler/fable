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
#include "program.h"    // for FbleModuleMap

static size_t LinkedModule(FbleValueHeap* heap, FbleProfile* profile,
    FbleModuleMap* linked, FbleValueV* funcs, FbleCode* code, FbleModule* module);

/**
 * @func[LinkedModule] Get or link a module into the given code.
 *  @arg[FbleValueHeap*][heap] Heap to use to allocate value.s.
 *  @arg[FbleProfile*][profile] Profile to add block info to.
 *  @arg[FbleModuleMap*][linked] Map from already linked module to size_t index.
 *  @arg[FbleValueV*][funcs] Values of linked modules.
 *  @arg[FbleCode*][code] The linking code.
 *  @arg[FbleModule*][module] The module to link into the given code.
 *  @returns[size_t] The index of the linked module.
 *  @sideeffects
 *   @i Adds modules to @a[linked].
 *   @i Adds values of modules to @a[funcs].
 *   @i Adds instructions to call the module function to @a[code].
 */
static size_t LinkedModule(FbleValueHeap* heap, FbleProfile* profile,
    FbleModuleMap* linked, FbleValueV* funcs, FbleCode* code, FbleModule* module)
{
  // Avoid linking a module that has already been linked into the code.
  intptr_t already_linked = 0;
  if (FbleModuleMapLookup(linked, module, (void**)&already_linked)) {
    return (size_t)already_linked;
  }

  FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
  call->loc.source = FbleNewString(__FILE__);
  call->loc.line = __LINE__ - 1;
  call->loc.col = 5;
  call->func.tag = FBLE_STATIC_VAR;
  FbleInitVector(call->args);
  for (size_t i = 0; i < module->link_deps.size; ++i) {
    size_t v = LinkedModule(heap, profile, linked, funcs, code, module->link_deps.xs[i]);
    FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
    FbleAppendToVector(call->args, var);
  }

  size_t index = funcs->size;
  call->func.index = index;
  call->dest = index;

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

  FbleModuleMapInsert(linked, module, (void*)(intptr_t)index);
  FbleAppendToVector(*funcs, func);
  FbleAppendToVector(code->instrs, &call->_base);
  return index;
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
  FbleModuleMap* map = FbleNewModuleMap();
  FbleModule* main_module = program->modules.xs[program->modules.size - 1];
  size_t main_index = LinkedModule(heap, profile, map, &funcs, code, main_module);

  code->executable.num_statics = funcs.size;
  code->num_locals = funcs.size;

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = main_index;
  FbleAppendToVector(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewInterpretedFuncValue(heap, code, 0, funcs.xs);
  FbleFreeVector(funcs);
  FbleFreeModuleMap(map, NULL, NULL);
  FbleFreeCode(code);
  return linked;
}
