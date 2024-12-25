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

static void LoadNative(FbleProgram* program, FbleNativeModule* native);
static FbleValue* Link(FbleValueHeap* heap, FbleProfile* profile, FbleProgram* program);


/**
 * @func[LoadNative] Loads a program from native modules.
 *  Loads the module and all its dependencies in topological order, such that
 *  later modules in the list depend on earlier modules, but not the other way
 *  around.
 *
 *  @arg[FbleProgram*][program] The modules loaded so far.
 *  @arg[FbleNativeModule*][native] The module to load.
 *  @sideeffects
 *   Adds the module and all dependencies to the program.
 */
static void LoadNative(FbleProgram* program, FbleNativeModule* native)
{
  // Check if we've already loaded the module.
  for (size_t i = 0; i < program->modules.size; ++i) {
    if (FbleModulePathsEqual(program->modules.xs[i].path, native->path)) {
      return;
    }
  }

  // Load the dependencies.
  for (size_t i = 0; i < native->deps.size; ++i) {
    LoadNative(program, native->deps.xs[i]);
  }

  FbleModule* module = FbleExtendVector(program->modules);
  module->path = FbleCopyModulePath(native->path);
  FbleInitVector(module->deps);
  for (size_t i = 0; i < native->deps.size; ++i) {
    FbleAppendToVector(module->deps, FbleCopyModulePath(native->deps.xs[i]->path));
  }
  module->type = NULL;
  module->value = NULL;
  module->code = NULL;
  module->exe = native->executable;
  FbleInitVector(module->profile_blocks);
  for (size_t i = 0; i < native->profile_blocks.size; ++i) {
    FbleAppendToVector(module->profile_blocks, FbleCopyName(native->profile_blocks.xs[i]));
  }
}

/**
 * @func[Link] Links together modules from a program into an FbleValue*.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *  @arg[FbleProgram*] program
 *   The program of modules to link together.
 *
 *  @returns FbleValue*
 *   A zero-argument fble function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* Link(FbleValueHeap* heap, FbleProfile* profile, FbleProgram* program)
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
    FbleModule* module = program->modules.xs + i;

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
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[v].path)) {
          FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
          FbleAppendToVector(call->args, var);
          break;
        }
      }
    }
    assert(call->args.size == module->deps.size);

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

// FbleLink -- see documentation in fble-link.h
FbleValue* FbleLink(FbleValueHeap* heap, FbleProfile* profile, FbleNativeModuleV native_search_path, FbleSearchPath* search_path, FbleModulePath* module_path, FbleStringV* build_deps)
{
  FbleProgram* program = NULL;

  for (size_t i = 0; i < native_search_path.size; ++i) {
    if (FbleModulePathsEqual(native_search_path.xs[i]->path, module_path)) {
      program = FbleAlloc(FbleProgram);
      FbleInitVector(program->modules);
      LoadNative(program, native_search_path.xs[i]);
      break;
    }
  }

  if (program == NULL) {
    program = FbleLoadForExecution(search_path, module_path, build_deps);
    if (program == NULL) {
      return NULL;
    }

    if (!FbleCompileProgram(program)) {
      FbleFreeProgram(program);
      return NULL;
    }
  }

  FbleValue* linked = Link(heap, profile, program);
  FbleFreeProgram(program);
  return linked;
}
// See documentation in fble-link.h
void FblePrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FbleNativeModule* module)
{
  if (module != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    fprintf(stream, "%s: %s -m ", binary_name, tool);
    FblePrintModulePath(stream, module->path);
    fprintf(stream, " (compiled)\n");
  }
}
